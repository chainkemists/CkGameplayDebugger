#include "CkDebuggerModel_ViewportPicker.h"

#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerViewportPicker_InputProcessor.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

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

namespace
{
    static constexpr auto PickRadius             = 30.0f;        // cm
    static constexpr auto MaxAttachmentWalkDepth = 16;
    static constexpr auto MarkerLodNearDistance  = 2'000.0f;     // full octahedron within this distance
    static constexpr auto MarkerThickness        = 1.5f;
    static constexpr auto HoverThickness         = 3.0f;
    static constexpr auto CullRadiusMin          = 100.0f;       // 1 m
    static constexpr auto CullRadiusMax          = 100'000.0f;   // 1 km
    static constexpr auto DebugDrawLifetime      = 0.0f;
    static constexpr auto NonPersistentLines     = false;
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

    DoRestoreMouseState();

    if (_WorldChangedHandle.IsValid())
    {
        if (const auto WorldModel = _WorldModel.Pin())
        {
            WorldModel->OnWorldChanged.Remove(_WorldChangedHandle);
        }
        _WorldChangedHandle.Reset();
    }

    _HoveredEntity = FCk_Handle{};
    _HasRay        = false;
    _IsActive      = false;

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
    Get_DrawThroughWalls() const -> bool
{
    return _DrawThroughWalls;
}

auto
    FCkDebuggerModel_ViewportPicker::
    Set_DrawThroughWalls(
        bool InValue) -> void
{
    _DrawThroughWalls = InValue;
}

// --------------------------------------------------------------------------------------------------------------------

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

    // Refresh the hover hit under the last known cursor ray, since entities can move between frames.
    if (_HasRay)
    {
        _HoveredEntity = DoPickAtRay(World, _LastRayOrigin, _LastRayDirection);
    }

    DoDrawMarkers(World);
    DoDrawHover(World);
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
        _HasRay        = false;
        _HoveredEntity = FCk_Handle{};
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
    const auto CullRadiusSq  = _CullRadius * _CullRadius;

    auto ActorCandidate  = FPickCandidate{};
    auto SphereCandidate = FPickCandidate{};

