#include "CkDebuggerCommon/Viewport/CkDebug3dInteractionRouter.h"

auto
    FCkDebug3dInteractionConfig::
    Set_ClickMovementThresholdPixels(float InPixels)
    -> FCkDebug3dInteractionConfig&
{
    _ClickMovementThresholdPixels = FMath::Max(0.0f, InPixels);
    return *this;
}

auto
    FCkDebug3dInteractionConfig::
    Set_HoverThrottleSeconds(double InSeconds)
    -> FCkDebug3dInteractionConfig&
{
    _HoverThrottleSeconds = FMath::Max(0.0, InSeconds);
    return *this;
}

auto
    FCkDebug3dInteractionConfig::
    Get_ClickMovementThresholdSquared() const
    -> double
{
    return FMath::Square(static_cast<double>(_ClickMovementThresholdPixels));
}

auto
    FCkDebug3dInteractionConfig::
    Get_HoverThrottleSeconds() const
    -> double
{
    return _HoverThrottleSeconds;
}

FCkDebug3dInteractionRouter::FCkDebug3dInteractionRouter(TSharedPtr<ICkDebug3dInteractionAdapter> InAdapter,
                                                         const FCkDebug3dInteractionConfig& InConfig)
    : _Adapter(MoveTemp(InAdapter)), _Config(InConfig)
{
}

auto
    FCkDebug3dInteractionRouter::
    OnPointerPressed(ECkDebug3dPointerButton InButton, FVector2D InScreenPosition,
        const FCkDebug3dInteractionModifiers& InModifiers,
        const FCkDebug3dCursorRay& InRay)
    -> bool
{
    if (InButton != ECkDebug3dPointerButton::Left)
    {
        return false;
    }

    _PendingLeftPress = InScreenPosition;
    _ControlGesture = InModifiers._Control || InModifiers._Command;
    if (_ControlGesture)
    {
        constexpr auto IsAdditiveSelection = true;
        constexpr auto ArmDrag = true;
        PublishHit(InRay, IsAdditiveSelection, ArmDrag);
    }
    return true;
}

auto
    FCkDebug3dInteractionRouter::
    OnPointerReleased(ECkDebug3dPointerButton InButton, FVector2D InScreenPosition,
        const FCkDebug3dInteractionModifiers& InModifiers,
        const FCkDebug3dCursorRay& InRay)
    -> bool
{
    if (InButton != ECkDebug3dPointerButton::Left)
    {
        return false;
    }

    if (_DragActive)
    {
        if (const auto Adapter = _Adapter.Pin())
        {
            Adapter->ReleaseDrag();
        }
        _DragActive = false;
    }

    if (NOT _ControlGesture && _PendingLeftPress.IsSet() &&
        FVector2D::DistSquared(*_PendingLeftPress, InScreenPosition) <= _Config.Get_ClickMovementThresholdSquared())
    {
        constexpr auto IsAdditiveSelection = false;
        constexpr auto ArmDrag = false;
        PublishHit(InRay, IsAdditiveSelection, ArmDrag);
    }

    _PendingLeftPress.Reset();
    _ControlGesture = false;
    return true;
}

auto
    FCkDebug3dInteractionRouter::
    OnDragRay(const FCkDebug3dCursorRay& InRay)
    -> void
{
    if (NOT _DragActive)
    {
        return;
    }
    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->UpdateDrag(InRay);
    }
}

auto
    FCkDebug3dInteractionRouter::
    OnWheel(float InDelta, const FCkDebug3dInteractionModifiers& InModifiers)
    -> bool
{
    if (NOT _DragActive || NOT(InModifiers._Control || InModifiers._Command) || FMath::IsNearlyZero(InDelta))
    {
        return false;
    }
    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->ShiftDragPlane(InDelta > 0.0f ? 1.0f : -1.0f);
    }
    return true;
}

auto
    FCkDebug3dInteractionRouter::
    OnKey(const FKey& InKey, const FCkDebug3dInteractionModifiers& InModifiers,
        double InCurrentTime)
    -> bool
{
    if (InModifiers.HasAny())
    {
        return false;
    }

    auto Command = TOptional<ECkDebug3dNeutralCommand>{};
    if (InKey == EKeys::SpaceBar)
    {
        Command = ECkDebug3dNeutralCommand::TogglePause;
    }
    else if (InKey == EKeys::Enter)
    {
        Command = ECkDebug3dNeutralCommand::StepOnce;
    }
    else if (InKey == EKeys::I)
    {
        Command = ECkDebug3dNeutralCommand::ToggleIsolate;
    }
    if (NOT Command.IsSet())
    {
        return false;
    }

    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->Command(*Command);
    }
    return true;
}

auto
    FCkDebug3dInteractionRouter::
    TickHover(const FCkDebug3dCursorRay& InRay,
        const FCkDebug3dInteractionModifiers& InModifiers, double InCurrentTime)
    -> void
{
    if (HasActiveGesture() || InCurrentTime - _LastHoverQueryTime < _Config.Get_HoverThrottleSeconds())
    {
        return;
    }
    _LastHoverQueryTime = InCurrentTime;

    const auto Adapter = _Adapter.Pin();
    if (NOT Adapter.IsValid())
    {
        return;
    }
    const auto Hit = Adapter->TryHit(InRay);
    const auto Identity = Hit.IsSet() ? TOptional<uint64>{Hit->_Identity} : TOptional<uint64>{};
    if (NOT _HoverWasPublished || _HoverIdentity != Identity)
    {
        _HoverIdentity = Identity;
        _HoverWasPublished = true;
        Adapter->SetHover(Identity);
    }
}

auto
    FCkDebug3dInteractionRouter::
    OnFocusLost()
    -> void
{
    if (_DragActive)
    {
        if (const auto Adapter = _Adapter.Pin())
        {
            Adapter->ReleaseDrag();
        }
    }
    _DragActive = false;
    _ControlGesture = false;
    _PendingLeftPress.Reset();

    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->SetHover({});
    }
    _HoverIdentity.Reset();
    _HoverWasPublished = true;
}

auto
    FCkDebug3dInteractionRouter::
    HasActiveGesture() const
    -> bool
{
    return _PendingLeftPress.IsSet() || _DragActive;
}

auto
    FCkDebug3dInteractionRouter::
    HasActiveDrag() const
    -> bool
{
    return _DragActive;
}

auto
    FCkDebug3dInteractionRouter::
    PublishHit(const FCkDebug3dCursorRay& InRay, bool InAdditive, bool InArmDrag)
    -> void
{
    const auto Adapter = _Adapter.Pin();
    if (NOT Adapter.IsValid())
    {
        return;
    }
    const auto Hit = Adapter->TryHit(InRay);
    if (NOT Hit.IsSet())
    {
        Adapter->ClearSelection(InAdditive);
        return;
    }

    Adapter->Select(Hit->_Identity, InAdditive);
    if (InArmDrag && Adapter->CanArmDrag(*Hit))
    {
        Adapter->ArmDrag(*Hit);
        _DragActive = true;
    }
}
