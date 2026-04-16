#include "CkGoapDebugConnectionPolicy.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkGoapDebugger/Graph/CkGoapDebugNode_Action.h"

#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

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

	const auto Theme = UCkDebuggerSettings::GetTheme();

	auto* SourceNode = InOutputPin ? Cast<UCkGoapDebugNode_Action>(InOutputPin->GetOwningNode()) : nullptr;
	auto* TargetNode = InInputPin ? Cast<UCkGoapDebugNode_Action>(InInputPin->GetOwningNode()) : nullptr;

	if (SourceNode && TargetNode && SourceNode->Get_InPlan() && TargetNode->Get_InPlan())
	{
		OutParams.WireColor = Theme.ActiveEdge;
		OutParams.WireThickness = Theme.ActiveEdgeThickness;
	}
	else
	{
		OutParams.WireColor = Theme.InactiveEdge;
		OutParams.WireThickness = Theme.InactiveEdgeThickness;
	}
}

// ====================================================================================================================
