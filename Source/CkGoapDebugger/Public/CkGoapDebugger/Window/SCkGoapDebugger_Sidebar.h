#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SListView.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class STextBlock;
class SBox;
class SCkGoapDebugger_ScrubTrack;

// ====================================================================================================================
// CkGoap Debugger — Sidebar.
//
// Two stacked sections:
//   - TOP    : STreeView of ActionSets for the currently selected entity. Each
//              ActionSet is a parent row, expandable; children are the ordered
//              chain (Root, Mid..., Leaf). Click-to-select drives the
//              ViewModel's ActionSet + Action selection.
//   - BOTTOM : SListView of FCkGoapDebugger_HistoryEvent for the selected
//              entity. Read-only for D2; D6 wires scrub interaction.
//
// Rebuild strategy:
//   - Tree structure is rebuilt only when the structural shape changes
//     (different entity / different ActionSet handles / chain handles).
//   - Per-row dynamic values (status badges, chain length text) bind via
//     TAttribute lambdas reading live from the ViewModel.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_Sidebar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_Sidebar) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkGoapDebugger_ViewModel> InViewModel) -> void;
    ~SCkGoapDebugger_Sidebar();

    // Drop cached row pointers / selection so the next world doesn't carry
    // stale FCk_Handle copies into a dead registry.
    auto Reset_ForWorldChange() -> void;

    // Called by the parent window on ViewModel OnChanged. Rebuilds tree
    // contents if the structural shape changed; otherwise relies on per-row
    // attribute lambdas.
    auto RefreshFromViewModel() -> void;

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Tree node — discriminated union between ActionSet rows and Action rows.
    // ActionSet rows expose a child-list (the ordered active chain); Action
    // rows have no children.
    // -----------------------------------------------------------------------------------------------------------------

    enum class ENodeKind : uint8
    {
        ActionSet,
        Action
    };

    struct FNode
    {
        ENodeKind Kind = ENodeKind::ActionSet;

        // ActionSet rows
        FCk_Handle_Goap_Planner ActionSetHandle;
        FString                   ActionSetDebugName;

        // Action rows
        FCk_Handle_Goap_Action    ActionHandle;
        FString                   ActionClassName;
        FString                   ActionTagText;
        ECkGoapDebugger_ActionRole Role = ECkGoapDebugger_ActionRole::Catalog;
        int32                     ChainDepth = -1;

        // Children only populated for ActionSet nodes (the ordered chain).
        TArray<TSharedPtr<FNode>> Children;
    };

    // -----------------------------------------------------------------------------------------------------------------
    // Tree plumbing
    // -----------------------------------------------------------------------------------------------------------------

    auto GenerateRow(TSharedPtr<FNode> InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto GetTreeChildren(TSharedPtr<FNode> InItem, TArray<TSharedPtr<FNode>>& OutChildren) -> void;
    auto OnSelectionChanged(TSharedPtr<FNode> InItem, ESelectInfo::Type InSelectInfo) -> void;

    // Rebuild _RootNodes from the currently selected entity's snapshot.
    // Returns true if the structural shape changed.
    auto RebuildTreeStructure() -> bool;

    // -----------------------------------------------------------------------------------------------------------------
    // History list plumbing
    // -----------------------------------------------------------------------------------------------------------------

    using FHistoryItemPtr = TSharedPtr<FCkGoapDebugger_HistoryEvent>;

    auto GenerateHistoryRow(FHistoryItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto RebuildHistoryItems() -> void;
    auto OnHistoryRowSelectionChanged(FHistoryItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    // ---- Scrub interaction ---------------------------------------------------
    // Reverse-order list maps to history indices via: histIdx = Hist.Num()-1-rowIdx.
    // Called by the scrub track (chronological hist index) and the events list
    // (chronological hist index translated from row).
    auto SelectHistoryEvent(int32 InHistIdx) -> void;

    // Push the scrub selection into the SListView selection so the row tint
    // matches the dot highlight.
    auto SyncHistoryListSelectionFromViewModel() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Header text helpers — Bind via TAttribute<FText> on header labels so we
    // don't have to rebuild the header on every refresh.
    // -----------------------------------------------------------------------------------------------------------------

    auto GetActionSetsHeaderText() const -> FText;
    auto GetHistoryHeaderText() const -> FText;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<STreeView<TSharedPtr<FNode>>> _TreeView;
    TArray<TSharedPtr<FNode>>                _RootNodes;

    TSharedPtr<SListView<FHistoryItemPtr>>   _HistoryListView;
    TArray<FHistoryItemPtr>                  _HistoryItems;

    TSharedPtr<SCkGoapDebugger_ScrubTrack>   _ScrubTrack;

    // Selection-restore guard — suppress OnSelectionChanged echoes that
    // originate from a programmatic SetItemSelection during refresh.
    bool _SuppressSelectionEcho = false;

    // Structural-hash cache. Recompute every RefreshFromViewModel; rebuild
    // only when the value changes.
    uint32 _LastTreeStructureHash = 0;
    uint32 _LastHistoryHash       = 0;

    // Track the entity whose tree is currently materialized.
    FCk_Handle _MaterializedEntity;
};

// ====================================================================================================================
