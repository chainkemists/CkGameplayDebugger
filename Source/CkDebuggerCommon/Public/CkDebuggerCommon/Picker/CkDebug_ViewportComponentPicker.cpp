#include "CkDebug_ViewportComponentPicker.h"

#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportComponentPickerInputProcessor.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_viewport_component_picker
{
    constexpr auto CullRadiusMin = 100.0f;
    constexpr auto CullRadiusMax = 100'000.0f;
}

// =====================================================================================================================

auto FCkDebug_ComponentPickResult::IsValid() const -> bool
{
    return Component.IsValid();
}

// =====================================================================================================================

FCkDebug_ViewportComponentPicker::FCkDebug_ViewportComponentPicker() = default;

FCkDebug_ViewportComponentPicker::~FCkDebug_ViewportComponentPicker()
{
    Deactivate();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    Construct(
        FParams InParams) -> void
{
    _Params = MoveTemp(InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    Activate() -> bool
{
    if (_IsActive)
    { return true; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    const auto IsPlayableWorld = World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game;
    if (IsPlayableWorld && ck::Is_NOT_Valid(World->GetGameViewport()))
    { return false; }

    if (NOT FSlateApplication::IsInitialized())
    { return false; }

    if (IsPlayableWorld)
    {
        DoCaptureMouseState(World);
    }

    _InputProcessor = MakeShared<FCkDebug_ViewportComponentPickerInputProcessor>(AsShared());
    constexpr auto InputPriority = 0;
    FSlateApplication::Get().RegisterInputPreProcessor(_InputProcessor, InputPriority);

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddLambda(
        [WeakPicker = TWeakPtr<FCkDebug_ViewportComponentPicker>{AsShared()}]()
        {
            if (const auto Picker = WeakPicker.Pin())
            {
                Picker->Deactivate();
            }
        });

    _HoveredResult = {};
    _HasPendingMousePosition = false;
    _IsActive = true;
    OnPickModeChanged.Broadcast(true);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
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

    if (_SessionInvalidatedHandle.IsValid())
    {
        ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle);
        _SessionInvalidatedHandle.Reset();
    }

    _HoveredResult = {};
    _HasPendingMousePosition = false;
    _IsActive = false;
    OnPickModeChanged.Broadcast(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    Toggle() -> void
{
    if (_IsActive) { Deactivate(); }
    else { Activate(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_ViewportComponentPicker::IsActive() const -> bool
{
    return _IsActive;
}

auto FCkDebug_ViewportComponentPicker::CanActivate() const -> bool
{
    return ck::IsValid(DoResolveTargetWorld());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
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

    if (_HasPendingMousePosition)
    {
        auto Origin = FVector::ZeroVector;
        auto Direction = FVector::ForwardVector;
        _HoveredResult = DoDeproject(World, _PendingMousePosition, Origin, Direction)
            ? DoPickAtRay(World, Origin, Direction)
            : FCkDebug_ComponentPickResult{};
        _HasPendingMousePosition = false;
    }

    if (auto* Component = _HoveredResult.Component.Get();
        NOT ck::debug_draw::Is_SuppressedForStreamerMode() && ck::IsValid(Component))
    {
        constexpr auto PersistentLines = false;
        DrawDebugBox(
            World,
            Component->Bounds.Origin,
            Component->Bounds.BoxExtent,
            FColor::Cyan,
            PersistentLines,
            0.0f,
            SDPG_Foreground,
            1.5f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    OnMouseMoved(
        const FVector2D& InAbsolutePos) -> void
{
    if (NOT _IsActive)
    { return; }

    _PendingMousePosition = InAbsolutePos;
    _HasPendingMousePosition = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    OnMouseClicked(
        const FVector2D& InAbsolutePos) -> bool
{
    if (NOT _IsActive)
    { return false; }

    auto* World = DoResolveTargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }

    auto Origin = FVector::ZeroVector;
    auto Direction = FVector::ForwardVector;
    if (NOT DoDeproject(World, InAbsolutePos, Origin, Direction))
    { return false; }

    const auto Result = DoPickAtRay(World, Origin, Direction);
    if (NOT Result.IsValid())
    { return false; }

    Deactivate();
    if (_Params.OnComponentPicked)
    {
        _Params.OnComponentPicked(Result);
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_ViewportComponentPicker::OnEscapePressed() -> bool
{
    if (NOT _IsActive)
    { return false; }

    Deactivate();
    return true;
}

auto FCkDebug_ViewportComponentPicker::Get_CullRadius() const -> float
{
    return _CullRadius;
}

auto FCkDebug_ViewportComponentPicker::Set_CullRadius(float InValue) -> void
{
    _CullRadius = FMath::Clamp(
        InValue,
        ck_debug_viewport_component_picker::CullRadiusMin,
        ck_debug_viewport_component_picker::CullRadiusMax);
}

auto
    FCkDebug_ViewportComponentPicker::
    Get_HoveredResult() const -> const FCkDebug_ComponentPickResult&
{
    return _HoveredResult;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    TryIntersect_RayBounds(
        const FBox& InBox,
        const FVector& InOrigin,
        const FVector& InDirection) -> TOptional<float>
{
    if (NOT InBox.IsValid)
    { return {}; }

    auto TMin = 0.0;
    auto TMax = TNumericLimits<double>::Max();

    for (auto Axis = 0; Axis < 3; ++Axis)
    {
        const auto Direction = InDirection[Axis];
        if (FMath::IsNearlyZero(Direction))
        {
            if (InOrigin[Axis] < InBox.Min[Axis] || InOrigin[Axis] > InBox.Max[Axis])
            { return {}; }
            continue;
        }

        const auto InvDirection = 1.0 / Direction;
        auto T1 = (InBox.Min[Axis] - InOrigin[Axis]) * InvDirection;
        auto T2 = (InBox.Max[Axis] - InOrigin[Axis]) * InvDirection;
        if (T1 > T2)
        { Swap(T1, T2); }

        TMin = FMath::Max(TMin, T1);
        TMax = FMath::Min(TMax, T2);
        if (TMin > TMax)
        { return {}; }
    }

    return static_cast<float>(TMin);
}

// =====================================================================================================================

auto
    FCkDebug_ViewportComponentPicker::
    DoResolveTargetWorld() const -> UWorld*
{
    if (NOT _Params.Get_TargetWorld)
    { return nullptr; }

    auto* World = _Params.Get_TargetWorld();
    if (ck::Is_NOT_Valid(World))
    { return nullptr; }

    const auto IsSupportedWorld =
        World->WorldType == EWorldType::Editor ||
        World->WorldType == EWorldType::PIE ||
        World->WorldType == EWorldType::Game;
    return IsSupportedWorld ? World : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoCaptureMouseState(
        UWorld* InWorld) -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto* Viewport = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(Viewport))
    { return; }

    auto* Controller = InWorld->GetFirstPlayerController();
    auto Restore = FRestoreMouseState{};
    Restore.Viewport = Viewport;
    Restore.Controller = Controller;
    Restore.PriorCapture = Viewport->GetMouseCaptureMode();
    Restore.PriorLock = Viewport->GetMouseLockMode();
    Restore.PriorCursorVisible = ck::IsValid(Controller) ? Controller->bShowMouseCursor : false;
    _MouseStateToRestore = Restore;

    Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
    Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
    constexpr auto IgnoreInput = false;
    Viewport->SetIgnoreInput(IgnoreInput);
    if (ck::IsValid(Controller))
    {
        constexpr auto ShowMouseCursor = true;
        Controller->SetShowMouseCursor(ShowMouseCursor);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoRestoreMouseState() -> void
{
    if (NOT _MouseStateToRestore.IsSet())
    { return; }

    const auto& Restore = _MouseStateToRestore.GetValue();
    if (auto* Viewport = Restore.Viewport.Get(); ck::IsValid(Viewport))
    {
        Viewport->SetMouseCaptureMode(Restore.PriorCapture);
        Viewport->SetMouseLockMode(Restore.PriorLock);
    }
    if (auto* Controller = Restore.Controller.Get(); ck::IsValid(Controller))
    {
        Controller->SetShowMouseCursor(Restore.PriorCursorVisible);
    }
    _MouseStateToRestore.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoDeproject(
        UWorld* InWorld,
        const FVector2D& InAbsolutePos,
        FVector& OutOrigin,
        FVector& OutDirection) const -> bool
{
    return ck::DebugViewportView::Deproject(InWorld, InAbsolutePos, OutOrigin, OutDirection);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoPickAtRay(
        UWorld* InWorld,
        const FVector& InOrigin,
        const FVector& InDirection) const -> FCkDebug_ComponentPickResult
{
    if (ck::Is_NOT_Valid(InWorld) || InDirection.IsNearlyZero())
    { return {}; }

    auto BestResult = FCkDebug_ComponentPickResult{};
    auto BestDistance = TNumericLimits<float>::Max();
    const auto TraceEnd = InOrigin + (InDirection.GetSafeNormal() * _CullRadius);

    constexpr auto TraceComplex = true;
    auto Query = FCollisionQueryParams{TEXT("CkDebugger_ComponentPicker"), TraceComplex};
    Query.bReturnFaceIndex = true;

    auto Hits = TArray<FHitResult>{};
    InWorld->LineTraceMultiByChannel(Hits, InOrigin, TraceEnd, ECC_Visibility, Query);
    for (const auto& Hit : Hits)
    {
        if (Hit.Distance > _CullRadius || NOT DoIsCandidate(InWorld, Hit.GetComponent()))
        { continue; }

        BestResult = DoBuildTraceResult(Hit);
        BestDistance = Hit.Distance;
        break;
    }

    for (auto It = TObjectIterator<UPrimitiveComponent>{RF_ClassDefaultObject | RF_ArchetypeObject}; It; ++It)
    {
        auto* Component = *It;
        if (NOT DoIsCandidate(InWorld, Component))
        { continue; }

        const auto HitDistance = TryIntersect_RayBounds(
            Component->Bounds.GetBox(),
            InOrigin,
            InDirection.GetSafeNormal());
        if (NOT HitDistance.IsSet() || *HitDistance > _CullRadius || *HitDistance >= BestDistance)
        { continue; }

        BestDistance = *HitDistance;
        BestResult = {};
        BestResult.Component = Component;
        BestResult.ImpactPoint = InOrigin + (InDirection.GetSafeNormal() * *HitDistance);
        BestResult.Distance = *HitDistance;
        BestResult.UsedBoundsFallback = true;
    }

    return BestResult;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoIsCandidate(
        UWorld* InWorld,
        const UPrimitiveComponent* InComponent) const -> bool
{
    if (ck::Is_NOT_Valid(InWorld) || ck::Is_NOT_Valid(InComponent))
    { return false; }

    if (InComponent->IsTemplate() || InComponent->GetWorld() != InWorld || NOT InComponent->IsRegistered())
    { return false; }

    if (NOT InComponent->IsVisible() || InComponent->GetNumMaterials() <= 0 || InComponent->Bounds.SphereRadius <= 0.0f)
    { return false; }

    return NOT _Params.TargetFilter || _Params.TargetFilter(*InComponent);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_ViewportComponentPicker::
    DoBuildTraceResult(
        const FHitResult& InHit) const -> FCkDebug_ComponentPickResult
{
    auto Result = FCkDebug_ComponentPickResult{};
    auto* Component = InHit.GetComponent();
    if (ck::Is_NOT_Valid(Component))
    { return Result; }

    Result.Component = Component;
    Result.ImpactPoint = InHit.ImpactPoint;
    Result.Distance = InHit.Distance;
    Result.FaceIndex = InHit.FaceIndex;
    Result.InstanceIndex = InHit.Item;

    if (InHit.FaceIndex != INDEX_NONE)
    {
        auto SectionIndex = int32{INDEX_NONE};
        Result.CollisionMaterial = Component->GetMaterialFromCollisionFaceIndex(InHit.FaceIndex, SectionIndex);
        Result.CollisionSectionIndex = SectionIndex;
    }

    return Result;
}

// --------------------------------------------------------------------------------------------------------------------
