#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Resolution-table view — the selected layer's active compiled set, as the matcher reads it.
//
// One row per TERMINAL button: the intents a press of it can complete, already ordered most-dominant first by the
// bake, plus that button's deferral verdict. A blank deferral column is the normal state and the interesting one —
// it means the press is acted on the frame it arrives, which is the property the whole design exists to protect.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_ResolutionPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_ResolutionPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto RefreshFromViewModel() -> void;
    auto Reset_ForWorldChange() -> void;

private:
    auto OnGenerateRow(
        TSharedPtr<FCkIntentDebugger_ResolutionRow> InRow,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    TSharedPtr<SListView<TSharedPtr<FCkIntentDebugger_ResolutionRow>>> _ListView;
    TArray<TSharedPtr<FCkIntentDebugger_ResolutionRow>> _Rows;
};

// --------------------------------------------------------------------------------------------------------------------
