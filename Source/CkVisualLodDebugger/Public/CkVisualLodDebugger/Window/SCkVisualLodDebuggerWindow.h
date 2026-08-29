#pragma once

#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkVisualLodDebugger/Data/CkVisualLodDebugger_DataCollector.h"
#include "CkVisualLodDebugger/Markers/CkVisualLodDebugger_MarkerSet.h"

#include "CoreMinimal.h"
#include "Widgets/Views/SHeaderRow.h"   // EColumnSortMode / EColumnSortPriority live here, not in SListView.h
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportPicker;
class SBox;
class SCkDebug_DualSearchBar;
class SCkDebug_EventLog;
class SVerticalBox;
class UWorld;

// ====================================================================================================================
// CK Visual LOD Debugger window.
//
// Layout: window chrome (viewport picker · freeze · markers) → alert lane → domain underline tabs (one
// per arbiter) → arbiter identity + status pill → stat strip → three overview subpanes (budgets +
// crowd pool · resolved view + config · activity sparklines) → member roster · detail rail · event log.
//
// ====================================================================================================================
// RENDERING POLICY — the split this window is built around
// ====================================================================================================================
//
// The refresh gate defaults to Unlimited, so Tick can run every frame. Rebuilding this body that often
// tears down live widgets mid-layout, which reads on screen as violent flicker (the lesson
// SCkDialogDebuggerWindow records and SCkAggroDebuggerWindow repeats).
//
// So the tree is built ONCE in Construct and every value flows through a TAttribute lambda reading
// _Live — the selected arbiter's snapshot, refreshed on the gated tick. Only three things can change
// STRUCTURE, and each has its own signature and its own stable host container:
//
//   * the arbiter SET      → _DomainTabsHost   (a domain appeared or vanished)
//   * the crowd COUNT      → _CrowdPoolBox     (an arbiter config declares N pools)
//   * the alert SET        → _AlertBox         (a fault appeared or cleared)
//
// Nothing else rebuilds, ever. The stat strip, the meters, the view card, and the sparklines are the
// same widgets for the window's whole life.
//
// The sparklines and the running totals are SAMPLED at the refresh rate, not accumulated per arbiter
// tick — the debugger has no hook on the arbiter's update. They are a trend, and the pane says so.
// ====================================================================================================================

class SCkVisualLodDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkVisualLodDebuggerWindow) {}
    SLATE_END_ARGS()

    virtual ~SCkVisualLodDebuggerWindow();

    auto
    Construct(
        const FArguments& InArgs) -> void;

    auto
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Visual LOD")); }

    /**
     * THE pick predicate for this debugger — one public static, deliberately.
     *
     * It is both the viewport picker's TargetFilter and the FCkDebug_EntityTargetRoute's predicate.
     * Picker and route must resolve the same real target; two predicates would be two answers to one
     * question.
     */
    static auto
    Is_VisualLodPickCandidate(
        const FCk_Handle& InEntity) -> bool;

    /**
     * Select InEntity's member in the roster, switching the domain tab to its arbiter.
     *
     * THE one entry point for every selection that did not originate in the roster itself — the module's
     * FCkDebug_EntityTargetRoute and the viewport picker both land here, so "targeted from elsewhere" and "clicked a
     * row" cannot drift into two different selections. Resolves through the pick predicate's lineage, so an owner
     * NPC or a child feature entity finds its member.
     */
    auto
    TargetEntity(
        const FCk_Handle& InEntity) -> void;

    // F frames the selected member in the ejected editor viewport (the context menu carries the discoverable twin).
    virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    // Deliberately NOT overriding SupportsKeyboardFocus: a focusable window means EVERY click on the
    // debugger yanks keyboard focus off the game viewport and kills game input until the viewport is
    // re-clicked. Key events bubble along the focus path, so OnKeyDown's F-to-frame still fires
    // whenever the roster (or any other child) holds focus — the Crowd agent panel's scoping, one level up.

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    // ---- structure (built once, or on one of the three signatures) ----

    auto DoBuild_CommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto DoBuild_Body() -> TSharedRef<SWidget>;
    auto DoBuild_ArbiterHeader() -> TSharedRef<SWidget>;
    auto DoBuild_StatStrip() -> TSharedRef<SWidget>;
    auto DoBuild_OverviewGrid() -> TSharedRef<SWidget>;
    auto DoBuild_BudgetsPane() -> TSharedRef<SWidget>;
    auto DoBuild_ViewPane() -> TSharedRef<SWidget>;
    auto DoBuild_ActivityPane() -> TSharedRef<SWidget>;

    auto DoRebuild_DomainTabs() -> void;
    auto DoRebuild_CrowdPools() -> void;
    auto DoRebuild_Alerts() -> void;

    auto DoBuild_StructureSignature() const -> FString;
    auto DoBuild_AlertSignature() const -> FString;

    // ---- per-gated-tick value refresh ----

    auto DoUpdate_Live() -> void;
    auto DoPush_ActivitySample() -> void;

    // ---- state ----

    auto DoGet_TargetWorld() const -> UWorld*;
    auto DoGet_StatusText() const -> FText;

    // Drops every FCk_Handle this window (and its collector) holds. Called from both lifecycle signals
    // and from the domain switch; handles hold the registry by value and must never outlive it.
    auto DoReset_WorldState() -> void;

    auto HandleSessionInvalidated() -> void;
    auto HandleWorldInvalidated(UWorld* InWorld) -> void;

    // ================================================================================================
    // The lower body: the member roster on the left, the detail rail and the event log on the right.
    //
    // All three obey the window's rendering policy. The roster's STRUCTURE is its row SET — rows are
    // reused by member entity across refreshes and every cell is TAttribute-bound, so a live tick moves
    // numbers without touching a widget. The detail rail and the event log are built ONCE for the
    // window's whole life: the rail's rows read the SELECTED member through a lambda and its
    // promoted/far section variants swap by Visibility rather than by rebuild, so even changing the
    // selection costs no widget-tree work.
    // ================================================================================================
    auto DoBuild_RosterPane() -> TSharedRef<SWidget>;
    auto DoBuild_DetailRail() -> TSharedRef<SWidget>;
    auto DoBuild_EventLog() -> TSharedRef<SWidget>;

    using FRosterItemPtr = TSharedPtr<FCkVisualLodDebugger_MemberInfo>;

    // ---- roster ----

    auto DoApply_RosterPipeline() -> void;
    auto DoRestore_RosterSelection() -> void;

    auto Handle_GenerateRosterRow(FRosterItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto Handle_RosterSelectionChanged(FRosterItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto Handle_RosterContextMenu() -> TSharedPtr<SWidget>;
    auto Handle_RosterSortChanged(EColumnSortPriority::Type InPriority, const FName& InColumn, EColumnSortMode::Type InMode) -> void;
    auto DoGet_SortModeFor(FName InColumn) const -> EColumnSortMode::Type;

    // Case-insensitive match over exactly the cells a row renders, so the user can never filter on
    // something the roster does not show.
    auto DoMatches_Query(const FCkVisualLodDebugger_MemberInfo& InMember, const FString& InNeedle) const -> bool;

    // ---- selection (one concept, four sources: roster click, viewport pick, sync bus, target route) ----

    auto DoSelect_Member(const FCk_Handle& InMember, bool InBroadcast) -> void;
    auto DoGet_SelectedMember() const -> const FCkVisualLodDebugger_MemberInfo*;

    auto HandleGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;

    // ---- event log + markers ----

    auto DoUpdate_EventLog() -> void;
    auto DoUpdate_Markers() -> void;

    // ---- collected data ----

    // Tallies over _Live.Members, computed ONCE per gated tick. The stat cells, pills and meters are all
    // TAttribute-bound and re-evaluate on every paint; re-walking the member array in each of them would
    // re-derive the same seven numbers per cell per frame, and a domain can hold hundreds of members.
    struct FTallies
    {
        int32 Members    = 0;
        int32 InView     = 0;
        int32 Proxy      = 0;
        int32 Far        = 0;
        int32 Fading     = 0;
        int32 Hidden     = 0;
        int32 Unrendered = 0;
        int32 UsedSlots  = 0;
    };

    FCkVisualLodDebugger_DataCollector _Collector;

    FTallies _Tallies;

    // The selected domain's snapshot, copied out each gated tick. Every attribute lambda in the body
    // reads THIS, so a refresh is one copy and zero widget-tree work.
    FCkVisualLodDebugger_ArbiterInfo _Live;
    bool _HasLiveArbiter = false;

    FName _SelectedDomain;

    // ---- retained hosts (the only containers a rebuild ever touches) ----

    TSharedPtr<SBox>          _DomainTabsHost;
    TSharedPtr<SVerticalBox>  _CrowdPoolBox;
    TSharedPtr<SVerticalBox>  _AlertBox;

    TSharedPtr<SVerticalBox>  _RosterPaneBox;
    TSharedPtr<SVerticalBox>  _DetailRailBox;

    FString _LastStructureSignature;
    FString _LastAlertSignature;

    // ---- roster ----

    TArray<FRosterItemPtr>                _RosterItems;
    TSharedPtr<SListView<FRosterItemPtr>> _RosterListView;
    TSharedPtr<SCkDebug_DualSearchBar>    _RosterSearchBar;

    FString _FilterString;
    FString _HighlightString;

    // Distance ascending mirrors the collector's default member order — the order the arbiter itself ranks in.
    FName                 _SortColumn = NAME_None;
    EColumnSortMode::Type _SortMode   = EColumnSortMode::Ascending;

    // ---- selection ----

    // THE selected member. Roster click, viewport pick, an incoming DebugSelectionSync and the module's
    // entity-target route all write this one handle; the detail rail, the marker emphasis and the F key all read it.
    FCk_Handle _SelectedMember;

    FDelegateHandle _SelectionSyncHandle;

    // ---- event log ----

    TSharedPtr<SCkDebug_EventLog> _EventLog;

    // What each member looked like at the previous gated collect. The log is SYNTHESIZED from the delta between two
    // snapshots — the arbiter fires no signals the debugger could subscribe to — so a flip that starts and completes
    // between two gated ticks is invisible by construction, and a transition seen here is "changed since last look",
    // not "happened just now".
    struct FMemberEventState
    {
        ECk_VisualLod_Representation Representation = ECk_VisualLod_Representation::None;
        bool  PreemptDemote = false;
        int32 SlotIndex     = INDEX_NONE;
    };

    TMap<FCk_Handle, FMemberEventState> _MemberEventStates;

    // The domain those states describe. Events are per-arbiter, so a tab switch discards the log rather than
    // reporting the new domain's whole membership as if it had just appeared.
    FName _EventDomain;
    bool  _EventFrozen     = false;
    bool  _HasEventBaseline = false;

    double _TickTimeSeconds = 0.0;

    // ---- world markers ----

    FCkVisualLodDebugger_MarkerSet _MarkerSet;

    // ---- activity rings (window-owned; sampled on the gated tick) ----

    TSharedPtr<TArray<float>> _RingPromotes;
    TSharedPtr<TArray<float>> _RingDemotes;
    TSharedPtr<TArray<float>> _RingPreempts;

    int32 _TotalPromotes = 0;
    int32 _TotalDemotes  = 0;
    int32 _TotalPreempts = 0;

    // ---- chrome ----

    TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;

    // Drives the retained PMG marker shapes: diamond over a promoted proxy, dot over a far GPU member, ring over an
    // unrendered one; hidden members skipped. Toggling off clears the whole set.
    bool _MarkersEnabled = false;

    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldInvalidatedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
