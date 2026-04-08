#pragma once

#include "CkDebuggerCommon/Graph/CkDebugConnectionPolicyBase.h"
#include "CoreMinimal.h"

class UCkSmDebugGraph;

// --------------------------------------------------------------------------------------------------------------------
// SM-specific connection drawing policy.
// Inherits straight-line+arrow drawing and node-to-node geometry from the base class.
// Adds: transition badge geometry resolution, scrub/flash wire highlights.
// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API FCkSmDebugConnectionPolicy : public FCkDebugConnectionPolicyBase
{
public:
	FCkSmDebugConnectionPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj);

	virtual auto DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams) -> void override;

	virtual auto DetermineLinkGeometry(
		FArrangedChildren& InArrangedNodes,
		TSharedRef<SWidget>& InOutputPinWidget,
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FArrangedWidget*& OutStartWidgetGeometry,
		FArrangedWidget*& OutEndWidgetGeometry) -> void override;

private:
	UCkSmDebugGraph* _Graph = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
