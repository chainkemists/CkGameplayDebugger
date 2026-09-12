#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mission Control — Inspector LEFT column.
//
// Stacks the mockup's left-side panels, all driven off the ViewModel's
// selected Planner snapshot:
//   1. Agent card      avatar initials, debug name, live status pill, attempt #
//   2. Chain crumb     ◆ Planner ▸ ◆● composite ▸ ● leaf — active glow, fallback amber
//   3. Goal panel      goal conditions as chips with LIVE satisfaction
//   4. Plan panel      step cards (NOW marker, pre/eff chips, nested sub-plan,
//                      fallback note) + footer stats
//   5. Settings drawer all planner params (RO locks where construction-time),
//                      live request verbs (policy / interval / budget /
//                      threshold / enable) + Replan / Cancel / Reset buttons
//
// Rebuild discipline: hash-debounced structural rebuilds (plan content, chain,
// selection); live values (status, satisfaction colors) are attribute-bound.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class FCkUiCollection;
class FCkUiView;
class SBox;
class SVerticalBox;
class SCkGoapDebuggerWindow;

class CKGOAPDEBUGGER_API SCkGoapDebugger_AgentColumn : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_AgentColumn) {}
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


    // Drop handle-bearing state while the registry is alive.
    auto Reset_ForWorldChange() -> void;

    /** Test-only inspection of the retained production authored surface. */
    auto Get_AuthoredView() const -> TSharedPtr<FCkUiView>
    {
        return _AuthoredView;
    }

    auto Get_AuthoredLoadFailure() const -> const FString&
    {
        return _AuthoredLoadFailure;
    }

private:
    auto DoRebuild(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName) -> void;

    auto DoBuildAgentCard(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName) -> TSharedRef<SWidget>;
    auto DoBuildChainCrumb(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildGoalPanel(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildPlanPanel(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildSettingsDrawer(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildSearchBudgetEditor(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto TryActivateAuthoredView() -> void;
    auto ActivateNativeFallback() -> void;
    auto ClearAuthoredNativePorts() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    TSharedPtr<SVerticalBox> _Body;
    TSharedPtr<SBox> _ContentHost;
    TSharedPtr<SWidget> _NativeContent;

    // Planner-dependent bodies and the int64 search-budget editor remain native. The authored
    // document owns the stable shell and all supported primitive controls around these ports.
    TSharedPtr<SBox> _BreadcrumbPort;
    TSharedPtr<SBox> _GoalPort;
    TSharedPtr<SBox> _PlanPort;
    TSharedPtr<SBox> _SearchBudgetPort;
    TSharedPtr<FCkUiCollection> _PolicyOptions;
    TSharedPtr<FCkUiView> _AuthoredView;
    FString _AuthoredLoadFailure;
    uint64 _AuthoredGeneration = 0;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