    // ---- 1. Physics line trace for actor-backed entities ----
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
            if (ck::IsValid(ResolvedHandle))
            {
                ActorCandidate.Entity     = ResolvedHandle;
                ActorCandidate.RayT       = Hit.Distance;
                ActorCandidate.IsActorHit = true;
            }
        }
    }

    // ---- 2. Ray-sphere test against all FFragment_Transform entities ----
    {
        auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
        if (ck::IsValid(TransientEntity))
        {
            auto BestSphereT      = TNumericLimits<float>::Max();
            auto BestSphereHandle = FCk_Handle{};

            TransientEntity.View<ck::FFragment_Transform, CK_IGNORE_PENDING_KILL>().ForEach(
            [&](FCk_Entity InEntity, const ck::FFragment_Transform& InTransform)
            {
                const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                const auto Center = InTransform.Get_Transform().GetLocation();

                const auto DistSq = FVector::DistSquared(Center, InOrigin);
                if (DistSq > CullRadiusSq)
                { return; }

                if (_IgnoreLocalPawn && DoIsEntityOwnedByIgnoredActor(Handle, IgnoredActors))
                { return; }

                // Inline ray-sphere (quadratic). We need the near-t to compare against the actor hit,
                // so FMath::LineSphereIntersection (which only returns bool) is insufficient.
                const auto L    = Center - InOrigin;
                const auto B    = FVector::DotProduct(L, InDirection);
                const auto C    = FVector::DotProduct(L, L) - (PickRadius * PickRadius);
                const auto Disc = (B * B) - C;
                if (Disc < 0.0f)
                { return; }

                const auto NearT = B - FMath::Sqrt(Disc);
                if (NearT < 0.0f || NearT > _CullRadius)
                { return; }

                if (NearT < BestSphereT)
                {
                    BestSphereT      = NearT;
                    BestSphereHandle = Handle;
                }
            });

            if (ck::IsValid(BestSphereHandle))
            {
                SphereCandidate.Entity     = BestSphereHandle;
                SphereCandidate.RayT       = BestSphereT;
                SphereCandidate.IsActorHit = false;
            }
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
    DoDrawMarkers(
        UWorld* InWorld) const -> void
{
#if ENABLE_DRAW_DEBUG
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    const auto CameraLocation  = DoGet_CameraLocation(InWorld);
    const auto CullRadiusSq    = _CullRadius * _CullRadius;
    const auto MarkerColor     = FCkDebuggerStyle::Color_PickMarker_Default.ToFColor(true);
    const auto DepthPriority   = static_cast<uint8>(_DrawThroughWalls ? SDPG_Foreground : SDPG_World);

    const auto IgnoredActors = DoGet_LocalIgnoredActors(InWorld);

    TransientEntity.View<ck::FFragment_Transform, CK_IGNORE_PENDING_KILL>().ForEach(
    [&](FCk_Entity InEntity, const ck::FFragment_Transform& InTransform)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
        if (ck::Is_NOT_Valid(Handle))
        { return; }

        const auto Center = InTransform.Get_Transform().GetLocation();

        const auto DistSqToCamera = FVector::DistSquared(Center, CameraLocation);
        if (DistSqToCamera > CullRadiusSq)
        { return; }

        if (_IgnoreLocalPawn && DoIsEntityOwnedByIgnoredActor(Handle, IgnoredActors))
        { return; }

        const auto XVertPos = Center + FVector{PickRadius, 0.0f, 0.0f};
        const auto XVertNeg = Center - FVector{PickRadius, 0.0f, 0.0f};
        const auto YVertPos = Center + FVector{0.0f, PickRadius, 0.0f};
        const auto YVertNeg = Center - FVector{0.0f, PickRadius, 0.0f};
        const auto ZVertPos = Center + FVector{0.0f, 0.0f, PickRadius};
        const auto ZVertNeg = Center - FVector{0.0f, 0.0f, PickRadius};

        const auto DistToCamera = FMath::Sqrt(DistSqToCamera);
        if (DistToCamera <= MarkerLodNearDistance)
        {
            // Full octahedron (12 edges)
            ::DrawDebugLine(InWorld, XVertPos, YVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertPos, XVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, XVertNeg, YVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertNeg, XVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);

            ::DrawDebugLine(InWorld, XVertPos, ZVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertPos, ZVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, XVertNeg, ZVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertNeg, ZVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);

            ::DrawDebugLine(InWorld, XVertPos, ZVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertPos, ZVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, XVertNeg, ZVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertNeg, ZVertNeg, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
        }
        else
        {
            // Far LOD: just the three principal axes through the center
            ::DrawDebugLine(InWorld, XVertNeg, XVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, YVertNeg, YVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
            ::DrawDebugLine(InWorld, ZVertNeg, ZVertPos, MarkerColor, NonPersistentLines, DebugDrawLifetime, DepthPriority, MarkerThickness);
        }
    });
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_ViewportPicker::
    DoDrawHover(
        UWorld* InWorld) const -> void
{
#if ENABLE_DRAW_DEBUG
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    if (ck::Is_NOT_Valid(_HoveredEntity))
    { return; }

    if (NOT UCk_Utils_Transform_UE::Has(_HoveredEntity))
    { return; }

    const auto Center         = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(_HoveredEntity).GetLocation();
    const auto HoverColor     = FCkDebuggerStyle::Color_PickMarker_Hover.ToFColor(true);
    const auto DepthPriority  = static_cast<uint8>(_DrawThroughWalls ? SDPG_Foreground : SDPG_World);
    constexpr auto Segments   = 16;

    ::DrawDebugSphere(
        InWorld, Center, PickRadius, Segments, HoverColor,
        NonPersistentLines, DebugDrawLifetime, DepthPriority, HoverThickness);

    const auto LabelLocation = Center + FVector{0.0f, 0.0f, PickRadius + 20.0f};
    UCk_Utils_DebugDraw_UE::DrawDebugString(
        InWorld, LabelLocation, _HoveredEntity.ToString(), FCkDebuggerStyle::Color_PickMarker_Hover, DebugDrawLifetime);
#endif
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

    auto* PC = InWorld->GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PC))
    { return _LastRayOrigin; }

    auto  CameraLocation = FVector::ZeroVector;
    auto  CameraRotation = FRotator::ZeroRotator;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    return CameraLocation;
}
