#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

struct FCkDebug3dCursorRay
{
    FVector _Origin = FVector::ZeroVector;
    FVector _Direction = FVector::ForwardVector;
    bool _IsAdditiveSelection = false;
};

enum class ECkDebug3dPointerButton : uint8
{
    Left,
    Right,
    Middle,
};

enum class ECkDebug3dNeutralCommand : uint8
{
    TogglePause,
    StepOnce,
    ToggleIsolate,
};

struct FCkDebug3dInteractionModifiers
{
    auto
    HasAny() const -> bool
    {
        return _Control || _Shift || _Alt || _Command;
    }

    bool _Control = false;
    bool _Shift = false;
    bool _Alt = false;
    bool _Command = false;
};

struct FCkDebug3dInteractionHit
{
    uint64 _Identity = 0;
    FVector _SurfacePoint = FVector::ZeroVector;
    float _Distance = 0.0f;
};

class CKDEBUGGERCOMMON_API ICkDebug3dInteractionAdapter
{
  public:
    virtual ~ICkDebug3dInteractionAdapter() = default;
    virtual auto
    TryHit(const FCkDebug3dCursorRay& InRay) -> TOptional<FCkDebug3dInteractionHit> = 0;
    virtual auto
    Select(uint64 InIdentity, bool InAdditive) -> void = 0;
    virtual auto
    ClearSelection(bool InAdditive) -> void
    {
    }
    virtual auto
    CanArmDrag(const FCkDebug3dInteractionHit& InHit) const -> bool
    {
        return true;
    }
    virtual auto
    ArmDrag(const FCkDebug3dInteractionHit& InHit) -> void
    {
    }
    virtual auto
    UpdateDrag(const FCkDebug3dCursorRay& InRay) -> void
    {
    }
    virtual auto
    ReleaseDrag() -> void
    {
    }
    virtual auto
    ShiftDragPlane(float InDirection) -> void
    {
    }
    virtual auto
    SetHover(TOptional<uint64> InIdentity) -> void
    {
    }
    virtual auto
    Command(ECkDebug3dNeutralCommand InCommand) -> void
    {
    }
};

class CKDEBUGGERCOMMON_API FCkDebug3dInteractionConfig
{
  public:
    auto
    Set_ClickMovementThresholdPixels(float InPixels) -> FCkDebug3dInteractionConfig&;
    auto
    Set_HoverThrottleSeconds(double InSeconds) -> FCkDebug3dInteractionConfig&;
    auto
    Get_ClickMovementThresholdSquared() const -> double;
    auto
    Get_HoverThrottleSeconds() const -> double;

  private:
    float _ClickMovementThresholdPixels = 4.0f;
    double _HoverThrottleSeconds = 0.06;
};

class CKDEBUGGERCOMMON_API FCkDebug3dInteractionRouter final
{
  public:
    FCkDebug3dInteractionRouter(TSharedPtr<ICkDebug3dInteractionAdapter> InAdapter,
                                const FCkDebug3dInteractionConfig& InConfig);

    auto
    OnPointerPressed(ECkDebug3dPointerButton InButton, FVector2D InScreenPosition,
                     const FCkDebug3dInteractionModifiers& InModifiers, const FCkDebug3dCursorRay& InRay = {}) -> bool;
    auto
    OnPointerReleased(ECkDebug3dPointerButton InButton, FVector2D InScreenPosition,
                      const FCkDebug3dInteractionModifiers& InModifiers, const FCkDebug3dCursorRay& InRay = {}) -> bool;
    auto
    OnDragRay(const FCkDebug3dCursorRay& InRay) -> void;
    auto
    OnWheel(float InDelta, const FCkDebug3dInteractionModifiers& InModifiers) -> bool;
    auto
    OnKey(const FKey& InKey, const FCkDebug3dInteractionModifiers& InModifiers, double InCurrentTime) -> bool;
    auto
    TickHover(const FCkDebug3dCursorRay& InRay, const FCkDebug3dInteractionModifiers& InModifiers, double InCurrentTime)
        -> void;
    auto
    OnFocusLost() -> void;
    auto
    HasActiveGesture() const -> bool;
    auto
    HasActiveDrag() const -> bool;

  private:
    auto
    PublishHit(const FCkDebug3dCursorRay& InRay, bool InAdditive, bool InArmDrag) -> void;

    TWeakPtr<ICkDebug3dInteractionAdapter> _Adapter;
    FCkDebug3dInteractionConfig _Config;
    TOptional<FVector2D> _PendingLeftPress;
    TOptional<uint64> _HoverIdentity;
    double _LastHoverQueryTime = -TNumericLimits<double>::Max();
    bool _ControlGesture = false;
    bool _DragActive = false;
    bool _HoverWasPublished = false;
};
