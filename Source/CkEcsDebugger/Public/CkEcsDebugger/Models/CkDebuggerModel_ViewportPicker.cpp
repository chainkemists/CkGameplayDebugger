#include "CkDebuggerModel_ViewportPicker.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerViewportPicker_InputProcessor.h"

// Successful pick re-focuses the debugger tab (the game viewport took focus while picking).
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"

// On-Screen Overlay reuse — same focus card + world tags, built via the shared presenters.
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "Debug/DebugDrawService.h"
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

#if WITH_EDITOR
    // When the PIE player is "ejected" (F8), this project's setup does NOT swap the LocalPlayer to
    // an ADebugCameraController — the game PC's view simply freezes at the eject point and input
    // goes to the Level Editor viewport. A pick sourced from the game PC would use a stale camera,
    // so in that state we deproject against the live Level Editor viewport instead.
    //
    // Discriminator: `GEditor->GetActiveViewport()` points at whichever FViewport is currently
    // driving input/rendering. When possessed it is the PIE game viewport; when ejected it is the
    // level editor viewport itself. Other candidates (IsInGameView, the viewport's client pointer,
    // ActorLock, cursor-in-widget checks) do not reliably flip — particularly in "Play in Current
    // Viewport" mode where the game viewport widget is nested inside the level editor viewport.
    static auto TryGet_LevelEditorViewport() -> FEditorViewportClient*
    {
        if (GEditor == nullptr || GEditor->PlayWorld == nullptr)
        { return nullptr; }

        auto* LEVC = GCurrentLevelEditingViewportClient;
        if (LEVC == nullptr || LEVC->Viewport == nullptr)
        { return nullptr; }

        if (GEditor->GetActiveViewport() != LEVC->Viewport)
        { return nullptr; }

        return LEVC;
    }

    static auto Deproject_LevelEditorViewport(
        FEditorViewportClient* InVC,
        FVector&               OutOrigin,
        FVector&               OutDirection) -> bool
    {
        auto* Viewport = InVC->Viewport;
        if (Viewport == nullptr)
        { return false; }

        const auto Size   = Viewport->GetSizeXY();
        const auto MouseX = Viewport->GetMouseX();
        const auto MouseY = Viewport->GetMouseY();
        if (Size.X <= 0 || Size.Y <= 0)
        { return false; }
        if (MouseX < 0 || MouseY < 0 || MouseX >= Size.X || MouseY >= Size.Y)
        { return false; }

        if (InVC->IsOrtho())
        { return false; }

        // Build view+projection from the viewport client's CURRENT camera state, rather than
        // calling FEditorViewportClient::CalcSceneView() — that call returns stale matrices on
        // repeat invocations outside the viewport's own Draw pass.
        const auto ViewLocation = InVC->GetViewLocation();
        const auto ViewRotation = InVC->GetViewRotation();

        const auto ViewRotationMatrix = FInverseRotationMatrix(ViewRotation) * FMatrix(
            FPlane(0.0, 0.0, 1.0, 0.0),
            FPlane(1.0, 0.0, 0.0, 0.0),
            FPlane(0.0, 1.0, 0.0, 0.0),
            FPlane(0.0, 0.0, 0.0, 1.0));
        const auto ViewMatrix = FTranslationMatrix(-ViewLocation) * ViewRotationMatrix;

        const auto AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
        const auto HalfFOVRad  = FMath::DegreesToRadians(InVC->ViewFOV * 0.5f);

        const auto ProjectionMatrix = FReversedZPerspectiveMatrix(
            HalfFOVRad, AspectRatio, 1.0f, GNearClippingPlane);

        const auto InvViewProj = (ViewMatrix * ProjectionMatrix).InverseFast();
        const auto ViewRect    = FIntRect(0, 0, Size.X, Size.Y);

        FSceneView::DeprojectScreenToWorld(
            FVector2D(MouseX, MouseY), ViewRect, InvViewProj, OutOrigin, OutDirection);
        return true;
    }
