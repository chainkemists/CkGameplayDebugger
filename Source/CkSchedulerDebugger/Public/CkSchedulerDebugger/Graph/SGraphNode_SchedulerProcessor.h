#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class UCkSchedulerDebugNode_Processor;

// --------------------------------------------------------------------------------------------------------------------

class SGraphNode_SchedulerProcessor : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_SchedulerProcessor) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs, UCkSchedulerDebugNode_Processor* InNode) -> void;

	virtual auto UpdateGraphNode() -> void override;
	virtual auto CreatePinWidgets() -> void override;
	virtual auto AddPin(const TSharedRef<SGraphPin>& InPinToAdd) -> void override;

private:
	UCkSchedulerDebugNode_Processor* _ProcessorNode = nullptr;

	auto CreateMetaRow() -> TSharedRef<SWidget>;

	auto Get_TimingText() const -> FText;
	auto Get_TimingColor() const -> FSlateColor;
};

// --------------------------------------------------------------------------------------------------------------------
