#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkGoapDebugNode_Goal.generated.h"

// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugNode_Goal : public UEdGraphNode
{
	GENERATED_BODY()

public:
	virtual auto AllocateDefaultPins() -> void override;
	virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
	virtual auto CanUserDeleteNode() const -> bool override { return false; }
	virtual auto CanDuplicateNode() const -> bool override { return false; }

	auto
	PopulateFromGoalInfo(
		const FCkGoapDebugger_GoalInfo& InGoal) -> void;

	auto Get_GoalName() const -> const FString& { return _GoalName; }
	auto Get_DisplayName() const -> const FString& { return _DisplayName; }
	auto Set_DisplayName(const FString& InName) -> void { _DisplayName = InName; }
	auto Get_Priority() const -> int32 { return _Priority; }
	auto Get_Conditions() const -> const TArray<FCkGoapDebugger_Condition>& { return _Conditions; }
	auto Get_IsActiveGoal() const -> bool { return _IsActiveGoal; }
	auto Set_IsActiveGoal(bool InValue) -> void { _IsActiveGoal = InValue; }

private:
	UPROPERTY()
	FString _GoalName;

	UPROPERTY()
	FString _DisplayName;

	UPROPERTY()
	int32 _Priority = 0;

	TArray<FCkGoapDebugger_Condition> _Conditions;

	UPROPERTY()
	bool _IsActiveGoal = false;
};

// ====================================================================================================================