#endif
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

    // Push the overlay focus card + world tags for the current focus / candidates.
    if (_OverlayCardsActive)
    {
        auto* PC = World->GetFirstPlayerController();

        // Ejected PIE: world tags use frozen-camera PC projection — DoUpdateOverlayCards
        // skips them then (the focus card still shows). Mirror the picker's own ejected
        // detection used elsewhere.
        auto IsEjected = false;
#if WITH_EDITOR
        IsEjected = TryGet_LevelEditorViewport() != nullptr;
#endif
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
    if (ck::Is_NOT_Valid(InWorld))
    { return false; }

#if WITH_EDITOR
    // During ejected PIE, the game PC's view freezes — deproject against the active Level Editor
    // viewport's live camera instead.
    if (auto* LEVC = TryGet_LevelEditorViewport())
    {
        return Deproject_LevelEditorViewport(LEVC, OutOrigin, OutDirection);
    }
#endif

    auto* GVC = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return false; }

    auto* LocalPlayer = InWorld->GetFirstLocalPlayerFromController();
    if (ck::Is_NOT_Valid(LocalPlayer))
    { return false; }

    const auto ViewportWidget = GVC->GetGameViewportWidget();
    if (NOT ViewportWidget.IsValid())
    { return false; }

    const auto& Geom        = ViewportWidget->GetCachedGeometry();
    const auto  LocalSize   = Geom.GetLocalSize();
    const auto  LocalPixels = Geom.AbsoluteToLocal(InAbsolutePos);

    if (LocalPixels.X < 0.0f || LocalPixels.Y < 0.0f ||
        LocalPixels.X >= LocalSize.X || LocalPixels.Y >= LocalSize.Y)
    { return false; }

    auto* Viewport = GVC->Viewport;
    if (Viewport == nullptr)
    { return false; }

    constexpr auto RealtimeUpdate = true;
    auto ViewFamilyArgs = FSceneViewFamily::ConstructionValues(
        Viewport, InWorld->Scene, GVC->EngineShowFlags)
        .SetRealtimeUpdate(RealtimeUpdate);
    auto ViewFamily = FSceneViewFamilyContext{MoveTemp(ViewFamilyArgs)};

    auto ViewLocation = FVector::ZeroVector;
    auto ViewRotation = FRotator::ZeroRotator;
    auto* SceneView   = LocalPlayer->CalcSceneView(
        &ViewFamily, ViewLocation, ViewRotation, Viewport);

    if (SceneView == nullptr)
    { return false; }

    // CalcSceneView expects viewport pixels; Slate's local coordinates match when the viewport
    // widget has a 1:1 DPI scale in the cached geometry (the division is baked into AbsoluteToLocal).
    SceneView->DeprojectFVector2D(LocalPixels, OutOrigin, OutDirection);
    return true;
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

    // ---- 1. Physics line trace for actor-backed entities ----
    // The resolved entity must be in the marker snapshot — the depth gate, distance
    // cull, and ignore-self filter apply to actor hits too (what you see is what you
    // can pick).
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
            auto* HitActor = Hit.GetActor();
            const auto ResolvedHandle = DoResolveActorEntity(InWorld, HitActor);
            if (ck::IsValid(ResolvedHandle) && _Markers.Contains(ResolvedHandle))
            {
                ActorCandidate.Entity     = ResolvedHandle;
                ActorCandidate.RayT       = Hit.Distance;
                ActorCandidate.IsActorHit = true;
            }
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

#if WITH_EDITOR
    // Ejected PIE: no in-game "local pawn" context worth ignoring, and the user may want to pick
    // their own pawn anyway.
    if (TryGet_LevelEditorViewport() != nullptr)
    { return IgnoredActors; }
#endif

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
    if (ck::Is_NOT_Valid(InWorld))
    { return _LastRayOrigin; }

#if WITH_EDITOR
    if (auto* LEVC = TryGet_LevelEditorViewport())
    { return LEVC->GetViewLocation(); }
#endif

    auto* PC = InWorld->GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PC))
    { return _LastRayOrigin; }

    auto  CameraLocation = FVector::ZeroVector;
    auto  CameraRotation = FRotator::ZeroRotator;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    return CameraLocation;
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
