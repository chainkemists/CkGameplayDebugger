#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Devices view — the player's physical hardware, drawn live.
//
// A thin producer hookup over the shared SCkDebug_DevicesPanel (CkDebuggerCommon): minted keys render with exact
// record history (bright bezel, press flash, hold fill toward the matcher's verdict point), unminted keys witnessed
// best-effort off the router's per-pass events. The shared panel is what keeps this view pixel-identical to the
// Input Debugger's devices section; a game HUD hosting the same panel is the intended future.
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
