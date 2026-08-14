#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Shared chassis for the procedural device visualizers (keyboard / mouse / gamepad): owns the snapshot attribute and
// the OPTIONAL interactivity — click-to-select and lazy hover tooltips — so each device widget only authors its
// layout table, its hit-test, and its paint. A consumer that binds neither delegate gets a purely passive visual;
// tooltips resolve lazily on hover and the cursor turns to a hand only over a clickable cap.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DeviceWidgetBase : public SLeafWidget
{
public:
    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    virtual auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    virtual auto OnMouseLeave(const FPointerEvent& InMouseEvent) -> void override;

protected:
    auto DoConstruct_DeviceCommon(
        const TAttribute<const FCkDebug_DeviceSnapshot*>& InSnapshot,
        const FCkDebug_DeviceKeyClicked& InOnKeyClicked,
        const FCkDebug_DeviceKeyTooltip& InKeyTooltip) -> void;

    // Which FKey's cap sits under the given local position; invalid when the point is bezel or gap.
    virtual auto Get_KeyAtPosition(const FGeometry& InGeometry, const FVector2D& InLocalPos) const -> FKey = 0;

protected:
    TAttribute<const FCkDebug_DeviceSnapshot*> _Snapshot;
    FCkDebug_DeviceKeyClicked _OnKeyClicked;
    FCkDebug_DeviceKeyTooltip _KeyTooltip;
    FKey _HoveredKey;
};

// --------------------------------------------------------------------------------------------------------------------
