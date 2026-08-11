#pragma once

#if WITH_EDITOR

#include "SGraphNode.h"
#include "CoreMinimal.h"

// ====================================================================================================================
// Slate visual for UCkGoapDebugNode_Goal — a rounded amber-tinted card.
//
//   ★ Goal
//   ─────────
//   Cond1 = true
//   Cond2 = false
//   ...
// ====================================================================================================================

class UCkGoapDebugNode_Goal;

class CKGOAPDEBUGGER_API SGraphNode_GoapGoal : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_GoapGoal) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkGoapDebugNode_Goal* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode()  -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& InPinToAdd) -> void override;

private:
    UCkGoapDebugNode_Goal* _GoalNode = nullptr;
};

#endif

// ====================================================================================================================
