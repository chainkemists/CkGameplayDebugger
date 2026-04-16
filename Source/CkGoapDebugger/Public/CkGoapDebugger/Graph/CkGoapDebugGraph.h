#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraph.h"
#include "CoreMinimal.h"

#include "CkGoapDebugGraph.generated.h"

// ====================================================================================================================

class UCkGoapDebugNode_Action;
class UCkGoapDebugNode_Goal;

// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	auto
	UpdateFromGoapInfo(
		const FCkGoapDebugger_GoapInfo& InInfo) -> void;

	auto ForceRebuild() -> void { _TopologyHash = 0; }

	auto FindActionNode(int32 InIndex) const -> UCkGoapDebugNode_Action*;

	auto SetSuppressNotifications(bool bSuppress) -> void { _SuppressNotifications = bSuppress; }
	virtual void NotifyGraphChanged() override
	{
		if (!_SuppressNotifications) { Super::NotifyGraphChanged(); }
	}

private:
	auto RebuildTopology(const FCkGoapDebugger_GoapInfo& InInfo) -> void;
	auto UpdateRuntimeState(const FCkGoapDebugger_GoapInfo& InInfo) -> void;
	auto ComputeTopologyHash(const FCkGoapDebugger_GoapInfo& InInfo) const -> uint32;
	auto PerformLayout() -> void;

private:
	UPROPERTY()
	uint32 _TopologyHash = 0;

	bool _SuppressNotifications = false;

	UPROPERTY()
	TArray<TObjectPtr<UCkGoapDebugNode_Action>> _ActionNodes;

	UPROPERTY()
	TObjectPtr<UCkGoapDebugNode_Goal> _GoalNode;
};

// ====================================================================================================================
