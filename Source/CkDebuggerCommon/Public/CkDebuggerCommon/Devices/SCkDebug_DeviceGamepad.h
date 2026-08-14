#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"
#include "CkDebuggerCommon/Devices/SCkDebug_DeviceWidgetBase.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// The gamepad, drawn procedurally beside the keyboard and mouse — same snapshot, same state vocabulary (flash on
// press, fill on hold toward the verdict, bright bezel when minted, amber when rebound, bright ring when
// highlighted, dim when disconnected). Regions: triggers and bumpers up top, D-pad left, face buttons right,
// select/start center, the two stick buttons below — a producer that folds stick DEFLECTION into the stick-button
// key states makes the sticks light on movement, not only on click.
// Optional click/tooltip interactivity comes from the shared device base.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DeviceGamepad : public SCkDebug_DeviceWidgetBase
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_DeviceGamepad)
        : _Snapshot(nullptr)
    {}
        SLATE_ATTRIBUTE(const FCkDebug_DeviceSnapshot*, Snapshot)
        SLATE_EVENT(FCkDebug_DeviceKeyClicked, OnKeyClicked)
        SLATE_EVENT(FCkDebug_DeviceKeyTooltip, KeyTooltip)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

protected:
    virtual auto Get_KeyAtPosition(const FGeometry& InGeometry, const FVector2D& InLocalPos) const -> FKey override;
};

// --------------------------------------------------------------------------------------------------------------------
