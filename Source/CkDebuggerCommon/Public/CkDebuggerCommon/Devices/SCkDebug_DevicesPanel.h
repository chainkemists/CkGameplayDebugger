#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The full devices row — keyboard, mouse and gamepad side by side over the suite's root background — as ONE shared
// panel, so every debugger that shows the player's hardware shows it identically. The explicit BgRoot backing is
// load-bearing: the device widgets' Bg1 body shells and Bg2 caps are authored against the root tone and go
// invisible on a host that paints Bg1 behind them.
//
// Consumers supply the snapshot; interactivity (click/tooltip) and the footnote line are optional.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DevicesPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_DevicesPanel)
        : _Snapshot(nullptr)
    {}
        SLATE_ATTRIBUTE(const FCkDebug_DeviceSnapshot*, Snapshot)
        SLATE_EVENT(FCkDebug_DeviceKeyClicked, OnKeyClicked)
        SLATE_EVENT(FCkDebug_DeviceKeyTooltip, KeyTooltip)

        // One muted line under the devices; empty collapses it.
        SLATE_ARGUMENT(FText, NoteText)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
