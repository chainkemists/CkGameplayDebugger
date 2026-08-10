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

private:
    auto DoRebuild(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName) -> void;

    auto DoBuildAgentCard(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName) -> TSharedRef<SWidget>;
    auto DoBuildChainCrumb(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildGoalPanel(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildPlanPanel(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto DoBuildSettingsDrawer(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    TSharedPtr<SVerticalBox> _Body;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
