#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Key / state view — one record row, read out.
//
// Held button set (with the key each button currently resolves to and whether this row is its press or release
// edge), the conditioned axis pair, the octant, the SOCD-cleaned cardinals, and the routing shape of the frame.
// The direction rosette lives here rather than in a view of its own: it is one more reading of the same row.
//
// Every value is bound through a TAttribute reading the ViewModel, so the tree is built once and nothing on the
// Tick path recreates a widget.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_KeyStatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_KeyStatePanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_Rosette() -> TSharedRef<SWidget>;
    auto Build_Readouts() -> TSharedRef<SWidget>;
    auto Build_ButtonPool() -> TSharedRef<SWidget>;
    auto Build_ButtonSlot(int32 InSlotIndex) -> TSharedRef<SWidget>;

    auto Get_ButtonAt(int32 InSlotIndex) const -> const struct FCkIntentDebugger_ButtonState*;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;
};

// --------------------------------------------------------------------------------------------------------------------
