#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Near-miss view — the matcher's scan-diagnostic ring, newest first.
//
// A failed backward scan leaves no trace anywhere else: the record shows the press, the phase rows show Idle, and
// no signal fires. This is the only surface that can say WHICH step had nothing behind it and by how many frames it
// missed — the `WindowExhausted` / `ContiguityBroken` / `NotSatisfied` split is three different fixes, so the step
// detail spells out which one and the frames the walk read while seeking it.
//
// EMPTY unless `ck.Intent.RecordScanDiagnostics` was on while the scans ran; the panel says so rather than looking
// like a matcher that never scanned.
// --------------------------------------------------------------------------------------------------------------------

// Row identity follows the SCAN, not its position in the ring. The ring is newest-first, so a position-keyed row
// would silently become a different scan under the user's selection the moment another one is recorded.
struct FCkIntentDebugger_NearMissRow
{
    FString Key;
    FCk_Intent_ScanDiagnostic Diagnostic;

    static auto Make_Key(const FCk_Intent_ScanDiagnostic& InDiagnostic) -> FString;
};

// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_NearMissPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_NearMissPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto RefreshFromViewModel() -> void;
    auto Reset_ForWorldChange() -> void;

private:
    auto OnGenerateRow(
        TSharedPtr<FCkIntentDebugger_NearMissRow> InRow,
        const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

    auto OnSelectionChanged(
        TSharedPtr<FCkIntentDebugger_NearMissRow> InRow,
        ESelectInfo::Type InSelectInfo) -> void;

    auto DoRebuild_StepDetail() -> void;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    TSharedPtr<SListView<TSharedPtr<FCkIntentDebugger_NearMissRow>>> _ListView;
    TArray<TSharedPtr<FCkIntentDebugger_NearMissRow>> _Rows;

    TSharedPtr<SVerticalBox> _StepDetail;

    FString _SelectedKey;
    uint32 _StepDetailHash = 0;
};

// --------------------------------------------------------------------------------------------------------------------
