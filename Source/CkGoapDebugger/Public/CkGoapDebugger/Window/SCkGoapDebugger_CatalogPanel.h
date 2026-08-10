#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mission Control — "Catalog Audit" tab. Design-time truth about the selected
// Planner's catalog, independent of the current world state:
//
//   1. Action cards per tier (top catalog + one section per composite's
//      sub-catalog): role glyph + name + cost + live stepper + badges
//      (sub-planner / fallback) + class · derived-tag line + pre/eff chips.
//   2. Health checks: fallback guarantee (always-valid-plan tenet), no
//      dependency cycles, goal keys registered, key budget n/64 meter, plus
//      cross-tier-reachability and dead-effect INFO findings from the lint.
//   3. Key × action matrix: WS keys × actions, P/E cells, goal badge per key
//      row, active-chain columns tinted, key-row click traces the key
//      (ViewModel Set_TracedWsKey — same channel as the WS rail).
//
// Findings come from the pure DecisionModel::Lint; the panel only adapts and
// renders. Hash-debounced rebuild off the selected Planner snapshot.
//
// Layout: persistent SSplitter chrome built ONCE in Construct — action catalog
// left (default half the window), health checks over the matrix right, every
// pane user-resizable. Rebuilds only SetContent the three hosts, so splitter
// drag positions survive data refreshes.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SBox;
class SWidgetSwitcher;

class CKGOAPDEBUGGER_API SCkGoapDebugger_CatalogPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_CatalogPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Called by the window when the ViewModel publishes a change.
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
        _LastHash = ~_LastHash;
        RefreshFromViewModel();
    }


    auto Reset_ForWorldChange() -> void;

private:
    auto DoRebuild(const FCkGoapDebugger_PlannerInfo& InPlanner) -> void;

    auto DoBuildCardSection(const FCkGoapDebugger_PlannerInfo& InTier, const FString& InLabel) -> TSharedRef<SWidget>;
    auto DoBuildActionCard(const FCkGoapDebugger_PlannerInfo& InTier, const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;
    auto DoBuildHealthChecks(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildMatrix(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;

    auto HandleCostDelta(int32 InDelta, FCk_Handle_Goap_Planner InPlanner, TSubclassOf<UCk_GoapAction_EntityScript> InClass, float InCurrentCost) -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Index 0 = "select a Planner" empty state, 1 = the splitter layout.
    TSharedPtr<SWidgetSwitcher> _ViewSwitcher;

    // Content hosts inside the persistent splitters — refreshed via SetContent.
    TSharedPtr<SBox> _CatalogHost;
    TSharedPtr<SBox> _HealthHost;
    TSharedPtr<SBox> _MatrixHost;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
