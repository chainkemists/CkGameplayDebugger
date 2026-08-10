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
class SCkDebug_ScrubTimeline;

// ====================================================================================================================
// CkGoap Debugger — Sidebar (U11.7-B).
//
// Two stacked sections inside a vertical SSplitter (user-resizable):
//   - TOP    : STreeView of FCkGoapDebugger_PlannerInfo for the currently
//              selected entity. Recursive — each Planner row is a tree row;
//              its ChildPlanners are nested children. Role badges (PLANNER /
//              PLANNER + ACTION for dual-role) sit at the right of each row.
//   - BOTTOM : a shared SCkDebug_ScrubTimeline over the selected entity's
//              history (events → selectable Dot marks, flap storms → segments)
//              above an SListView of the same events. The splitter between top
//              and bottom is user-draggable.
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
    /**
     * Drop the rebuild debounce so the next refresh re-emits structure. The window calls this on a
     * style-revision bump: colours / fonts / paddings are attribute-bound and already live, but a
     * panel's STRUCTURE (which rows and slots exist at all) is composed once against the axes, so a
     * structural axis change needs the re-emit. Flipping the hash rather than zeroing it keeps a
     * genuine zero hash from swallowing the invalidation.
     */
    auto Invalidate_StyleCache() -> void
    {
        _LastTreeStructureHash = ~_LastTreeStructureHash;
        _LastHistoryHash = ~_LastHistoryHash;
        RefreshFromViewModel();
    }


    // The history block (header + scrub track + list), built in Construct but parented by the
    // window into a full-width bottom dock. The sidebar's own ChildSlot holds only the planner tree.
    auto Get_HistoryWidget() -> TSharedRef<SWidget>;

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
        // Opt-out flag from FFragment_Goap_Planner_Params::Get_AllowPlanFailed.
        // Drives the OPT-OUT pill on this row and the neutral status-dot
        // override when PlanStatus is PlanFailed.
        bool                                AllowPlanFailed    = false;
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

    // A flattened list row: a planner-group header, or a content row (single event or collapsed flap).
    struct FCkGoapDebugger_HistoryListEntry
    {
        bool    IsGroupHeader = false;
        FString PlannerName;                  // header text (when IsGroupHeader)
        FCk_Handle_Goap_Planner Planner;
        FCkGoapDebugger_HistoryRow Row;       // content row (when NOT a header)
        int32   RepHistIdx = INDEX_NONE;      // representative history index for scrub
        FString Key;                          // stable identity across rebuilds
    };
    using FHistoryItemPtr = TSharedPtr<FCkGoapDebugger_HistoryListEntry>;

    auto GenerateHistoryRow(FHistoryItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto RebuildHistoryItems() -> void;
    auto OnHistoryRowSelectionChanged(FHistoryItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;

    // ---- Copy / export -------------------------------------------------------
    auto OnHistoryContextMenu() -> TSharedPtr<SWidget>;
    auto BuildCopyText(const TArray<FHistoryItemPtr>& InItems) const -> FString;
    auto Get_PlannerDisplayName(const FCk_Handle_Goap_Planner& InPlanner) const -> FString;

    // ---- Scrub interaction ---------------------------------------------------
    auto SelectHistoryEvent(int32 InHistIdx) -> void;

    // Drag-scrub lands on a time, not an event — snap to the nearest recorded event so the panes
    // downstream keep receiving a concrete history index.
    auto SelectHistoryEventNearestTime(double InTimeSeconds) -> void;

    auto SyncHistoryListSelectionFromViewModel() -> void;

    // ---- Scrub track ---------------------------------------------------------
    // Push the current entity's history into the shared timeline as marks + flap segments.
    auto RebuildScrubContent() -> void;

    // "Now" (last recorded event) and the cursor time for the selected event.
    auto Get_LiveTimeSeconds() const -> double;
    auto Get_ScrubTimeSeconds() const -> double;

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

    // Built in Construct, parented by the window into the full-width bottom dock.
    TSharedPtr<SWidget>                      _HistorySection;

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
    using FHistoryKey = FString;
    TMap<FHistoryKey, FHistoryItemPtr>       _HistoryItemsByKey;

    TSharedPtr<SCkDebug_ScrubTimeline>       _ScrubTrack;

    // Entity whose history the scrub window was last framed against. The view window belongs to
    // the widget (and, after the first frame, to the user) — we only reset it on an entity switch.
    FCk_Handle _ScrubFramedEntity;

    // Content-hash of the last mark/segment push. Mode, cursor and selection ring are
    // attribute-bound inside the timeline, so only a grown recording or a moved selection re-pushes.
    uint32 _LastScrubHash = 0;

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
