#include "CkSchedulerDebugger/Graph/CkSchedulerDebugConnectionPolicy.h"

#if WITH_EDITOR

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

// --------------------------------------------------------------------------------------------------------------------

FCkSchedulerDebugConnectionPolicy::FCkSchedulerDebugConnectionPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements)
	: FCkDebugConnectionPolicyBase(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugConnectionPolicy::
	DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams)
	-> void
{
	OutParams.AssociatedPin1 = InOutputPin;
	OutParams.AssociatedPin2 = InInputPin;
	OutParams.WireThickness = 1.5f;
	OutParams.WireColor = CkStyle::Graph_Edge();
	OutParams.bDrawBubbles = false;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
