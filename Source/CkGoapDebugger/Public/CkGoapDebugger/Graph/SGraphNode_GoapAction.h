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
//   - Body: two columns — precondition labels (left) and effect labels (right),
//     each with a coloured dot (sat=green / unsat=red preconditions; effects=blue).
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
