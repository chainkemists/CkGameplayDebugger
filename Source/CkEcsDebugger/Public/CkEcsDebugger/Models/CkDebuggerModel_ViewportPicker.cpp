#include "CkDebuggerModel_ViewportPicker.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"

#include "CkIskmRenderer/Proxy/CkIskmProxy_Utils.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerViewportPicker_InputProcessor.h"

// Successful pick re-focuses the debugger tab (the game viewport took focus while picking).
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

// On-Screen Overlay reuse — same focus card + world tags, built via the shared presenters.
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    static constexpr auto PickRadius             = 30.0f;        // cm
    static constexpr auto MaxAttachmentWalkDepth = 16;
    static constexpr auto CullRadiusMin          = 100.0f;       // 1 m
    static constexpr auto CullRadiusMax          = 100'000.0f;   // 1 km

    // Billboard presentation (screen-space pixels). Constant size — they do NOT shrink with distance.
    static constexpr auto BillboardSizeMinPx      = 8.0f;
    static constexpr auto BillboardSizeMaxPx      = 128.0f;
    static constexpr auto BillboardHoverScale     = 1.25f;
    static constexpr auto BillboardDefaultAlpha   = 0.45f;

    // Ejected-PIE discrimination + deprojection live in ck::DebugViewportView
    // (CkDebuggerCommon) — shared with the on-screen overlay and the
    // focus-entity helper.

    // Slab-method ray/AABB: near-hit T along InDirection (0 when the origin is
    // inside the box), unset on miss. FMath::LineBoxIntersection returns bool
    // only — ranking candidates needs the T.
    auto Intersect_RayBoxNearT(
        const FBox&    InBox,
        const FVector& InOrigin,
        const FVector& InDirection) -> TOptional<float>
    {
        auto TMin = 0.0;
        auto TMax = TNumericLimits<double>::Max();

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            const auto D = InDirection[Axis];
            if (FMath::IsNearlyZero(D))
            {
                if (InOrigin[Axis] < InBox.Min[Axis] || InOrigin[Axis] > InBox.Max[Axis])
                { return {}; }
                continue;
            }

            const auto Inv = 1.0 / D;
            auto T1 = (InBox.Min[Axis] - InOrigin[Axis]) * Inv;
            auto T2 = (InBox.Max[Axis] - InOrigin[Axis]) * Inv;
            if (T1 > T2)
            { Swap(T1, T2); }

            TMin = FMath::Max(TMin, T1);
            TMax = FMath::Min(TMax, T2);
            if (TMin > TMax)
            { return {}; }
        }

        return static_cast<float>(TMin);
    }
}

// =====================================================================================================================

FCkDebuggerModel_ViewportPicker::FCkDebuggerModel_ViewportPicker() = default;

