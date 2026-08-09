#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Layer-stack view — "why did nothing happen when I pressed X".
//
// The stack top-down, each layer's declarative captures underneath it, and — for a layer carrying a matcher — the
// active-set summary and the physical keys the matcher registered for its terminals. A `Consume` capture above the
// layer you expected to fire IS the answer, structurally.
//
// Selecting a layer row sets the ViewModel's selected layer; the timeline, resolution and near-miss views all read
// that selection.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkIntentDebugger_StackNodeKind : uint8
{
    Layer,
    Capture,
    MatcherSummary,
    RegisteredKey
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkIntentDebugger_StackNode
{
    ECkIntentDebugger_StackNodeKind Kind = ECkIntentDebugger_StackNodeKind::Layer;

    int32 LayerPriority = 0;
    int32 ChildIndex = INDEX_NONE;

    int32 Indent = 0;
    FString Label;
    FString Detail;
    FLinearColor Tint = FLinearColor::White;

    auto Get_Key() const -> FString;
};

// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_LayerStackPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_LayerStackPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto RefreshFromViewModel() -> void;
    auto Reset_ForWorldChange() -> void;

private:
    auto OnGenerateRow(TSharedPtr<FCkIntentDebugger_StackNode> InNode, const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;

    auto OnSelectionChanged(TSharedPtr<FCkIntentDebugger_StackNode> InNode, ESelectInfo::Type InSelectInfo) -> void;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    TSharedPtr<SListView<TSharedPtr<FCkIntentDebugger_StackNode>>> _ListView;
    TArray<TSharedPtr<FCkIntentDebugger_StackNode>> _Nodes;
};

// --------------------------------------------------------------------------------------------------------------------
