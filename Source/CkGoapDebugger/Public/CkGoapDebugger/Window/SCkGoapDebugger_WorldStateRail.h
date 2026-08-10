#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CkGoap/WorldState/CkGoap_WorldState_Fragment_Data.h"   // FCk_Handle_Goap_WorldState

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class STextBlock;
class SBox;
class SVerticalBox;

// Sort order for the rail's key rows.
enum class ECkGoapDebugger_WsSortMode : uint8
{
    ByName,        // alphabetical on the full tag string (default)
    ByTrueFirst    // TRUE values first, then alphabetical
};

// ====================================================================================================================
// SCkGoapDebugger_WorldStateRail — Mission Control's "World State" panel
// (mockup RIGHT column). Shows the resolved WorldState for the currently
// selected Action (or the selected Planner's WS as a fallback when no
// specific Action is selected).
//
// Layout (top-to-bottom):
//   - Header  : SCkDebug_SectionHeader "WORLD STATE · what the agent believes"
//               with the Sandbox switch as RightContent. Sandbox ON makes the
//               value pills editable (flips go into the "DebugUI" override
//               layer); switching OFF pops the layer — back to live truth.
//   - Search  : SCkDebug_DualSearchBar (Filter narrows rows, Highlight dims
//               non-matches) + a sort toggle (Name ⇄ TRUE-first). Lives in
//               the fixed chrome so rebuilds never steal input focus.
//   - Body    : override-layer stack (when non-empty) + one row per key:
//               name · usage census "nP·mE" · [layer shadow badge]
//               [just-changed chip] · SCkDebug_ValuePill. Row click traces
//               the key across panes (ViewModel Set_TracedWsKey).
//   - Footer  : "keys n / 64 · subscribers m" + the live trace hint.
//
// Rebuild strategy:
//   - Cheap: each refresh clears the body VBox and re-populates (hash-gated).
//     The WS list is small (~20 keys typical). Override/sandbox/trace visuals
//     are TAttribute-bound so they track live without a rebuild.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_WorldStateRail : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_WorldStateRail) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_WorldStateRail();

    // Called by the parent window on ViewModel::OnChanged.
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
        _LastContentHash = ~_LastContentHash;
        RefreshFromViewModel();
    }


    // Read by the window's alert strip — the sandbox banner shows as soon as
    // the sandbox is ARMED, not only once a key is actually overridden.
    auto Get_IsSandboxMode() const -> bool { return _SandboxMode; }

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildEmptyState() -> TSharedRef<SWidget>;
    auto BuildHeader(const FString& InLabel) -> TSharedRef<SWidget>;
    auto BuildKeyRow(const FCkGoapDebugger_WorldStateEntry& InEntry, bool InHighlightDimmed, FIntPoint InUsage) -> TSharedRef<SWidget>;
    auto BuildSearchAndSortBar() -> TSharedRef<SWidget>;
    auto BuildFooter(int32 InKeyCount) -> TSharedRef<SWidget>;

    // Override-stack inspector — always visible above the key rows (mockup
    // "layerbox"): pushed layers top-of-stack first, then a permanent
    // "base store" row. Each layer is a clickable row with name + key count +
    // Pop button; click the row to expand a per-key drilldown showing the
    // keys the layer carries and their values.
    auto BuildOverrideLayersSection(const FCk_Handle_Goap_WorldState& InWs, int32 InBaseKeyCount) -> TSharedRef<SWidget>;
    auto BuildOverrideLayerRow(const FCk_Handle_Goap_WorldState& InWs, FName InLayerName) -> TSharedRef<SWidget>;

    // Resolve which WS list to show + the label/source. Returns false when no
    // entity / Planner is selected.
    auto Resolve_DisplayedWorldState(
        const TArray<FCkGoapDebugger_WorldStateEntry>*& OutEntries,
        FString& OutLabel) const -> bool;

    // -----------------------------------------------------------------------------------------------------------------
    // Sandbox (debug-override) helpers
    //
    // The rail acts as a hands-on shadow for `Goap.WorldState`. With Sandbox
    // ON, each row's value pill is editable: a flip pushes a single-key
    // override into the layer named "DebugUI" via
    // `UCk_Utils_Goap_WorldState_UE::Push_Override_SingleKey`. Switching
    // Sandbox OFF pops the whole layer — live truth restored.
    //
    // The currently-shown WS handle is cached at refresh time (in
    // `_CurrentWorldState`) so click handlers and TAttribute-bound visuals can
    // touch the API without re-resolving from the ViewModel mid-event.
    // -----------------------------------------------------------------------------------------------------------------

    // ValuePill OnToggled — pill payload first (delegate arg), key bound at build time.
    auto HandlePillToggled(bool InNewValue, FGameplayTag InKey) -> void;
    auto HandleSandboxToggled(bool InNewState) -> void;
    auto HandleClick_ResetDebugUiLayer() -> FReply;

    // Base-store truth-table edit. Distinct from HandlePillToggled on purpose: the PILL writes the
    // DebugUI OVERRIDE layer (a shadow you pop to undo), this writes Set_Value — the base store every
    // read falls through to. Needs no Sandbox arming precisely because it is not a hidden shadow.
    auto HandleBaseValueToggled(bool InNewValue, FGameplayTag InKey) -> void;

    // Pops EVERY override layer at once (Clear_Overrides), gameplay-pushed ones included — unlike the
    // per-layer Pop and unlike the Sandbox switch, which only takes DebugUI back off.
    auto HandleClick_ClearAllOverrides() -> FReply;

    // Row click — toggles the cross-pane key trace on the ViewModel.
    auto HandleRowClicked_Trace(FGameplayTag InKey) -> FReply;

    // Generic per-layer Pop button — works for any named layer, not just DebugUI.
    auto HandleClick_PopLayer(FName InLayerName) -> FReply;

    // Toggles whether the layer drilldown is expanded for that layer.
    auto HandleClick_ToggleLayerExpand(FName InLayerName) -> FReply;

    // Cycles the key-row sort order (Name ⇄ TRUE-first).
    auto HandleClick_CycleSortMode() -> FReply;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Host slots
    TSharedPtr<SBox>         _HeaderHost;
    TSharedPtr<SVerticalBox> _Body;
    TSharedPtr<SBox>         _FooterHost;

    // Resolved WS handle for the currently-rendered selection. Updated each
    // refresh; used by click handlers and TAttribute lambdas that consult the
    // override stack live.
    FCk_Handle_Goap_WorldState _CurrentWorldState{};

    // Content-hash gate. The rail rebuilds three subtrees per refresh; on a
    // typical Live tick the WS entries don't change, so we early-out unless
    // the snapshot actually differs from the last render. Captures (entity,
    // Planner, key set + values + RecentlyChanged flag, label).
    //
    // NOTE: override-state visuals (per-row OVERRIDE pill / tint, header
    // "[+N layer]" badge, Reset button visibility + tooltip) are bound via
    // TAttribute lambdas so they update live without forcing a rebuild. They
    // are deliberately NOT folded into _LastContentHash — pushing/popping the
    // DebugUI layer must not trigger a destructive subtree rebuild.
    uint32 _LastContentHash = 0;
    bool   _HasMaterialized = false;

    // Per-layer expand/collapse state for the override-stack drilldown. Names
    // present in this set render their key/value listing inline; rest are
    // collapsed to just the row header.
    TSet<FName> _ExpandedLayers;

    // Search + sort state (per CkDebuggerCommon "Search bars" rebuild-on-change
    // pattern). All three fold into _LastContentHash so a change rebuilds the
    // body; the inputs themselves live in the fixed chrome and keep focus.
    FString _FilterString;
    FString _HighlightString;
    ECkGoapDebugger_WsSortMode _SortMode = ECkGoapDebugger_WsSortMode::ByName;

    // Sandbox mode — pills editable, flips go into the "DebugUI" layer.
    // Deliberately NOT in the content hash: pill editability and tooltips are
    // attribute-bound, so the toggle needs no rebuild.
    bool _SandboxMode = false;
};

// ====================================================================================================================
