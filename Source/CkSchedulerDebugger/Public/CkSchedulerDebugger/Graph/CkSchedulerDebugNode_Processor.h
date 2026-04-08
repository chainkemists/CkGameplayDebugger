#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"

#include "CkSchedulerDebugNode_Processor.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSCHEDULERDEBUGGER_API UCkSchedulerDebugNode_Processor : public UEdGraphNode
{
	GENERATED_BODY()

public:
	int32 ProcessorNodeIndex = INDEX_NONE;
	FName ProcessorName;
	FString DisplayName;
	FName GroupName;

	bool IsGhost = false;
	bool IsGroupStart = false;
	bool IsGroupEnd = false;
	bool HasDirtyMarker = false;
	bool IsParallel = false;

	double LastFrameTimeMs = 0.0;
	int32 PumpCountThisFrame = 0;
	bool WasDirtyThisFrame = false;
	int32 ExecutionOrder = INDEX_NONE;

	// ----

	virtual auto AllocateDefaultPins() -> void override;
	virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;

	auto GetInputPin() const -> UEdGraphPin* { return Pins.IsValidIndex(0) ? Pins[0] : nullptr; }
	auto GetOutputPin() const -> UEdGraphPin* { return Pins.IsValidIndex(1) ? Pins[1] : nullptr; }
};

// --------------------------------------------------------------------------------------------------------------------
