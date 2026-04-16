#include "CkGoapDebugConnectionPolicy.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkGoapDebugger/Graph/CkGoapDebugNode_Action.h"

// ====================================================================================================================

FCkGoapDebugConnectionPolicy::FCkGoapDebugConnectionPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj)
	: FCkDebugConnectionPolicyBase(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
{
	_Graph = Cast<UCkGoapDebugGraph>(InGraphObj);
}

// ====================================================================================================================

auto
	FCkGoapDebugConnectionPolicy::
	DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams)
	-> void
{
	FCkDebugConnectionPolicyBase::DetermineWiringStyle(InOutputPin, InInputPin, OutParams);

	auto* SourceNode = InOutputPin ? Cast<UCkGoapDebugNode_Action>(InOutputPin->GetOwningNode()) : nullptr;
	auto* TargetNode = InInputPin ? Cast<UCkGoapDebugNode_Action>(InInputPin->GetOwningNode()) : nullptr;

	if (SourceNode && TargetNode && SourceNode->Get_InPlan() && TargetNode->Get_InPlan())
	{
		OutParams.WireColor = FLinearColor(0.13f, 0.77f, 0.37f);
		OutParams.WireThickness = 3.0f;
	}
	else
	{
		OutParams.WireColor = FLinearColor(0.1f, 0.14f, 0.2f);
		OutParams.WireThickness = 1.0f;
	}
}

// ====================================================================================================================
