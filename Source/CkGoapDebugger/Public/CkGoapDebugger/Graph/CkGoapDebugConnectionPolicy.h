#pragma once

#include "CkDebuggerCommon/Graph/CkDebugConnectionPolicyBase.h"
#include "CoreMinimal.h"

// ====================================================================================================================

class UCkGoapDebugGraph;

// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugConnectionPolicy : public FCkDebugConnectionPolicyBase
{
public:
	FCkGoapDebugConnectionPolicy(
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

private:
	UCkGoapDebugGraph* _Graph = nullptr;
};

// ====================================================================================================================
