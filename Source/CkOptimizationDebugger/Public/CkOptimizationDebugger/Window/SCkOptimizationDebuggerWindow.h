#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_CleanupCommands.h"
#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_ProfileCommands.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Window/SCkOptimizationSnapshotViewer.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Camera/CameraTypes.h"
#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkInspectorEditGuard;
class ITableRow;
class SEditableTextBox;
class STextBlock;
class SVerticalBox;
class SWidgetSwitcher;

// --------------------------------------------------------------------------------------------------------------------

/** One line of the findings list. The list is FLAT and carries its own group headers rather than being a tree: a
 *  finding group never nests, so a tree would buy an expander and nothing else, and a flat list keeps one stable
 *  row-identity map instead of a map plus a shape signature.
 *
 *  `Key` is what the row is reused BY across rebuilds — the finding's stable key for a finding line, and a
 *  `group|<check id>` key for a header line. The two spaces cannot collide: a stable key always contains the
 *  check id followed by `|` and a target, and no check id is literally `group`. */
struct FCkOptimizationDebugger_FindingListItem
{
    FString Key;

    bool IsGroupHeader = false;

    FName CheckId;
    ECkOptimizationDebugger_Severity Severity = ECkOptimizationDebugger_Severity::Minor;
    ECkOptimizationDebugger_Category Category = ECkOptimizationDebugger_Category::Mesh;

    // Header lines only: how many findings the group holds.
    int32 GroupCount = 0;

    // Header lines carry the group's title here too, so a row generator never has to branch on which field to read.
    FString Title;

    // Meaningful on finding lines only.
    FCkOptimizationDebugger_FindingRow Finding;

    // Whether the Highlight query names this line. False draws it muted — highlight dims, it never hides.
    bool IsHighlightMatch = true;

    /** Header lines only: whether the group is folded. A folded group still emits its HEADER — only its finding
     *  lines are left out of the item source — so the count, the worst-severity glyph and the staged-count pill on
     *  that header are what stop a fold from reading as findings disappearing. */
    bool IsCollapsed = false;

    /** Header lines only: every stable key the group holds, so the header's checkbox and its Select-all entry can
     *  stage the whole group without the row generator re-deriving the membership the rebuild already walked.
     *
     *  It holds the group's VISIBLE findings, which is what the header's own count describes. A header that staged
     *  rows the current filter excludes would queue work the reader cannot see to inspect. */
    TArray<FString> GroupStableKeys;

    // Header lines only: how many of `GroupStableKeys` are staged. Drives the tri-state checkbox and the pill.
    int32 GroupQueuedCount = 0;

    // Finding lines only: whether this finding is staged in the fix queue.
    bool IsQueued = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** One line of the cleanup list. Flat with its own group headers, exactly like the findings list and for the same
 *  reason: a duplicate group never nests, so a tree would buy an expander and nothing else.
 *
 *  `Key` is what the row is reused BY across rebuilds — `<category id>|<asset path>` for a row and
 *  `group|<duplicate key>` for a header. The category is part of a row's key because one asset can legitimately
 *  appear under two categories, and a key that dropped it would make the second appearance replace the first. */
struct FCkOptimizationDebugger_CleanupListItem
{
    FString Key;

    bool IsGroupHeader = false;

    // Header lines: the duplicated asset's name. Row lines: the row's own display name, kept here too so a row
    // generator never has to branch on which field to read.
    FString Title;

    // Header lines only: how many assets the group holds, and what keeping ONE of them would free.
    int32 GroupCount = 0;
    int64 GroupReclaimableBytes = 0;

