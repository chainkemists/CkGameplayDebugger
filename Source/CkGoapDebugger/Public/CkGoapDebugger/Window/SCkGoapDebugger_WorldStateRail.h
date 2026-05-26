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

// ====================================================================================================================
// SCkGoapDebugger_WorldStateRail — fixed-width right rail mirroring the
// mockup's `.ws-rail` panel. Shows the resolved WorldState for the currently
// selected Action (or the selected Planner's WS as a fallback when no
// specific Action is selected).
//
// Layout (top-to-bottom):
//   - Header  : "WorldState · {WS_Label}"   { keys count }   (pane-head style)
//   - Body    : scrollable list of key/value rows; each row optionally tinted
//               with the "recently changed" amber background and prefixed by
//               a star glyph.
//   - Footer  : "★ = recently changed · {recent} of {total} keys"
//
// Rebuild strategy:
//   - Cheap: each refresh clears the body VBox and re-populates.
//     The WS list is small (~20 keys typical).
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

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildEmptyState() -> TSharedRef<SWidget>;
    auto BuildKeyRow(const FCkGoapDebugger_WorldStateEntry& InEntry) -> TSharedRef<SWidget>;

    // Override-stack inspector — appears above the key rows when one or more
    // override layers are pushed onto the WS. Each layer is a clickable row
    // with name + key count + Pop button; click the row to expand a per-key
    // drilldown showing the keys the layer carries and their values.
    auto BuildOverrideLayersSection(const FCk_Handle_Goap_WorldState& InWs) -> TSharedRef<SWidget>;
    auto BuildOverrideLayerRow(const FCk_Handle_Goap_WorldState& InWs, FName InLayerName) -> TSharedRef<SWidget>;

    // Resolve which WS list to show + the label/source. Returns false when no
    // entity / Planner is selected.
    auto Resolve_DisplayedWorldState(
        const TArray<FCkGoapDebugger_WorldStateEntry>*& OutEntries,
        FString& OutLabel) const -> bool;

    // -----------------------------------------------------------------------------------------------------------------
    // Debug-override stack helpers
    //
    // The rail acts as a hands-on shadow for `Goap.WorldState`. Each key row is
    // clickable: click pushes a single-key override into the layer named
    // "DebugUI" via `UCk_Utils_Goap_WorldState_UE::Push_Override_SingleKey`.
    // Click again to flip back; a header "Reset" button pops the whole layer.
    //
    // The currently-shown WS handle is cached at refresh time (in
    // `_CurrentWorldState`) so click handlers and TAttribute-bound visuals can
    // touch the API without re-resolving from the ViewModel mid-event.
    // -----------------------------------------------------------------------------------------------------------------

    auto HandleClick_ToggleKeyOverride(FGameplayTag InKey, bool InCurrentEffectiveValue) -> FReply;
    auto HandleClick_ResetDebugUiLayer() -> FReply;

    // Generic per-layer Pop button — works for any named layer, not just DebugUI.
    auto HandleClick_PopLayer(FName InLayerName) -> FReply;

    // Toggles whether the layer drilldown is expanded for that layer.
    auto HandleClick_ToggleLayerExpand(FName InLayerName) -> FReply;

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
};

// ====================================================================================================================
