#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Devices view — the player's physical hardware, drawn live.
//
// The full-size keyboard renders the ViewModel's device snapshot: minted keys with exact record history (bright
// bezel, press flash, hold fill toward the matcher's verdict point), unminted keys witnessed best-effort off the
// router's per-pass events. The widget itself lives in CkDebuggerCommon (Runtime) and consumes plain values only —
// this panel is the debugger-side producer hookup, and a game HUD hosting the same widget is the intended future.
//
// The gamepad joins in its own slice; the panel's layout already reserves the seat.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_DevicesPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_DevicesPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;
};

// --------------------------------------------------------------------------------------------------------------------