    // Meaningful on row lines only.
    FCkOptimizationDebugger_CleanupRow Row;
};

// --------------------------------------------------------------------------------------------------------------------

/** The offline optimization toolkit: level-analysis findings, a dashboard over them, a memory analyzer, a profiling
 *  launcher, and a project-cleanup pass. Five pages behind one page bar.
 *
 *  There is no world, no PIE session and no ECS registry behind this window. Everything it shows is derived from
 *  assets, editor levels and project settings by an explicit user-driven scan, so it holds no `FCk_Handle` and needs
 *  neither the `EndPIE` clear nor the `OnEnginePreExit` teardown that handle-holding debuggers require — see the
 *  module CLAUDE.md's no-live-handle invariant.
 *
 *  It DOES subscribe to the shared session-invalidated notification, for a different reason: an actor-targeted
 *  finding names an object path inside a world, and entering or leaving PIE swaps the world those paths resolve
 *  against. Findings are dropped at that boundary rather than left pointing at a world that no longer exists.
 *
 *  Consequence for refresh: this window never polls, and therefore does NOT override `Tick`. Every rebuild is driven
 *  by an explicit event — scan, filter, page switch — so the base's gated style-revision watch is the only per-tick
 *  work here. */
class SCkOptimizationDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkOptimizationDebuggerWindow) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    virtual ~SCkOptimizationDebuggerWindow() override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override
    { return FText::FromString(TEXT("CK Optimization Debugger")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    using FFindingItem = TSharedPtr<FCkOptimizationDebugger_FindingListItem>;

    // The memory tables list the model's plain row struct directly — unlike a finding line, a memory row needs no
    // group headers and no highlight bit, so there is nothing to wrap it in.
    using FMemoryItem = TSharedPtr<FCkOptimizationDebugger_MemoryRow>;

    // One row of the snapshot mesh list. Shared pointers rather than values so the list keeps ITEM IDENTITY across a
    // refresh — an SListView compares items by pointer, and rebuilt values would drop the reader's selection every
    // time a sort or a recapture rebuilt the table.
    using FSnapshotMeshItem = TSharedPtr<ck_optimization_debugger_snapshot_lens::FCkOptimizationDebugger_SnapshotMeshRow>;

    // The cleanup list DOES wrap, because the duplicates category carries group headers.
    using FCleanupItem = TSharedPtr<FCkOptimizationDebugger_CleanupListItem>;

    // ---- Chrome construction ----
    auto DoCreate_MenuActions() -> TSharedRef<SWidget>;
    auto DoCreate_Toolbar() -> TSharedRef<SWidget>;
    auto DoCreate_Body() -> TSharedRef<SWidget>;
    auto DoCreate_Status() -> TSharedRef<SWidget>;
    auto DoCreate_PageTabs() -> TSharedRef<SWidget>;

    // ---- Pages. Switcher slots are added in ECkOptimizationDebugger_Page order; Get_PageIndex depends on it. ----
    auto DoCreate_DashboardPage() -> TSharedRef<SWidget>;
    auto DoCreate_FindingsPage() -> TSharedRef<SWidget>;
    auto DoCreate_MemoryPage() -> TSharedRef<SWidget>;
    auto DoCreate_ProfilingPage() -> TSharedRef<SWidget>;
    auto DoCreate_CleanupPage() -> TSharedRef<SWidget>;
    auto DoCreate_SnapshotsPage() -> TSharedRef<SWidget>;

    auto DoCreate_CategoryFilters() -> TSharedRef<SWidget>;
    auto DoCreate_ScopeFilters() -> TSharedRef<SWidget>;

    // ---- Dashboard sections. Each returns a finished sub-tree; DoRebuild_Dashboard slots them in order. ----
    auto DoBuild_DashboardEmptyState() -> TSharedRef<SWidget>;
    auto DoBuild_DashboardTiles() -> TSharedRef<SWidget>;
    auto DoBuild_DashboardSeverityStrip() -> TSharedRef<SWidget>;
    auto DoBuild_DashboardDiskSection() -> TSharedRef<SWidget>;
    auto DoBuild_DashboardLevelsSection() -> TSharedRef<SWidget>;
    auto DoBuild_DashboardThresholdsSection() -> TSharedRef<SWidget>;

    // ---- Commands ----
    auto DoOnScanClicked() -> FReply;
    auto Get_CanScan() const -> bool;

    /** Narrows the findings page to one severity and switches to it — what a dashboard severity badge does. The
     *  state change itself lives on the model (`Set_SeveritySolo`) so a spec can drive it without a widget. */
    auto DoSolo_Severity(ECkOptimizationDebugger_Severity InSeverity) -> void;

    /** Flips a level's inclusion and persists the whole set. Nothing is re-rendered and nothing re-scans: the row's
     *  colour, its hint and its switch are all `TAttribute`s over the model, so the greying is immediate without a
     *  rebuild — and the next SCAN is what honours the exclusion. */
    auto DoSet_LevelExcluded(FName InLevelName, bool InExcluded) -> void;

    auto DoOnGoToClicked() -> FReply;
    auto DoOnOpenAssetClicked() -> FReply;
    auto DoOnCaptureSnapshotClicked() -> FReply;
    auto DoOnRecaptureSnapshotClicked() -> FReply;
    auto DoOnSnapshotReportClicked() -> FReply;
    auto DoOnSnapshotSaveClicked() -> FReply;
    auto DoOnSnapshotLoadClicked() -> FReply;
    auto DoOnCycleSnapshotClicked(int32 InDelta) -> FReply;
    auto DoOnDeleteSnapshotClicked() -> FReply;
    auto DoOnApplyFixClicked() -> FReply;

    /** The one scan path. `DoOnScanClicked` and the post-fix refresh both go through it, so a fix cannot end up
     *  refreshing the list by some second route that forgets to drop the row-identity map. Returns whether a scan
     *  actually ran — outside an editor session it does not. */
    auto DoRun_Scan() -> bool;

    /** The findings the list has selected, group headers dropped. Falls back to the detail panel's own finding when
     *  the list reports nothing, which is the state a rebuild leaves behind. */
    auto Get_SelectedFindings() const -> TArray<FCkOptimizationDebugger_FindingRow>;

    auto DoNavigate_ToSelected() -> void;
    auto DoNavigate_To(const FCkOptimizationDebugger_FindingRow& InFinding) -> void;

    /** Opens the selected finding's asset in its own editor. One target even from a multi-selection, for the same
     *  reason navigation takes one: opening N asset editors from one click is not what the reader asked for. */
    auto DoOpenAsset_Selected() -> void;
    /** By value, not by reference: it is bound as a context-menu delegate PAYLOAD, and the payload binding
     *  requires the decayed type (same reason `DoOnMemoryDoubleClicked` takes its item by value). */
    auto DoOpenAsset_ForTarget(FCkOptimizationDebugger_Target InTarget) -> void;
    auto DoApply_FixToSelected() -> void;

    /** Applies every automatic fix the CURRENT FILTER admits. Not the whole scan — see the implementation for why
     *  ignoring the filter would act on rows the reader muted. */
    auto DoApply_FixAllVisible() -> void;

    /** The one apply pipeline both entry points share: the empty check, the session gate, the confirmation, the
     *  single-vs-batch split, the post-fix re-scan and the status line. `InScopeLabel` is what the "nothing here has
     *  a fix" message calls the set, so it can say which set it means. */
    auto DoApply_Fixes(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFixable,
        const FString& InScopeLabel) -> void;

    /** Mutes or unmutes every non-header row in the selection, persists the set, and rebuilds. The direction is
     *  decided by the CALLER from what the selection already is, so the menu's verb and the effect cannot
     *  disagree on a mixed selection. */
    auto DoToggle_MuteOnSelected(bool InMute) -> void;

    // ---- Collapse ----

    /** Folds or unfolds one check's group and persists the set. Rebuilds the list; changes no count. */
    auto DoSet_CheckCollapsed(FName InCheckId, bool InCollapsed) -> void;

    auto DoSet_AllChecksCollapsed(bool InCollapsed) -> void;

    // ---- Fix queue ----

    /** Stages or unstages a set of findings and rebuilds. One entry point for the row checkbox, the group checkbox
     *  and the context menu, so the three cannot drift on what staging means. */
    auto DoSet_Queued(const TArray<FString>& InStableKeys, bool InQueued) -> void;

    /** Stages every finding in the selection, or unstages them when the selection is already wholly staged — the
     *  same "the verb follows the whole selection" rule the mute menu holds to. */
    auto DoToggle_QueueOnSelected(bool InQueued) -> void;

    auto DoClear_Queue() -> void;

    /** Applies the queue. Runs the SAME pipeline the selection apply does, so the confirmation, the batch
     *  partition, the transaction and the post-fix re-scan are shared rather than re-implemented per entry point. */
    auto DoApply_Queue() -> void;

    /** The tray body: one section per check, its fix verb as the heading, with a remove control per entry. Rebuilt
     *  in place — a plain box rather than a list, because the queue is small and bounded by what a reader staged by
     *  hand, and a plain box is what makes the per-entry remove button legal in a row at all. */
    auto DoRebuild_FixQueue() -> void;

    /** Recomputes what the queue's apply button says and whether it is enabled — the queue's twin of
     *  `DoRefresh_SelectionCommands`, and never called from an attribute lambda for the same reason. */
    auto DoRefresh_QueueCommands() -> void;

    auto DoCreate_FixQueuePanel() -> TSharedRef<SWidget>;

    // ---- Rebuild entry points (never called from Tick) ----

    /** `InRefreshStatus == false` leaves the status strip alone. A style revision and a session invalidation both
     *  fan out through here, and both have something of their own to say afterwards — without the opt-out the
     *  findings line overwrote it, so triggering a Style Lab revision after a cleanup scan reset the strip to
     *  "No scan yet." */
    auto DoRebuild_All(bool InRefreshStatus = true) -> void;

    auto DoRebuild_Dashboard() -> void;
    auto DoRebuild_Findings(bool InRefreshStatus = true) -> void;
    auto DoRebuild_FindingDetail() -> void;
    auto DoRefresh_Status() -> void;

    /** Recomputes what the action buttons say and whether they are enabled. Called from every selection change and
     *  every findings rebuild — never from an attribute lambda, because an attribute that re-derived the selection
     *  would do it on the paint path, every frame. */
    auto DoRefresh_SelectionCommands() -> void;

    /** Re-points the viewer, refills the strip and refreshes the tab count and the facts panel. Event-driven only —
     *  capture, cycle, delete, strip click. The window overrides no `Tick`, and this is not a place to start. */
    auto DoRebuild_Snapshots() -> void;

    /** One synchronous capture, from the button. Everything it can fail at is reported on the status strip.
     *
     *  The override is the stored point of view of the snapshot on screen, and only "Recapture from here" passes
     *  one: replaying a POV is what makes two captures of one framing comparable pixel for pixel after the world
     *  moved. Unset is the live camera, which is every other capture. */
    auto DoRun_SnapshotCapture(TOptional<FMinimalViewInfo> InViewOverride) -> void;

    /** The right-hand panel: capture facts when nothing is selected, one mesh's LODs and material slots when one
     *  is, an aggregate when several are. Rebuilt on SELECTION change and snapshot switch — never on hover, which
     *  would rebuild a panel of rows on every mouse move across a mesh boundary. */
    auto DoRebuild_SnapshotDetail() -> void;

    auto DoOnToggle_SnapshotSelectionMode() -> void;

    /** The one control that decides which of the snapshot's images is on screen. Building the menu here rather than
     *  from an attribute means the auxiliary entries reflect the ACTIVE snapshot at the moment it is opened. */
    auto DoCreate_SnapshotViewMenu() -> TSharedRef<SWidget>;

    /** Pushes the chosen view and the current budgets into the viewer, then refreshes the legend line. The budgets
     *  are read HERE — a lens is a measurement, and nothing that computes one may reach for the settings CDO. */
    auto DoSelect_SnapshotView(FCkOptimizationDebugger_SnapshotView InView) -> void;

    /** What the selector button reads. Auxiliary views print their stored NAME, so a snapshot carrying views this
     *  build has never heard of still labels them honestly. */
    auto DoGet_SnapshotViewLabel(const FCkOptimizationDebugger_SnapshotView& InView) const -> FString;

    /** Rebuilds the mesh-list rows from the ACTIVE snapshot, the coverage the viewer already counted and the current
     *  budgets, then re-applies the sort and pushes the selection back down. Event-driven, like everything else on
     *  this page: capture, cycle, delete, sort and selection — never a tick. */
    auto DoRebuild_SnapshotMeshList() -> void;

    /** Pushes the snapshot's selection INTO the list without letting the list report it back as a user action. The
     *  guard is the whole point: image and list are one selection, and two widgets that echo each other turn one
     *  click into an infinite refresh. */
    auto DoSync_SnapshotMeshSelection() -> void;

    auto DoCreate_SnapshotMeshHeaderRow() -> TSharedRef<SHeaderRow>;

    auto DoGenerate_SnapshotMeshRow(FSnapshotMeshItem InItem, const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;

    auto DoOnSnapshotMeshSelectionChanged(FSnapshotMeshItem InItem, ESelectInfo::Type InSelectInfo) -> void;

    auto DoOnSnapshotMeshSortChanged(EColumnSortPriority::Type InPriority, const FName& InColumnId,
        EColumnSortMode::Type InSortMode) -> void;

    auto Get_SnapshotMeshSortMode(
        ck_optimization_debugger_snapshot_lens::ECkOptimizationDebugger_SnapshotMeshColumn InColumn) const
        -> EColumnSortMode::Type;

    /** The other stored snapshots, as things to compare the one on screen AGAINST. Built when opened so it lists
     *  what exists now rather than what existed when the page was constructed. */
    auto DoCreate_SnapshotCompareMenu() -> TSharedRef<SWidget>;

    /** `INDEX_NONE` stops comparing. Anything else names the BASELINE — the older capture the one on screen is read
     *  as a change from. */
    auto DoSelect_SnapshotCompare(int32 InBaselineIndex) -> void;

    /** Copy paths, and nothing that mutates: the mesh list is a view of a stored capture, so there is no action here
     *  that could act on the world it pictured. */
    auto DoBuild_SnapshotMeshContextMenu() -> TSharedPtr<SWidget>;
    auto DoOnSnapshotHoveredPrimChanged(TOptional<int32> InPrimIndex) -> void;
    auto DoOnSnapshotPrimClicked(TOptional<int32> InPrimIndex,
        ECkOptimizationDebugger_SnapshotClickModifier InModifier) -> void;

    // By value, like every other payload-bound handler on this window — the delegate binding requires the decayed
    // type (see `DoOpenAsset_ForTarget`).
    auto DoOnSnapshotGoToClicked(FCkOptimizationDebugger_Target InTarget) -> FReply;
    auto DoOnSnapshotOpenAssetClicked(FCkOptimizationDebugger_Target InTarget) -> FReply;

    auto DoSelect_Page(ECkOptimizationDebugger_Page InPage) -> void;

    /** Bounds-checked reads of the cached per-table / per-category counts. A tab whose enum value has outrun the
     *  cached array reads 0 rather than indexing past it — the arrays are sized from the projections' own lists, so
     *  that can only happen between a value being added and the first rebuild after it. */
    auto Get_CachedMemoryCount(ECkOptimizationDebugger_MemoryTable InTable) const -> int32;
    auto Get_CachedCleanupCount(ECkOptimizationDebugger_CleanupCategory InCategory) const -> int32;
    auto Get_CachedCategoryCount(ECkOptimizationDebugger_Category InCategory) const -> int32;

    /** One-shot: runs a dashboard rebuild the threshold edit guard held back, on the frame AFTER the edit ended.
     *  Rebuilding inline would destroy the text box whose commit handler is still on the stack. Not a poll — it
     *  fires once, from a user action, and unregisters itself. */
    auto DoOnDeferredDashboardRebuild(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;

    // ---- Findings list ----
    auto DoGenerate_FindingRow(FFindingItem InItem, const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;
    auto DoOnFindingSelectionChanged(FFindingItem InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto DoOnFindingContextMenu() -> TSharedPtr<SWidget>;

    /** The menu a right-click on group HEADERS opens: stage, fix, mute and solo over whole checks. Separate from
     *  the finding menu because every verb in it is scoped to a check rather than to a row, and one menu offering
     *  both would have to say which of the two each entry meant. */
    auto DoBuild_GroupContextMenu(const TArray<FFindingItem>& InHeaders) -> TSharedPtr<SWidget>;
    auto DoOnFindingDoubleClicked(FFindingItem InItem) -> void;

    auto TryGet_SelectedFinding() const -> const FCkOptimizationDebugger_FindingRow*;

    // ---- Memory analyzer ----
    /** The memory scan is cheap next to the level scan — one pass over resident objects, nothing loaded — but it is
     *  still an explicit user action. Nothing about this page polls. */
    auto DoOnMemoryRefreshClicked() -> FReply;
    auto DoRun_MemoryScan() -> void;

    /** Refills the active table's item source through the stable-identity pipeline and re-caches the totals the
     *  header's attributes read. Never called from a Tick — scan, filter, sort and table switch, and nothing else. */
    auto DoRebuild_Memory() -> void;

    auto DoSelect_MemoryTable(ECkOptimizationDebugger_MemoryTable InTable) -> void;

    /** Slate's own header-row sort callback. The clicked column becomes the sort column; clicking the ACTIVE column
     *  is what toggles the direction, and Slate hands the new mode in rather than asking us to derive it. */
    auto DoOnMemorySortChanged(EColumnSortPriority::Type InPriority, const FName& InColumnId,
        EColumnSortMode::Type InSortMode) -> void;

    auto Get_MemorySortMode(ECkOptimizationDebugger_MemoryColumn InColumn) const -> EColumnSortMode::Type;

    auto DoGenerate_MemoryRow(FMemoryItem InItem, const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;
    auto DoOnMemoryContextMenu() -> TSharedPtr<SWidget>;
    auto DoOnMemoryDoubleClicked(FMemoryItem InItem) -> void;

    auto DoCreate_MemoryTableTabs() -> TSharedRef<SWidget>;
    auto DoCreate_MemoryHeader() -> TSharedRef<SWidget>;
    auto DoCreate_MemoryHeaderRow() -> TSharedRef<SHeaderRow>;

    // ---- Profiling launcher ----
    /** One shelf: a section header plus a wrapping row of that group's entries. Built once per group when the page
     *  is created — nothing here is rebuilt, because every entry's on/off is an attribute reading live viewport
     *  state rather than a record this window keeps. */
    auto DoBuild_ProfileGroupSection(ECkOptimizationDebugger_ProfileGroup InGroup) -> TSharedRef<SWidget>;

    /** The escape hatch: a one-line console box pinned under the shelves, plus the last few commands as re-run
     *  chips. Deliberately NOT inside the scroll box — a reader who scrolled to the bottom shelf must not lose the
     *  box they are typing into. */
    auto DoBuild_ProfileCustomCommandPanel() -> TSharedRef<SWidget>;

    auto DoExecute_ProfileCommand(const FCkOptimizationDebugger_ProfileCommand& InCommand) -> void;
    auto DoOnProfileCommandClicked(FName InCommandId) -> FReply;

    auto DoOnRunCustomCommandClicked() -> FReply;
    auto DoOnCustomCommandCommitted(const FText& InText, ETextCommit::Type InCommitType) -> void;
    auto DoRun_CustomCommand() -> void;

    /** Refills the recent-command rail — the ONE piece of derived state this page holds. Every other control on it
     *  binds an attribute straight to the viewport, which is why there is no wider profiling rebuild. */
    auto DoRebuild_Profiling() -> void;

    // ---- Project cleanup ----
    /** The cleanup pass is its own explicit action, independent of the level scan in both directions: it asks about
     *  ASSETS, and re-walking `/Game` every time somebody analyzed a level would be a second question nobody asked. */
    auto DoOnCleanupScanClicked() -> FReply;
    auto DoRun_CleanupScan() -> void;

    /** Refills the active category's item source through the stable-identity pipeline and re-caches the totals the
     *  header's attributes read. Never called from a Tick — scan, filter, category switch, and after an action. */
    auto DoRebuild_Cleanup() -> void;

    auto DoSelect_CleanupCategory(ECkOptimizationDebugger_CleanupCategory InCategory) -> void;

    /** Recomputes what the action button says and whether it is enabled. Called from every selection change and every
     *  cleanup rebuild — never from an attribute lambda, for the same reason `DoRefresh_SelectionCommands` is not. */
    auto DoRefresh_CleanupCommands() -> void;

    auto Get_SelectedCleanupRows() const -> TArray<FCkOptimizationDebugger_CleanupRow>;

    /** The action the ACTIVE category is acted on by, or null when it has none. One button per category rather than
     *  three that mostly disable. */
    auto TryGet_ActiveCleanupAction() const -> const FCkOptimizationDebugger_CleanupActionInfo*;

    auto DoOnCleanupActionClicked() -> FReply;
    auto DoRun_CleanupAction() -> void;

    auto DoNavigate_ToCleanupRow(const FCkOptimizationDebugger_CleanupRow& InRow) -> void;
    auto DoNavigate_ToSelectedCleanupRow() -> void;

    auto DoGenerate_CleanupRow(FCleanupItem InItem, const TSharedRef<STableViewBase>& InOwnerTable)
        -> TSharedRef<ITableRow>;
    auto DoOnCleanupSelectionChanged(FCleanupItem InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto DoOnCleanupContextMenu() -> TSharedPtr<SWidget>;
    auto DoOnCleanupDoubleClicked(FCleanupItem InItem) -> void;

    auto DoCreate_CleanupHeader() -> TSharedRef<SWidget>;
    auto DoCreate_CleanupCategoryTabs() -> TSharedRef<SWidget>;

    // ---- Helpers ----
    auto DoSet_Status(const FString& InText, ECk_Tone InTone) -> void;
    auto DoOnSessionInvalidated() -> void;

private:
    FCkOptimizationDebugger_Model _Model;

    TSharedPtr<SWidgetSwitcher> _PageSwitcher;

    TSharedPtr<SVerticalBox> _DashboardBox;
    TSharedPtr<SVerticalBox> _FindingDetailBox;

    // The fix queue's own body, rebuilt in place by DoRebuild_FixQueue.
    TSharedPtr<SVerticalBox> _FixQueueBox;

    TSharedPtr<SListView<FFindingItem>> _FindingList;
    TArray<FFindingItem> _FindingItems;

    // Stable row identity: SListView tracks selection by pointer, and a re-scan that reproduces a finding must not
    // move the user's selection. Keyed by FCkOptimizationDebugger_FindingListItem::Key.
    //
    // It holds every VISIBLE finding, including the ones inside folded groups, which `_FindingItems` deliberately
    // does not — see the rebuild for why the identity cache and the rendered source are different sets.
    TMap<FString, FFindingItem> _FindingItemsByKey;

    // The rendered key sequence as of the last rebuild. Folding a group changes what the list draws without changing
    // what the identity map holds, so the refresh trigger has to compare the sequence — the same rule the memory
    // list follows, where a sort reorders the rows without touching the set.
    TArray<FString> _FindingRowOrder;

    // The stable key of the selected FINDING (never a group header), so a rebuild can restore the selection the
    // user made rather than the pointer that happened to hold it.
    FString _SelectedFindingKey;

    TSharedPtr<STextBlock> _StatusText;

    ECk_Tone _StatusTone = ECk_Tone::Neutral;

    // What the action buttons bind to. Derived state, refreshed by `DoRefresh_SelectionCommands` on selection and
    // rebuild — the buttons' attributes read these fields and never walk the selection themselves.
    bool _HasSelectedFinding = false;
    bool _OpenAssetButtonEnabled = false;

    // The Snapshots tab's badge. Cached for the same reason every other tab count is: the tab bar sits outside the
    // page switcher, so its attributes run on every page on every frame.
    int32 _SnapshotCount = 0;

    // Input-routing state, so it lives on the WINDOW rather than in the model — it is not something a snapshot has,
    // it is something the reader is currently doing.
    bool _SnapshotSelectionMode = false;

    // Which image the viewer is drawing, and what its colours mean. The legend is CACHED rather than recomputed in
    // the text attribute, which is evaluated on the paint path.
    FCkOptimizationDebugger_SnapshotView _SnapshotView;
    FString _SnapshotLensLegend;

    // The mesh list: the same selection the picture carries, in a form that can be sorted. Collapsed by default so
    // the page opens as a picture, which is what a reader came to the Snapshots tab for.
    bool _SnapshotMeshListVisible = false;

    // Set while the window is writing the selection INTO the list, so the list's own change callback can tell a
    // programmatic push from a click and refuse to echo it back.
    bool _SnapshotSelectionSyncGuard = false;

    ck_optimization_debugger_snapshot_lens::ECkOptimizationDebugger_SnapshotMeshColumn _SnapshotMeshSortColumn =
        ck_optimization_debugger_snapshot_lens::ECkOptimizationDebugger_SnapshotMeshColumn::Triangles;

    bool _SnapshotMeshSortAscending = false;

    // The snapshot the one on screen is compared AGAINST, by index into the model's array, or INDEX_NONE. An index
    // rather than a copy, because the strip has to mark that chip and the model owns the array.
    int32 _SnapshotCompareIndex = INDEX_NONE;

    bool _SnapshotSoloMode = false;

    // What the readout under the image says. Cached like every other attribute-read string on this window.
    FString _SnapshotHoverText;
    int32 _SelectedFixableCount = 0;
    int32 _VisibleFixableCount = 0;
    bool _FixButtonEnabled = false;
    FString _FixButtonLabel = FString{TEXT("Apply Fix")};
    FString _FixButtonTooltip;

    // ---- Cached view counts ----
    // Every tab and sub-tab count in this window reads one of these fields, and NOT the model projection behind it.
    //
    // The page tab bar sits OUTSIDE the page switcher, so it is painted on every page, every frame. Binding those
    // counts straight to `Get_VisibleFindingCount` / `Get_VisibleMemoryRowCount` / `Get_VisibleCleanupRowCount` put
    // a full walk of the corresponding census on the paint path — twice per tab, because `SCkDebug_UnderlineTabs`
    // evaluates `CountText` for its visibility as well as its text. On a memory census of tens of thousands of
    // resident objects that is the single most expensive thing this window did, and it did it whether or not the
    // Memory page was even open.
    //
    // Refreshed by the same `DoRebuild_*` that refills the list they describe, which is exactly when they can change.
    int32 _VisibleFindingCount = 0;
    FCkOptimizationDebugger_SeverityCounts _VisibleSeverityCounts;
    FCkOptimizationDebugger_SeverityCounts _TotalSeverityCounts;

    // What each SEVERITY TOGGLE would give the reader: every filter axis honoured except the severity mask itself.
    // Distinct from `_VisibleSeverityCounts` above, which honours the mask and is what the page bar reports.
    FCkOptimizationDebugger_SeverityCounts _ToggleSeverityCounts;

    // Per-category visible counts, indexed by the category's enum value and read through a bounds-checked helper.
    // The category toggles print these in their tooltips, and those tooltips resolve on hover rather than at
    // construction — so the number has to come off a field the rebuild maintains, not a projection walked on demand.
    TArray<int32> _VisibleCategoryCounts;

    // What the fix-queue tray and its apply button bind to, refreshed by the same rebuild that refills the tray.
    int32 _QueuedCount = 0;
    int32 _QueuedFixableCount = 0;
    bool _QueueApplyEnabled = false;
    FString _QueueApplyLabel = FString{TEXT("Review && Fix")};
    FString _QueueApplyTooltip;

    // Indexed by the enum value, sized by the projection's own table/category list so a value added in the middle
    // cannot silently shift them.
    TArray<int32> _VisibleMemoryCountByTable;
    TArray<int32> _VisibleCleanupCountByCategory;

    // Per-WINDOW, never static: two optimization debugger tabs must not block each other's rebuilds. The dashboard's
    // threshold editors report their focus window into this, and `DoRebuild_Dashboard` defers rather than destroying
    // the text box a user is halfway through typing into.
    TSharedPtr<FCkInspectorEditGuard> _ThresholdEditGuard;

    // ---- Memory analyzer ----
    TSharedPtr<SListView<FMemoryItem>> _MemoryList;
    TArray<FMemoryItem> _MemoryItems;

    // Stable row identity across FILTER and SORT passes, keyed by asset path. Dropped by a memory scan for the same
    // reason the findings map is dropped by a level scan: a re-scan can reproduce a path with different numbers
    // behind it, and a reused row widget is only rebuilt when the set changes.
    TMap<FString, FMemoryItem> _MemoryItemsByPath;

    // The item ORDER as of the last rebuild. A sort changes the order without changing the set, and `SListView`
    // renders in item-source order — so the refresh trigger has to compare the sequence, not just the membership.
    TArray<FString> _MemoryRowOrder;

    ECkOptimizationDebugger_MemoryTable _ActiveMemoryTable = ECkOptimizationDebugger_MemoryTable::Textures;

    // Biggest first is the answer to "what is eating memory", which is the only reason this page exists.
    ECkOptimizationDebugger_MemoryColumn _MemorySortColumn = ECkOptimizationDebugger_MemoryColumn::ResourceSize;
    bool _MemorySortAscending = false;

    // Cached so the header tiles' attributes read a field rather than re-summing every row on the paint path.
    FCkOptimizationDebugger_MemoryTotals _MemoryTotals;

    // The largest resource size in the ACTIVE table, so a row's meter is its share of the worst offender rather than
    // of a grand total that would leave every bar invisible.
    //
    // Read by each row through a `TAttribute`, never baked in at row construction: a filter pass reuses the row
    // widgets it kept, so a denominator captured when the row was built would survive the filter that changed it and
    // every remaining bar would be drawn against a largest row that is no longer in the table.
    int64 _MemoryLargestRowBytes = 0;

    // ---- Profiling launcher ----
    TSharedPtr<SEditableTextBox> _CustomCommandBox;

    // Rebuilt in place by DoRebuild_Profiling. A plain box rather than a list: at most five rows, fixed between
    // runs — there is no virtualization to buy, and a plain box is what makes a CLICKABLE chip legal in a row at
    // all (inside an STableRow it would eat the click that selects it).
    TSharedPtr<SVerticalBox> _RecentCommandsBox;

    // Most recent first, de-duplicated, capped at k_MaxRecentCommands. Per-window and per-session on purpose: a
    // console one-off is not a preference, and persisting it would outlive the question it answered.
    TArray<FString> _RecentCommands;

    // ---- Project cleanup ----
    TSharedPtr<SListView<FCleanupItem>> _CleanupList;

    // ---- Snapshots page ----
    // The strip is a fixed panel of clickable chips rather than a list: a click-consuming widget inside an
    // `SListView` row swallows the row's own hit, which is the module's row-safety rule.
    TSharedPtr<SHorizontalBox> _SnapshotStrip;
    TSharedPtr<SVerticalBox> _SnapshotFactsBox;
    TSharedPtr<SCkOptimizationSnapshotViewer> _SnapshotViewer;
    TSharedPtr<SListView<FSnapshotMeshItem>> _SnapshotMeshList;
    TArray<FSnapshotMeshItem> _SnapshotMeshItems;
    TArray<FCleanupItem> _CleanupItems;

    // Stable row identity across FILTER and CATEGORY passes, keyed by `<category id>|<asset path>`. Dropped by a
    // cleanup scan for the same reason the memory map is dropped by a memory scan.
    TMap<FString, FCleanupItem> _CleanupItemsByKey;

    // The item ORDER as of the last rebuild — a category switch changes the sequence without necessarily changing the
    // membership of the map, and `SListView` renders in item-source order.
    TArray<FString> _CleanupRowOrder;

    ECkOptimizationDebugger_CleanupCategory _ActiveCleanupCategory =
        ECkOptimizationDebugger_CleanupCategory::Unreferenced;

    // Cached so the header tiles' attributes read a field rather than re-counting every row on the paint path.
    FCkOptimizationDebugger_CleanupTotals _CleanupTotals;

    // What the cleanup action button binds to. Derived state, refreshed by `DoRefresh_CleanupCommands`.
    bool _CleanupActionEnabled = false;
    FString _CleanupActionLabel;
    FString _CleanupActionTooltip;
};

// --------------------------------------------------------------------------------------------------------------------
