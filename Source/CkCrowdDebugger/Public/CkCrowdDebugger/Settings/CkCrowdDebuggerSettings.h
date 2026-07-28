#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkCrowdDebuggerSettings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig, meta = (DisplayName = "Ck Crowd Debugger"))
class CKCROWDDEBUGGER_API UCkCrowdDebuggerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

	UPROPERTY(Config, EditAnywhere, Category = "Viewport",
		meta = (
			ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0",
			ToolTip = "Opacity of the path-network overlay. 0 disables rendering."))
	float PathNetworkOpacity = 0.35f;
};

// --------------------------------------------------------------------------------------------------------------------
