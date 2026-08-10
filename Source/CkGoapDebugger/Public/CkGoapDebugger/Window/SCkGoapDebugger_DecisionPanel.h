#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mission Control — center "Decision" tab.
//
// Answers the designer question "why THIS plan?" by showing EVERY candidate,
// scored, per tier:
//   - group per planner tier: the selected Planner's catalog first, then one
//     "↳ inside <composite>" group per in-plan composite (walking the chain),
//     or a dormant note when the composite isn't in the current plan.
//   - card order inside a group: in-plan (by step) → viable-not-chosen →
//     blocked → fallback.
//   - each card: role glyph + name + cost + live cost stepper
//     (Request_SetChildActionCost) + edited badge + sub-planner/fallback
//     badges + a why-line (in-plan step / forced-first Δ / unmet keys /
//     fallback), precondition chips (live satisfaction vs the tier's WS) +
//     effect chips, cross-tier coaching notes, and a cost meter.
//   - chips highlight when their key matches the ViewModel's traced WS key
//     (set by clicking a row in the World State rail).
//
// Scoring runs through the pure DecisionModel (regressive-A* miniature) so
// "what would forcing X first cost" never round-trips the engine pipeline.
//
// Rebuild discipline: hash-debounced full rebuild (catalog, plan, WS values,
// edited costs); trace-highlight and stepper enablement are attribute-bound.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SVerticalBox;

class CKGOAPDEBUGGER_API SCkGoapDebugger_DecisionPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_DecisionPanel) {}
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


    // Drop handle-bearing state while the registry is alive. Edited-cost
    // tracking resets too — a new world means new authored costs.
    auto Reset_ForWorldChange() -> void;

private:
    auto DoRebuild(const FCkGoapDebugger_PlannerInfo& InPlanner) -> void;

    // One scored group for a planner tier. InGroupLabel is the mockup's
    // "<name> — every candidate, scored" / "↳ inside <name> — …" header.
    auto DoBuildGroup(const FCkGoapDebugger_PlannerInfo& InTier, const FString& InGroupLabel) -> TSharedRef<SWidget>;

    auto DoBuildCandidateCard(
        const FCkGoapDebugger_PlannerInfo& InTier,
        const FCkGoapDebugger_ActionInfo& InAction,
        const FCkGoapDebugger_CandidateScore& InScore,
        const FString& InNoteText) -> TSharedRef<SWidget>;

    auto DoBuildDormantCard(const FString& InCompositeName) -> TSharedRef<SWidget>;

    // Stepper / reset plumbing — issues Request_SetChildActionCost on the
    // tier's Planner handle (captured by value, IsValid-checked).
    auto HandleCostDelta(int32 InDelta, FCk_Handle_Goap_Planner InPlanner, TSubclassOf<UCk_GoapAction_EntityScript> InClass, FString InClassName, float InCurrentCost) -> void;
    auto HandleResetEditedCosts() -> FReply;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    TSharedPtr<SVerticalBox> _Body;

    // ClassName → the cost BEFORE the first stepper edit this session. Drives
    // the "edited" badge and the reset button; entries whose current cost
    // returns to the original are dropped.
    TMap<FString, float> _OriginalCosts;

    // (Planner, class) pairs needed to undo edits on reset. Parallel to
    // _OriginalCosts by ClassName.
    TMap<FString, TPair<FCk_Handle_Goap_Planner, TSubclassOf<UCk_GoapAction_EntityScript>>> _EditTargets;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
