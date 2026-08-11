#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The mouse, drawn procedurally beside the keyboard — same snapshot, same state vocabulary (flash on press, fill on
// hold toward the verdict, bright bezel when minted, dim when disconnected). Regions: left/right buttons, the wheel
// (middle click; scroll up/down render as flash-only nubs — they are impulse keys whose press and release land on
// one row), and the two thumb buttons on the left flank.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DeviceMouse : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_DeviceMouse)
        : _Snapshot(nullptr)
    {}
        SLATE_ATTRIBUTE(const FCkDebug_DeviceSnapshot*, Snapshot)
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

private:
    TAttribute<const FCkDebug_DeviceSnapshot*> _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
