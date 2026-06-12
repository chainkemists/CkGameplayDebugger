#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// ====================================================================================================================
// Slate visual for UCkGoapDebugNode_Action.
//
// Layout (per mockup F v2):
//   - Outer rounded border tinted by selection / plan / failure state:
//       * Selected:        amber 2.5px ring
//       * Failure-blocked: red 2px ring
//       * In-plan:         blue 2px ring
//       * Composite:       purple 2px ring (if not otherwise highlighted)
//       * Default:         dim outline
//   - Header row: action class name (truncated) and "$N" cost in amber.
//   - Composite badge: small purple "▸ <leaf tag>" strip between header and body
//     when ChildActionHandles.Num() > 0.
//   - Body: two columns — precondition rows (left) and effect labels (right),
//     name-sorted. Each precondition reads "[exp] -> [cur] KeyName" where the
//     squares encode VALUES (green = true, red = false): exp is the authored
//     desired value (static), cur is the LIVE value from the Planner's
//     resolved WS (TAttribute-bound; dim when the key isn't in the WS).
//     Satisfied = both squares match. Effects keep a static blue dot. Card
//     width self-sizes (Min 180 / Max 420) so long key names never clip.
//   - Plan-step badge: top-left numbered circle when PlanStepIndex > 0.
// ====================================================================================================================

class UCkGoapDebugNode_Action;

class CKGOAPDEBUGGER_API SGraphNode_GoapAction : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_GoapAction) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkGoapDebugNode_Action* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode()    -> void override;
    virtual auto CreatePinWidgets()   -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& InPinToAdd) -> void override;

private:
    auto BuildBody() -> TSharedRef<SWidget>;

    UCkGoapDebugNode_Action* _ActionNode = nullptr;
};

// ====================================================================================================================
