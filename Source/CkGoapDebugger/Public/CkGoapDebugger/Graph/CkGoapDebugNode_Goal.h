#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkGoapDebugNode_Goal.generated.h"

// ====================================================================================================================
// UEdGraphNode subclass — the rightmost "★ Goal" anchor on the action graph.
// Reflects the goal conditions of the selected Action (or root Action when
// none is selected). One input pin per goal condition lets the connection
// policy wire up effect-producer Actions feeding into the goal.
// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugNode_Goal : public UEdGraphNode
{
    GENERATED_BODY()

public:
    // UEdGraphNode
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }

    auto PopulateFromGoal(
        const FString& InOwningActionName,
        const TArray<FCkGoapDebugger_Condition>& InGoal) -> void;

    auto Get_GoalConditions() const -> const TArray<FCkGoapDebugger_Condition>& { return _Goal; }
    auto Get_OwningActionName() const -> const FString& { return _OwningActionName; }

private:
    TArray<FCkGoapDebugger_Condition> _Goal;

    UPROPERTY()
    FString _OwningActionName;
};

// ====================================================================================================================
