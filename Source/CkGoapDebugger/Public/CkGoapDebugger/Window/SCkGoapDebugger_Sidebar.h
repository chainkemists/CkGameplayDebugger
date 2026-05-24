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
// CkGoap Debugger — Sidebar (U11.7-B).
//
// Two stacked sections inside a vertical SSplitter (user-resizable):
//   - TOP    : STreeView of FCkGoapDebugger_PlannerInfo for the currently
//              selected entity. Recursive — each Planner row is a tree row;
//              its ChildPlanners are nested children. Role badges (PLANNER /
//              PLANNER + ACTION for dual-role) sit at the right of each row.
//   - BOTTOM : SListView of FCkGoapDebugger_HistoryEvent for the selected
//              entity. Same scrub/history behaviour as before; the splitter
//              between top and bottom is user-draggable.
//
// Row identity:
//   Source items are TSharedPtr<FRowItem> keyed by the underlying
//   FCk_Handle_Goap_Planner. The map across refreshes is preserved so that
//   STreeView selection & expansion state survives ticks. Only when the set
//   of planner handles changes do we call RequestTreeRefresh.
//
// Selection drives BOTH the new selection state and the legacy Planner
// selection (synthesized from the Planner) so the existing PrimaryPane /
// Breadcrumb / Graph keep working until U11.7-C/D retire them.
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

    // -----------------------------------------------------------------------------------------------------------------
    // Tree row item — a stable per-Planner record. The fields are updated
    // in place across refreshes so STreeView preserves selection identity.
    // Public so file-local helpers in the .cpp can spell its name.
    // -----------------------------------------------------------------------------------------------------------------

    struct FRowItem
    {
        FCk_Handle_Goap_Planner             PlannerHandle;
        FString                             DisplayName;
        FGameplayTag                        PlannerTag;
        bool                                IsActionRole       = false;
        bool                                IsInActiveChain    = false;
        ECk_GoapPlanStatus                  PlanStatus         = ECk_GoapPlanStatus::Idle;
        int32                               Depth              = 0;
        TArray<TSharedPtr<FRowItem>>        Children;
    };

    using FRowItemPtr = TSharedPtr<FRowItem>;

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Tree plumbing
    // -----------------------------------------------------------------------------------------------------------------

    auto GenerateRow(FRowItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto GetTreeChildren(FRowItemPtr InItem, TArray<FRowItemPtr>& OutChildren) -> void;
    auto OnSelectionChanged(FRowItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    // Rebuild _RootNodes from the currently selected entity's snapshot.
    // Reuses existing FRowItemPtr entries keyed by PlannerHandle to keep
    // STreeView selection / expansion stable. Returns true when the set of
    // planner handles changed (i.e. structural rebuild needed).
    auto RebuildTreeStructure() -> bool;

    // Synchronize STreeView selection with the ViewModel's selected planner.
    // No-op when already matching.
    auto SyncTreeSelectionFromViewModel() -> void;

    // Recursively re-expand all rows so the nested Planner forest is visible.
    auto ExpandAll(const TArray<FRowItemPtr>& InNodes) -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // History list plumbing
    // -----------------------------------------------------------------------------------------------------------------

    using FHistoryItemPtr = TSharedPtr<FCkGoapDebugger_HistoryEvent>;

    auto GenerateHistoryRow(FHistoryItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto RebuildHistoryItems() -> void;
    auto OnHistoryRowSelectionChanged(FHistoryItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    // ---- Scrub interaction ---------------------------------------------------
    auto SelectHistoryEvent(int32 InHistIdx) -> void;
    auto SyncHistoryListSelectionFromViewModel() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Header text helpers
    // -----------------------------------------------------------------------------------------------------------------

    auto GetPlannerTreeHeaderText() const -> FText;
    auto GetHistoryHeaderText() const -> FText;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<STreeView<FRowItemPtr>>      _TreeView;
    TArray<FRowItemPtr>                     _RootNodes;

    // Stable PlannerHandle -> FRowItemPtr map. Used to reuse existing
    // TSharedPtr identity across RebuildTreeStructure passes so STreeView
    // selection survives every Tick.
    TMap<FCk_Handle_Goap_Planner, FRowItemPtr> _RowItemsByHandle;

    TSharedPtr<SListView<FHistoryItemPtr>>   _HistoryListView;
    TArray<FHistoryItemPtr>                  _HistoryItems;

    // Stable per-event key -> FHistoryItemPtr map. Reused across
    // RebuildHistoryItems passes so SListView row identity (selection, hover,
    // widget reuse) is preserved when the history set has not structurally
    // changed. Mirrors the _RowItemsByHandle pattern used for the Planner tree.
    //
    // Key composition: (FrameNumber, Kind, HistIndex).
    // HistIndex is the event's position in the DataCollector's per-entity
    // history array. The DataCollector appends events; it never reorders, so
    // the index is stable for an event's entire lifetime in the buffer. The
    // (FrameNumber, Kind) pair alone is NOT unique — multiple events can fire
    // on the same frame with the same kind (e.g., the cluster of PlanFound
    // events emitted during initial Setup propagation). When that happens, a
    // collision-free key is essential to keep TSharedPtr identity unique per
    // event; otherwise SListView's WidgetMapToItem desyncs from its
    // ItemsWithGeneratedWidgets set and FWidgetGenerator::ValidateWidgetGeneration
    // fires on the next paint.
    using FHistoryKey = TTuple<int64, ECkGoapDebugger_HistoryEventKind, int32>;
    TMap<FHistoryKey, FHistoryItemPtr>       _HistoryItemsByKey;

    TSharedPtr<SCkGoapDebugger_ScrubTrack>   _ScrubTrack;

    // Selection-restore guard — suppress OnSelectionChanged echoes that
    // originate from a programmatic SetItemSelection during refresh.
    bool _SuppressSelectionEcho = false;

    // Structural-hash cache. Recompute every RefreshFromViewModel; rebuild
    // only when the value changes.
    uint32 _LastTreeStructureHash = 0;
    uint32 _LastHistoryHash       = 0;

    // Last name-depth a row set was generated at. A depth change forces a
    // full RebuildList (row Text widgets cached the display name at
    // construction time, RequestListRefresh alone would not redraw them).
    int32  _LastHistoryNameDepth  = -1;

    // Track the entity whose tree is currently materialized.
    FCk_Handle _MaterializedEntity;

    // Track the entity whose History list is currently materialized. On entity
    // change we hard-reset the history items + key map + hash so stale row
    // TSharedPtrs from the previous entity's events don't survive into the new
    // entity's SListView (which would trip ValidateWidgetGeneration's checkf
    // when the WidgetMap references items no longer in the source array).
    FCk_Handle _LastHistoryEntity;
};

// ====================================================================================================================
