#pragma once

#include "CkDebuggerCommon/Graph/CkDebugConnectionPolicyBase.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class CKSCHEDULERDEBUGGER_API FCkSchedulerDebugConnectionPolicy : public FCkDebugConnectionPolicyBase
{
public:
	FCkSchedulerDebugConnectionPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements);

	virtual auto DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams) -> void override;
};

// --------------------------------------------------------------------------------------------------------------------
