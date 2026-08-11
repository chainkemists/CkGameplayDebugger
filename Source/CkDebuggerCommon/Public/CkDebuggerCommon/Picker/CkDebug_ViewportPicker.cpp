#include "CkDebug_ViewportPicker.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkIskmRenderer/Proxy/CkIskmProxy_Utils.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Picker/CkDebug_PickerOverlayCards.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPickerInputProcessor.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
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

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_viewport_picker
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

    // Ejected-PIE discrimination + deprojection live in ck::DebugViewportView —
    // shared with the on-screen overlay and the focus-entity helper.

    // Slab-method ray/AABB: near-hit T along InDirection (0 when the origin is
    // inside the box), unset on miss. FMath::LineBoxIntersection returns bool
    // only — ranking candidates needs the T.
    static auto Intersect_RayBoxNearT(
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

FCkDebug_ViewportPicker::FCkDebug_ViewportPicker() = default;

FCkDebug_ViewportPicker::~FCkDebug_ViewportPicker()
{
    Deactivate();
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
    Construct(
        FParams InParams) -> void
{
    _Params = MoveTemp(InParams);

    _MeshesFirst = UCkDebuggerSettings::Get()->PickerMeshesFirst;
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
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

    _InputProcessor = MakeShared<FCkDebug_ViewportPickerInputProcessor>(AsShared());

    constexpr auto InputPriority = 0;
    FSlateApplication::Get().RegisterInputPreProcessor(_InputProcessor, InputPriority);

    // Any session boundary (BeginPIE/EndPIE) invalidates the world and every
    // cached handle — exit pick mode synchronously.
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddLambda(
        [WeakSelf = TWeakPtr<FCkDebug_ViewportPicker>(AsShared())]()
        {
            if (const auto StrongSelf = WeakSelf.Pin())
            {
                StrongSelf->Deactivate();
            }
        });

    // Front-load the shared marker textures (load + async compilation) so the first
    // DrawMarkers call never sees a placeholder resource — that was the intermittent
    // crash on cold editor starts. Then build the initial snapshot so markers appear
    // on this frame's draw rather than after the first Tick.
    _Markers.EnsureTextures();
    DoRefreshMarkers(World);

    // Register for BOTH "Game" (PIE viewports) and "Editor" (level editor viewport) show flags so
    // billboards keep rendering after the user ejects with F8 and input moves to the editor viewport.
    const auto DrawDelegate = FDebugDrawDelegate::CreateSP(
        AsShared(), &FCkDebug_ViewportPicker::DoDrawBillboards);
    _DebugDrawHandle_Game   = UDebugDrawService::Register(TEXT("Game"),   DrawDelegate);
    _DebugDrawHandle_Editor = UDebugDrawService::Register(TEXT("Editor"), DrawDelegate);

    // Focus card + world tags, reused from the On-Screen Overlay via the factory
    // hook. Null when the overlay module is absent — diamonds still draw.
    _OverlayCards = ck::DebugPickerCards::Create();
    if (_OverlayCards.IsValid())
    {
        _OverlayCards->Activate(World);
    }

    _IsActive = true;
    OnPickModeChanged.Broadcast(_IsActive);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
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

    if (_SessionInvalidatedHandle.IsValid())
    {
        ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle);
        _SessionInvalidatedHandle.Reset();
    }

    if (_OverlayCards.IsValid())
    {
        _OverlayCards->Deactivate(DoResolveTargetWorld());
        _OverlayCards.Reset();
    }

    _Markers.Reset();
    _FocusEntity = FCk_Handle{};
    _HasRay      = false;
    _IsActive    = false;

    OnPickModeChanged.Broadcast(_IsActive);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
    IsActive() const -> bool
{
    return _IsActive;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
    CanActivate() const -> bool
{
    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    return ck::IsValid(World->GetGameViewport());
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
    Get_IgnoreLocalPawn() const -> bool
{
    return _IgnoreLocalPawn;
}

auto
    FCkDebug_ViewportPicker::
    Set_IgnoreLocalPawn(
        bool InValue) -> void
{
    _IgnoreLocalPawn = InValue;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
    Get_CullRadius() const -> float
{
    return _CullRadius;
}

auto
    FCkDebug_ViewportPicker::
    Set_CullRadius(
        float InValue) -> void
{
    _CullRadius = FMath::Clamp(InValue,
        ck_debug_viewport_picker::CullRadiusMin, ck_debug_viewport_picker::CullRadiusMax);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
    Get_BillboardSize() const -> float
{
    return _BillboardSizePx;
}

auto
    FCkDebug_ViewportPicker::
    Set_BillboardSize(
        float InValue) -> void
{
    _BillboardSizePx = FMath::Clamp(InValue,
        ck_debug_viewport_picker::BillboardSizeMinPx, ck_debug_viewport_picker::BillboardSizeMaxPx);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
    Get_MeshesFirst() const -> bool
{
    return _MeshesFirst;
}

auto
    FCkDebug_ViewportPicker::
    Set_MeshesFirst(
        bool InValue) -> void
{
    if (_MeshesFirst == InValue)
    { return; }

    _MeshesFirst = InValue;

    if (auto* Settings = GetMutableDefault<UCkDebuggerSettings>())
    {
        Settings->PickerMeshesFirst = InValue;
        Settings->SaveConfig();
    }
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
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
    if (_OverlayCards.IsValid())
    {
        auto* PC = World->GetFirstPlayerController();

        // Ejected PIE: world tags use frozen-camera PC projection — the cards
        // presenter skips them then (the focus card still shows).
        const auto IsEjected = ck::DebugViewportView::Get_IsEjected();
        _OverlayCards->Update(World, PC, IsEjected, _FocusEntity, _Markers);
    }
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
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

    // Deactivate BEFORE handing the pick to the host so mouse capture is
    // restored and the input pre-processor is gone by the time the host
    // re-focuses its tab. Hit is a by-value handle — safe across Deactivate.
    Deactivate();

    if (_Params.OnEntityPicked)
    {
        _Params.OnEntityPicked(Hit);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportPicker::
    OnEscapePressed() -> bool
{
    if (NOT _IsActive)
    { return false; }

    Deactivate();
    return true;
}

// =====================================================================================================================

auto
    FCkDebug_ViewportPicker::
    DoResolveTargetWorld() const -> UWorld*
{
    if (NOT _Params.Get_TargetWorld)
    { return nullptr; }

    auto* World = _Params.Get_TargetWorld();
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
    DoResolveActorEntity(
        UWorld* InWorld,
        AActor* InHitActor) const -> FCk_Handle
{
    if (ck::Is_NOT_Valid(InHitActor))
    { return FCk_Handle{}; }

    auto* CurrentActor = InHitActor;
    for (auto Depth = 0; Depth < ck_debug_viewport_picker::MaxAttachmentWalkDepth; ++Depth)
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
    FCkDebug_ViewportPicker::
    DoPickAtRay(
        UWorld*        InWorld,
        const FVector& InOrigin,
        const FVector& InDirection) const -> FCk_Handle
{
    using namespace ck_debug_viewport_picker;

    if (ck::Is_NOT_Valid(InWorld))
    { return FCk_Handle{}; }

    const auto IgnoredActors = DoGet_LocalIgnoredActors(InWorld);

    auto ActorCandidate  = FPickCandidate{};
    auto SphereCandidate = FPickCandidate{};

    // ---- 1. Physics line trace for geometry-backed entities ----
    // The resolved entity must be in the marker snapshot — the depth gate, distance
    // cull, ignore-self filter, and (in target mode) the host's target filter apply
    // to trace hits too (what you see is what you can pick). An ISM-instance hit
    // resolves to its proxy entity FIRST (more specific than the shared ISM actor);
    // only then the actor-attachment walk.
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

            // Target mode may exclude the trace-resolved entity itself — e.g. the
            // hit resolves to an ActorRelay's entity while the snapshot holds the
            // NPC representative under it. Fall back to the closest same-lineage
            // snapshot entry so clicking the mesh still picks the previewed entity.
            if (ck::IsValid(ResolvedHandle) && NOT _Markers.Contains(ResolvedHandle))
            {
                auto BestDistSq = TNumericLimits<double>::Max();
                auto BestLineage = FCk_Handle{};
                for (const auto& Entry : _Markers.Get_Entries())
                {
                    if (NOT ck::DebugSelectionSync::Is_SameLineage(Entry.Entity, ResolvedHandle))
                    { continue; }

                    const auto DistSq = FVector::DistSquared(Entry.WorldPos, Hit.ImpactPoint);
                    if (DistSq < BestDistSq)
                    {
                        BestDistSq  = DistSq;
                        BestLineage = Entry.Entity;
                    }
                }
                ResolvedHandle = BestLineage;
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
    DoRefreshMarkers(
        UWorld* InWorld) -> void
{
    auto Params = FCkDebug_EntityMarkers::FGatherParams{};
    Params.CullOrigin  = DoGet_CameraLocation(InWorld);
    Params.CullRadius  = _CullRadius;
    Params.TargetMatch = _Params.TargetFilter;
    if (ck::IsValid(_FocusEntity))
    {
        Params.FullDepthRoots.Add(_FocusEntity);
    }

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
    FCkDebug_ViewportPicker::
    DoDrawBillboards(
        UCanvas*           InCanvas,
        APlayerController* InPC) -> void
{
    using namespace ck_debug_viewport_picker;

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

    // Emphasize the sticky focus diamond. No text label — entity detail lives in the
    // reused overlay focus card, not as canvas text.
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
    FCkDebug_ViewportPicker::
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

    constexpr auto ResetArray      = true;
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
    FCkDebug_ViewportPicker::
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
    FCkDebug_ViewportPicker::
    DoGet_CameraLocation(
        UWorld* InWorld) const -> FVector
{
    return ck::DebugViewportView::Get_ViewCameraLocation(InWorld).Get(_LastRayOrigin);
}
