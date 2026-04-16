#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// ====================================================================================================================

class UCkGoapDebugNode_Action;

// ====================================================================================================================

class CKGOAPDEBUGGER_API SGraphNode_GoapAction : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_GoapAction) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs, UCkGoapDebugNode_Action* InNode) -> void;

	virtual auto UpdateGraphNode() -> void override;
	virtual auto CreatePinWidgets() -> void override;
	virtual auto AddPin(const TSharedRef<SGraphPin>& PinToAdd) -> void override;

private:
	auto GetBorderBackgroundColor() const -> FSlateColor;
	auto CreatePortRows() -> TSharedRef<SWidget>;
	auto CreateCompactEffectsSummary() -> TSharedRef<SWidget>;

	UCkGoapDebugNode_Action* _ActionNode = nullptr;
};

// ====================================================================================================================
