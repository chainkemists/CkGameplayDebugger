#pragma once

#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"
#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

#include "CkCore/Macros/CkMacros.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

#include "CkSchedulerDebugGraph.generated.h"

class UCkSchedulerDebugNode_Processor;

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSCHEDULERDEBUGGER_API UCkSchedulerDebugGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	auto RebuildFromData(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> void;

	auto UpdateTimingData(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> void;

	auto FindNodeByProcessorIndex(int32 InProcessorIndex) const -> UCkSchedulerDebugNode_Processor*;

	// ----

	auto ForceRebuild() -> void { _TopologyHash = 0; }
	auto SetSuppressNotifications(bool InSuppress) -> void { _SuppressNotifications = InSuppress; }

	virtual void NotifyGraphChanged() override
	{
		if (NOT _SuppressNotifications) { Super::NotifyGraphChanged(); }
	}

	// ----

	auto RequestRelayout() -> void;

	FCkDebugGraphLayoutParams LayoutParams;

private:
	auto DoCreateNodes(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> void;
	auto DoCreateEdges(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> void;
	auto DoLayoutNodes(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> void;

	auto ComputeTopologyHash(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) const -> uint32;

	// ----

	UPROPERTY()
	uint32 _TopologyHash = 0;

	bool _SuppressNotifications = false;

	TArray<FCkSchedulerDebugger_ProcessorInfo> _LastProcessors;
};

// --------------------------------------------------------------------------------------------------------------------
