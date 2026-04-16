#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkGoapDebugNode_Action.generated.h"

// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugNode_Action : public UEdGraphNode
{
	GENERATED_BODY()

public:
	virtual auto AllocateDefaultPins() -> void override;
	virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
	virtual auto CanUserDeleteNode() const -> bool override { return false; }
	virtual auto CanDuplicateNode() const -> bool override { return false; }

	auto
	PopulateFromActionInfo(
		const FCkGoapDebugger_ActionInfo& InAction,
		int32 InIndex) -> void;

	auto
	UpdatePlanState(
		bool InIsInPlan,
		int32 InPlanStepIndex) -> void;

	// Accessors
	auto Get_ActionName() const -> const FString& { return _ActionName; }
	auto Get_DisplayName() const -> const FString& { return _DisplayName; }
	auto Set_DisplayName(const FString& InName) -> void { _DisplayName = InName; }
	auto Get_Cost() const -> float { return _Cost; }
	auto Get_Preconditions() const -> const TMap<FGameplayTag, bool>& { return _Preconditions; }
	auto Get_Effects() const -> const TMap<FGameplayTag, bool>& { return _Effects; }
	auto Get_InPlan() const -> bool { return _InPlan; }
	auto Get_PlanStepIndex() const -> int32 { return _PlanStepIndex; }
	auto Get_ActionIndex() const -> int32 { return _ActionIndex; }
	auto Get_ActionClass() const -> TSubclassOf<UCk_GoapAction_EntityScript> { return _ActionClass; }

private:
	UPROPERTY()
	FString _ActionName;

	FString _DisplayName;

	UPROPERTY()
	float _Cost = 1.0f;

	TMap<FGameplayTag, bool> _Preconditions;
	TMap<FGameplayTag, bool> _Effects;
	TSubclassOf<UCk_GoapAction_EntityScript> _ActionClass;

	UPROPERTY()
	bool _InPlan = false;

	UPROPERTY()
	int32 _PlanStepIndex = -1;

	UPROPERTY()
	int32 _ActionIndex = -1;
};

// ====================================================================================================================