FCkDebuggerModel_ViewportPicker::~FCkDebuggerModel_ViewportPicker()
{
    Deactivate();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    Construct(
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelection,
        TSharedPtr<FCkDebuggerModel_WorldContext>    InWorld) -> void
{
    _SelectionModel = InSelection;
    _WorldModel     = InWorld;

    _MeshesFirst = UCkEcsDebuggerSettings::Get()->PickerMeshesFirst;
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    Activate() -> bool
{
    if (_IsActive)
    { return true; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    auto* GVC = World->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return false; }

    DoCaptureMouseState(World);

    _InputProcessor = MakeShared<FCkDebuggerViewportPicker_InputProcessor>(AsShared());

    constexpr auto InputPriority = 0;
    FSlateApplication::Get().RegisterInputPreProcessor(_InputProcessor, InputPriority);

    if (const auto WorldModel = _WorldModel.Pin())
    {
        _WorldChangedHandle = WorldModel->OnWorldChanged.AddLambda(
            [WeakSelf = TWeakPtr<FCkDebuggerModel_ViewportPicker>(AsShared())](UWorld*)
            {
                if (const auto StrongSelf = WeakSelf.Pin())
                {
                    StrongSelf->Deactivate();
                }
            });
    }

    // Front-load the shared marker textures (load + async compilation) so the first
    // DrawMarkers call never sees a placeholder resource — that was the intermittent
    // crash on cold editor starts. Then build the initial snapshot so markers appear
    // on this frame's draw rather than after the first Tick.
    _Markers.EnsureTextures();
    DoRefreshMarkers(World);

    // Register for BOTH "Game" (PIE viewports) and "Editor" (level editor viewport) show flags so
    // billboards keep rendering after the user ejects with F8 and input moves to the editor viewport.
    const auto DrawDelegate = FDebugDrawDelegate::CreateSP(
        AsShared(), &FCkDebuggerModel_ViewportPicker::DoDrawBillboards);
    _DebugDrawHandle_Game   = UDebugDrawService::Register(TEXT("Game"),   DrawDelegate);
    _DebugDrawHandle_Editor = UDebugDrawService::Register(TEXT("Editor"), DrawDelegate);

    DoActivateOverlayCards(World);

    _IsActive = true;
    OnPickModeChanged.Broadcast(_IsActive);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    Deactivate() -> void
{
    if (NOT _IsActive)
    { return; }

    if (_InputProcessor.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(_InputProcessor);
    }
    _InputProcessor.Reset();

    if (_DebugDrawHandle_Game.IsValid())
    {
        UDebugDrawService::Unregister(_DebugDrawHandle_Game);
        _DebugDrawHandle_Game.Reset();
    }
    if (_DebugDrawHandle_Editor.IsValid())
    {
        UDebugDrawService::Unregister(_DebugDrawHandle_Editor);
        _DebugDrawHandle_Editor.Reset();
    }

    DoRestoreMouseState();

    if (_WorldChangedHandle.IsValid())
    {
        if (const auto WorldModel = _WorldModel.Pin())
        {
            WorldModel->OnWorldChanged.Remove(_WorldChangedHandle);
        }
        _WorldChangedHandle.Reset();
    }

    DoDeactivateOverlayCards();

    _Markers.Reset();
    _FocusEntity = FCk_Handle{};
    _HasRay      = false;
    _IsActive    = false;

    OnPickModeChanged.Broadcast(_IsActive);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    Toggle() -> void
{
    if (_IsActive)
    {
        Deactivate();
    }
    else
    {
        Activate();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    IsActive() const -> bool
{
    return _IsActive;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    CanActivate() const -> bool
{
    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    return ck::IsValid(World->GetGameViewport());
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    Get_IgnoreLocalPawn() const -> bool
{
    return _IgnoreLocalPawn;
}

auto
    FCkDebuggerModel_ViewportPicker::
    Set_IgnoreLocalPawn(
        bool InValue) -> void
{
    _IgnoreLocalPawn = InValue;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    Get_CullRadius() const -> float
{
    return _CullRadius;
}

auto
    FCkDebuggerModel_ViewportPicker::
    Set_CullRadius(
        float InValue) -> void
{
    _CullRadius = FMath::Clamp(InValue, CullRadiusMin, CullRadiusMax);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    Get_BillboardSize() const -> float
{
    return _BillboardSizePx;
}

auto
    FCkDebuggerModel_ViewportPicker::
    Set_BillboardSize(
        float InValue) -> void
{
    _BillboardSizePx = FMath::Clamp(InValue, BillboardSizeMinPx, BillboardSizeMaxPx);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    Get_MeshesFirst() const -> bool
{
    return _MeshesFirst;
}

auto
    FCkDebuggerModel_ViewportPicker::
    Set_MeshesFirst(
        bool InValue) -> void
{
    if (_MeshesFirst == InValue)
    { return; }

    _MeshesFirst = InValue;

    if (auto* Settings = GetMutableDefault<UCkEcsDebuggerSettings>())
    {
        Settings->PickerMeshesFirst = InValue;
        Settings->SaveConfig();
    }
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    Tick(
        float InDeltaSeconds) -> void
{
    if (NOT _IsActive)
    { return; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    {
        Deactivate();
        return;
    }

    // Rebuild the marker snapshot (entities move/spawn/die between frames) and re-issue
    // the one-frame dashed parent→child links. Billboard rendering is driven by the
    // UDebugDrawService callback (DoDrawBillboards) off this snapshot.
    DoRefreshMarkers(World);
    _Markers.DrawLinks(World);

    // Refresh the focus under the last known cursor ray. STICKY: only update when the ray
    // actually hits an entity, so _FocusEntity (card + emphasized diamond) holds the last
    // hover instead of flickering empty as the cursor sweeps between markers.
    if (_HasRay)
    {
        const auto Hit = DoPickAtRay(World, _LastRayOrigin, _LastRayDirection);
        if (ck::IsValid(Hit))
        {
            _FocusEntity = Hit;
        }
    }

    // Hovered geometry-backed entity: outline its resolved bounds (the "you are
    // about to pick THIS mesh" affordance). One-frame box re-issued from this
    // UNGATED tick — immediate-mode is right for transient hover; persistent
    // selection uses the retained PMG gizmo instead. Meshless entities keep the
    // emphasized diamond as their only hover emphasis.
    if (ck::IsValid(_FocusEntity) && DoIsMeshResolvable(_FocusEntity))
    {
        if (const auto Bounds = ck::DebugFocus::Get_EntityWorldBounds(_FocusEntity))
        {
            DrawDebugBox(World, Bounds->Origin, Bounds->BoxExtent,
                FColor{255, 200, 60}, /*bPersistentLines=*/false, /*LifeTime=*/0.0f,
                /*DepthPriority=*/0, /*Thickness=*/1.5f);
        }
    }

    // Push the overlay focus card + world tags for the current focus / candidates.
    if (_OverlayCardsActive)
    {
        auto* PC = World->GetFirstPlayerController();

        // Ejected PIE: world tags use frozen-camera PC projection — DoUpdateOverlayCards
        // skips them then (the focus card still shows). Mirror the picker's own ejected
        // detection used elsewhere.
        const auto IsEjected = ck::DebugViewportView::Get_IsEjected();
        DoUpdateOverlayCards(World, PC, IsEjected);
    }
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    OnMouseMoved(
        const FVector2D& InAbsolutePos) -> void
{
    if (NOT _IsActive)
    { return; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return; }

    auto Origin    = FVector::ZeroVector;
    auto Direction = FVector::ForwardVector;
    if (NOT DoDeproject(World, InAbsolutePos, Origin, Direction))
    {
        // Cursor left the viewport: stop ray-testing, but keep _FocusEntity sticky so the
        // card doesn't vanish while the mouse is briefly off the render surface.
        _HasRay = false;
        return;
    }

    _LastRayOrigin    = Origin;
    _LastRayDirection = Direction;
    _HasRay           = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    OnMouseClicked(
        const FVector2D& InAbsolutePos) -> bool
{
    if (NOT _IsActive)
    { return false; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    auto Origin    = FVector::ZeroVector;
    auto Direction = FVector::ForwardVector;
    if (NOT DoDeproject(World, InAbsolutePos, Origin, Direction))
    { return false; }

    const auto Hit = DoPickAtRay(World, Origin, Direction);
    if (ck::Is_NOT_Valid(Hit))
    { return false; }

    if (const auto SelectionModel = _SelectionModel.Pin())
    {
        const auto NewSelection = TArray<FCk_Handle>{Hit};
        SelectionModel->Set_SelectedEntities(NewSelection);
    }

    // The entity tree applies model changes as ESelectInfo::Direct and deliberately never
    // re-broadcasts those, so a world-picked entity would stay invisible to Crowd/GOAP.
    // Broadcast here — this IS the user-driven selection.
    ck::DebugSelectionSync::Broadcast(Hit, TEXT("EcsDebugger"));

    Deactivate();

    // Bring the debugger tab back to front — the game viewport took focus while
    // picking, so a successful pick would otherwise land in a background tab.
    ck::DebugNav::Goto_Entity(Hit);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    OnEscapePressed() -> bool
{
    if (NOT _IsActive)
    { return false; }

    Deactivate();
    return true;
}

// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    DoResolveTargetWorld() const -> UWorld*
{
    const auto WorldModel = _WorldModel.Pin();
    if (NOT WorldModel.IsValid())
    { return nullptr; }

    auto* World = WorldModel->Get_SelectedWorld();
    if (ck::Is_NOT_Valid(World))
    { return nullptr; }

    const auto IsPlayableWorld =
        World->WorldType == EWorldType::PIE ||
        World->WorldType == EWorldType::Game;

    if (NOT IsPlayableWorld)
    { return nullptr; }

    return World;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoCaptureMouseState(
        UWorld* InWorld) -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto* GVC = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return; }

    auto* PC = InWorld->GetFirstPlayerController();

    auto Restore               = FRestoreMouseState{};
    Restore.Viewport           = GVC;
    Restore.Controller         = PC;
    Restore.PriorCapture       = GVC->GetMouseCaptureMode();
    Restore.PriorLock          = GVC->GetMouseLockMode();
    Restore.PriorCursorVisible = ck::IsValid(PC) ? PC->bShowMouseCursor : false;

    _MouseStateToRestore = Restore;

    // Release the cursor so the picker can receive mouse events in first-person PIE.
    GVC->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
    GVC->SetMouseLockMode(EMouseLockMode::DoNotLock);
    GVC->SetIgnoreInput(false);

    if (ck::IsValid(PC))
    {
        PC->SetShowMouseCursor(true);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoRestoreMouseState() -> void
{
    if (NOT _MouseStateToRestore.IsSet())
    { return; }

    const auto& Restore = _MouseStateToRestore.GetValue();

    if (auto* GVC = Restore.Viewport.Get(); ck::IsValid(GVC))
    {
        GVC->SetMouseCaptureMode(Restore.PriorCapture);
        GVC->SetMouseLockMode(Restore.PriorLock);
    }

    if (auto* PC = Restore.Controller.Get(); ck::IsValid(PC))
    {
        PC->SetShowMouseCursor(Restore.PriorCursorVisible);
    }

    _MouseStateToRestore.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoDeproject(
        UWorld*          InWorld,
        const FVector2D& InAbsolutePos,
        FVector&         OutOrigin,
        FVector&         OutDirection) const -> bool
{
    return ck::DebugViewportView::Deproject(InWorld, InAbsolutePos, OutOrigin, OutDirection);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoResolveActorEntity(
        UWorld* InWorld,
        AActor* InHitActor) const -> FCk_Handle
{
    if (ck::Is_NOT_Valid(InHitActor))
    { return FCk_Handle{}; }

    auto* CurrentActor = InHitActor;
    for (auto Depth = 0; Depth < MaxAttachmentWalkDepth; ++Depth)
    {
        if (ck::Is_NOT_Valid(CurrentActor))
        { break; }

        if (CurrentActor->IsPendingKillPending())
        { break; }

        if (CurrentActor->GetWorld() != InWorld)
        { break; }

        const auto Handle = UCk_Utils_OwningActor_UE::TryGet_ActorEntityHandle(CurrentActor);
        if (ck::IsValid(Handle))
        { return Handle; }

        CurrentActor = CurrentActor->GetAttachParentActor();
    }

    return FCk_Handle{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoPickAtRay(
        UWorld*        InWorld,
        const FVector& InOrigin,
        const FVector& InDirection) const -> FCk_Handle
{
    if (ck::Is_NOT_Valid(InWorld))
    { return FCk_Handle{}; }

    const auto IgnoredActors = DoGet_LocalIgnoredActors(InWorld);

    auto ActorCandidate  = FPickCandidate{};
    auto SphereCandidate = FPickCandidate{};

    // ---- 1. Physics line trace for geometry-backed entities ----
    // The resolved entity must be in the marker snapshot — the depth gate, distance
    // cull, and ignore-self filter apply to trace hits too (what you see is what you
    // can pick). An ISM-instance hit resolves to its proxy entity FIRST (more
    // specific than the shared ISM actor); only then the actor-attachment walk.
    {
        constexpr auto TraceComplex = false;
        auto Params = FCollisionQueryParams{TEXT("CkDebugger_Picker"), TraceComplex};
        Params.AddIgnoredActors(IgnoredActors);

        auto Hit = FHitResult{};
        const auto TraceEnd = InOrigin + (InDirection * _CullRadius);
        const auto Hit_Result = InWorld->LineTraceSingleByChannel(
            Hit, InOrigin, TraceEnd, ECC_Visibility, Params);

        if (Hit_Result)
        {
            auto ResolvedHandle = DoResolveIsmInstanceEntity(Hit);
            if (NOT ck::IsValid(ResolvedHandle))
            {
                ResolvedHandle = DoResolveActorEntity(InWorld, Hit.GetActor());
            }

            if (ck::IsValid(ResolvedHandle) && _Markers.Contains(ResolvedHandle))
            {
                ActorCandidate.Entity     = ResolvedHandle;
                ActorCandidate.RayT       = Hit.Distance;
                ActorCandidate.IsActorHit = true;
            }
        }
    }

    // ---- 1b. Analytic ray-vs-bounds for geometry-backed entities ----
    // Covers meshes the physics trace can't see: renderer-only ISMs run
    // NoCollision (the ISKM stress gyms — 500+ agents), so their instances
    // never produce a trace hit. Nearest box entry competes as a geometry
    // candidate; when both a trace hit and a box hit exist, nearest-T wins
    // (a box near-face is at most one mesh extent in front of its surface —
    // acceptable ranking noise for a debug picker).
    {
        auto BestBoxT      = TNumericLimits<float>::Max();
        auto BestBoxHandle = FCk_Handle{};

        for (const auto& Entry : _MeshPickBounds)
        {
            const auto HitT = Intersect_RayBoxNearT(Entry.Box, InOrigin, InDirection);
            if (NOT HitT.IsSet() || *HitT > _CullRadius)
            { continue; }

            if (*HitT < BestBoxT)
            {
                BestBoxT      = *HitT;
                BestBoxHandle = Entry.Entity;
            }
        }

        if (ck::IsValid(BestBoxHandle) &&
            (NOT ActorCandidate.IsValid() || BestBoxT < ActorCandidate.RayT))
        {
            ActorCandidate.Entity     = BestBoxHandle;
            ActorCandidate.RayT       = BestBoxT;
            ActorCandidate.IsActorHit = true;
        }
    }

    // ---- 2. Ray-sphere test against the previewed marker entries ----
    // The snapshot already applies the depth gate, distance cull, and ignore-self
    // filter, so every entry is fair game.
    {
        auto BestSphereT      = TNumericLimits<float>::Max();
        auto BestSphereHandle = FCk_Handle{};

        for (const auto& Entry : _Markers.Get_Entries())
        {
            const auto& Center = Entry.WorldPos;

            // Inline ray-sphere (quadratic). We need the near-t to compare against the actor hit,
            // so FMath::LineSphereIntersection (which only returns bool) is insufficient.
            const auto L    = Center - InOrigin;
            const auto B    = FVector::DotProduct(L, InDirection);
            const auto C    = FVector::DotProduct(L, L) - (PickRadius * PickRadius);
            const auto Disc = (B * B) - C;
            if (Disc < 0.0f)
            { continue; }

            const auto NearT = B - FMath::Sqrt(Disc);
            if (NearT < 0.0f || NearT > _CullRadius)
            { continue; }

            if (NearT < BestSphereT)
            {
                BestSphereT      = NearT;
                BestSphereHandle = Entry.Entity;
            }
        }

        if (ck::IsValid(BestSphereHandle))
        {
            SphereCandidate.Entity     = BestSphereHandle;
            SphereCandidate.RayT       = BestSphereT;
            SphereCandidate.IsActorHit = false;
        }
    }

    // ---- 3. Priority resolution ----
    // Actor hits report distance to the collision surface while sphere hits report distance to the
    // center of the marker. Bias toward the actor hit by one pick radius to compensate; otherwise the
    // sphere always wins on large actors.
    if (ActorCandidate.IsValid() && SphereCandidate.IsValid())
    {
        const auto BiasedSphereT = SphereCandidate.RayT + PickRadius;
        return ActorCandidate.RayT <= BiasedSphereT ? ActorCandidate.Entity : SphereCandidate.Entity;
    }

    if (ActorCandidate.IsValid())
    { return ActorCandidate.Entity; }

    if (SphereCandidate.IsValid())
    { return SphereCandidate.Entity; }

    return FCk_Handle{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoResolveIsmInstanceEntity(
        const FHitResult& InHit) const -> FCk_Handle
{
    const auto* IsmComp = Cast<UInstancedStaticMeshComponent>(InHit.GetComponent());
    if (ck::Is_NOT_Valid(IsmComp) || InHit.Item == INDEX_NONE)
    { return {}; }

    auto InstanceTransform = FTransform::Identity;
    constexpr auto WorldSpace = true;
    if (NOT IsmComp->GetInstanceTransform(InHit.Item, InstanceTransform, WorldSpace))
    { return {}; }

    // Match the instance's world position against the marker snapshot: the proxy
    // entity's transform IS the instance transform (modulo small local offsets).
    // Deliberately transform-based rather than instance-id-based — no dependency on
    // renderer internals or version-sensitive id<->index component API; the cost is
    // ambiguity below MatchRadius, which agent separation keeps rare in practice.
    constexpr auto MatchRadiusSq = 100.0f * 100.0f;
    const auto InstanceLocation  = InstanceTransform.GetLocation();

    auto BestHandle = FCk_Handle{};
    auto BestDistSq = MatchRadiusSq;
    for (const auto& Entry : _Markers.Get_Entries())
    {
        const auto DistSq = static_cast<float>(FVector::DistSquared(Entry.WorldPos, InstanceLocation));
        if (DistSq >= BestDistSq)
        { continue; }

        if (NOT UCk_Utils_IsmProxy_UE::Has(Entry.Entity))
        { continue; }

        BestDistSq = DistSq;
        BestHandle = Entry.Entity;
    }

    return BestHandle;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoIsMeshResolvable(
        const FCk_Handle& InEntity) const -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    if (UCk_Utils_IsmProxy_UE::Has(InEntity))
    { return true; }

    // Skeletal-instance proxies (ISKM) have no mesh-bounds API yet — they pick
    // via the 1 m-box fallback in Get_EntityWorldBounds. Exact skeletal bounds
    // is a recorded CkFoundation follow-up.
    if (UCk_Utils_IskmProxy_UE::Has(InEntity))
    { return true; }

    return ck::IsValid(UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoRefreshMarkers(
        UWorld* InWorld) -> void
{
    auto Params = FCkDebug_EntityMarkers::FGatherParams{};
    Params.CullOrigin = DoGet_CameraLocation(InWorld);
    Params.CullRadius = _CullRadius;

    if (_IgnoreLocalPawn)
    {
        auto IgnoredActors = DoGet_LocalIgnoredActors(InWorld);
        if (NOT IgnoredActors.IsEmpty())
        {
            // Consumed synchronously inside Gather — `this` capture is safe.
            Params.Filter = [this, IgnoredActors = MoveTemp(IgnoredActors)](const FCk_Handle& InHandle)
            {
                return NOT DoIsEntityOwnedByIgnoredActor(InHandle, IgnoredActors);
            };
        }
    }

    _Markers.Gather(InWorld, Params);

    // One pass over the snapshot for everything geometry-related:
    //   - analytic pick volumes (ray-vs-bounds pick + hover outline), always;
    //   - "Meshes first" diamond suppression (still gathered — such entities stay
    //     pickable via their meshes and keep their links).
    // Bounds resolve per entry per tick (ISM-proxy mesh bounds / actor bounds via
    // DebugFocus) — hundreds of entries is fine for a debug-tool tick.
    _MeshSuppressedNums.Reset();
    _MeshPickBounds.Reset();
    for (const auto& Entry : _Markers.Get_Entries())
    {
        if (NOT DoIsMeshResolvable(Entry.Entity))
        { continue; }

        if (const auto Bounds = ck::DebugFocus::Get_EntityWorldBounds(Entry.Entity))
        { _MeshPickBounds.Add(FMeshPickEntry{Entry.Entity, Bounds->GetBox()}); }

        if (_MeshesFirst)
        { _MeshSuppressedNums.Add(Entry.EntityNum); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoDrawBillboards(
        UCanvas*           InCanvas,
        APlayerController* InPC) -> void
{
    if (NOT _IsActive)
    { return; }

    if (InCanvas == nullptr)
    { return; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return; }

    // DebugDrawService fires for every game viewport on screen; reject callbacks for worlds we aren't inspecting.
    if (ck::IsValid(InPC) && InPC->GetWorld() != World)
    { return; }

    auto DrawParams            = FCkDebug_EntityMarkers::FDrawParams{};
    DrawParams.TileSizePx      = FMath::Clamp(_BillboardSizePx, BillboardSizeMinPx, BillboardSizeMaxPx);
    DrawParams.DefaultAlpha    = BillboardDefaultAlpha;
    DrawParams.EmphasizedScale = BillboardHoverScale;

    // Emphasize the sticky focus diamond. No text label — entity detail now lives in the
    // reused overlay focus card (Set_FocusCardContent), not as canvas text.
    if (ck::IsValid(_FocusEntity))
    {
        DrawParams.EmphasizedEntityNum =
            static_cast<uint32>(_FocusEntity.Get_Entity().Get_EntityNumber());
    }

    // "Meshes first" declutter — geometry-pickable entities lose their diamond
    // (they are hover-outlined + trace-picked via their meshes instead).
    if (_MeshesFirst && NOT _MeshSuppressedNums.IsEmpty())
    {
        DrawParams.SuppressedEntityNums = &_MeshSuppressedNums;
    }

    _Markers.DrawMarkers(InCanvas, DrawParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoGet_LocalIgnoredActors(
        UWorld* InWorld) const -> TArray<TWeakObjectPtr<const AActor>>
{
    auto IgnoredActors = TArray<TWeakObjectPtr<const AActor>>{};

    if (NOT _IgnoreLocalPawn)
    { return IgnoredActors; }

    if (ck::Is_NOT_Valid(InWorld))
    { return IgnoredActors; }

    // Ejected PIE: no in-game "local pawn" context worth ignoring, and the user may want to pick
    // their own pawn anyway.
    if (ck::DebugViewportView::Get_IsEjected())
    { return IgnoredActors; }

    auto* PC = InWorld->GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PC))
    { return IgnoredActors; }

    auto* LocalPawn = PC->GetPawn().Get();
    if (ck::Is_NOT_Valid(LocalPawn))
    { return IgnoredActors; }

    IgnoredActors.Add(LocalPawn);
    IgnoredActors.Add(PC);

    constexpr auto ResetArray     = true;
    constexpr auto RecurseAttached = true;
    auto AttachedChildren = TArray<AActor*>{};
    LocalPawn->GetAttachedActors(AttachedChildren, ResetArray, RecurseAttached);
    for (auto* AttachedActor : AttachedChildren)
    {
        if (ck::IsValid(AttachedActor))
        {
            IgnoredActors.Add(AttachedActor);
        }
    }

    return IgnoredActors;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoIsEntityOwnedByIgnoredActor(
        const FCk_Handle& InEntity,
        const TArray<TWeakObjectPtr<const AActor>>& InIgnoredActors) const -> bool
{
    if (InIgnoredActors.IsEmpty())
    { return false; }

    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    auto* OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InEntity);
    if (ck::Is_NOT_Valid(OwningActor))
    { return false; }

    for (const auto& IgnoredActorWeak : InIgnoredActors)
    {
        const auto* IgnoredActor = IgnoredActorWeak.Get();
        if (ck::Is_NOT_Valid(IgnoredActor))
        { continue; }

        if (OwningActor == IgnoredActor)
        { return true; }
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoGet_CameraLocation(
        UWorld* InWorld) const -> FVector
{
    return ck::DebugViewportView::Get_ViewCameraLocation(InWorld).Get(_LastRayOrigin);
}

// =====================================================================================================================
// Overlay cards (focus card + world tags) — reused from the On-Screen Overlay so the picker
// shows the same main card + world cards instead of bespoke canvas text.
// =====================================================================================================================

auto
    FCkDebuggerModel_ViewportPicker::
    DoActivateOverlayCards(
        UWorld* InWorld) -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    // AMENDMENT 1 — coexistence: if the On-Screen Overlay subsystem is already running
    // (ck.DebugOverlay != 0) it is drawing its own focus card + world tags for this player.
    // Adding ours would double them up, so skip — the picker's diamonds still draw.
    if (const auto* MasterCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
        MasterCVar != nullptr && MasterCVar->GetInt() != 0)
    { return; }

    auto* GVC = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return; }

    if (_OverlayProviders.IsEmpty())
    {
        _OverlayProviders = FCk_DebugOverlay_Registry::Get().CreateAll();
    }
    _OverlayHistory = MakeUnique<FCk_DebugOverlay_History>();

    // v1: use the overlay's StartingLayout (the live cycle index lives in the subsystem
    // instance and isn't statically reachable). Picker and overlay can diverge if the user
    // cycles layouts; acceptable for now.
    _OverlayLayoutIndex = ck_debugoverlay::Get_StartingLayoutIndex(
        GetDefault<UCk_DebugOverlay_Settings>());

    _OverlayRoot = SNew(SCkDebugOverlay_Root);

    // Same Z-order as the overlay subsystem (above game UI, below modal dialogs).
    constexpr auto OverlayCardZOrder = 100;
    GVC->AddViewportWidgetContent(_OverlayRoot.ToSharedRef(), OverlayCardZOrder);

    _OverlayCardsActive = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoDeactivateOverlayCards() -> void
{
    if (_OverlayRoot.IsValid())
    {
        if (auto* World = DoResolveTargetWorld(); ck::IsValid(World))
        {
            if (auto* GVC = World->GetGameViewport(); ck::IsValid(GVC))
            {
                GVC->RemoveViewportWidgetContent(_OverlayRoot.ToSharedRef());
            }
        }
        _OverlayRoot.Reset();
    }

    _OverlayProviders.Reset();
    _OverlayHistory.Reset();
    _OverlayCardsActive = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoUpdateOverlayCards(
        UWorld*            InWorld,
        APlayerController* InPC,
        bool               InIsEjected) -> void
{
    if (NOT _OverlayCardsActive || NOT _OverlayRoot.IsValid())
    { return; }

    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    const auto* Layout   = ck_debugoverlay::Resolve_Layout(Settings, _OverlayLayoutIndex);
    if (Layout == nullptr || NOT _OverlayHistory)
    { return; }

    const auto Now = FPlatformTime::Seconds();

    // ---- Plate anchor + width (settings-driven; cheap no-op when unchanged) ----
    _OverlayRoot->Set_PlateLayout(
        Settings ? Settings->PlateAnchor : ECk_DebugOverlay_PlateAnchor::TopRight,
        Settings ? Settings->PlateWidth  : 720.0f);

    // ---- Focus card for the sticky focus entity (empty model hides the card content) ----
    const auto Model = ck_debugoverlay::Build_EntityModel(
        _FocusEntity, _OverlayProviders, *Layout, _OverlayHistory.Get(), Now);

    auto CardStyle = Layout->DefaultStyle;
    CardStyle.FontScale *= Settings ? Settings->PlateFontScale : 1.0f;

    _OverlayRoot->Set_FocusCardContent(Model, CardStyle, *_OverlayHistory, Now, /*bIsLocked=*/false);

    // ---- World tags for the previewed candidates (the marker snapshot = the candidate set) ----
    auto Handles    = TArray<FCk_Handle>{};
    auto Candidates = TArray<ck_debugoverlay::FCandidate>{};
    Handles.Reserve(_Markers.Get_Entries().Num());
    Candidates.Reserve(_Markers.Get_Entries().Num());

    for (const auto& Entry : _Markers.Get_Entries())
    {
        auto Cand          = ck_debugoverlay::FCandidate{};
        Cand.WorldLocation = Entry.WorldPos;
        Cand.Depth         = Entry.Depth;
        Cand.bIsOnScreen   = false;

        if (ck::IsValid(InPC) && NOT InIsEjected)
        {
            auto ScreenPos = FVector2D{};
            Cand.bIsOnScreen = UGameplayStatics::ProjectWorldToScreen(
                InPC, Entry.WorldPos, ScreenPos, /*bPlayerViewportRelative=*/false);
        }

        Handles.Add(Entry.Entity);
        Candidates.Add(Cand);
    }

    const auto WorldTags = ck_debugoverlay::Build_WorldTags(
        Handles, Candidates, _OverlayProviders, *Layout, InPC, InIsEjected);

    _OverlayRoot->Update_WorldTags(WorldTags);
}
