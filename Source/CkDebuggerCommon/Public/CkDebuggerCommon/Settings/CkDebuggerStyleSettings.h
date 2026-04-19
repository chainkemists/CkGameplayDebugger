#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "CkDebuggerStyleSettings.generated.h"

// ====================================================================================================================
// Editor-time tunables for the Ck debugger UI (GOAP, SM, future).
// Displayed in Project Settings under "CkFoundation → GOAP".
// All values are read live via GetDefault<UCkDebuggerStyleSettings>().
// ====================================================================================================================

UCLASS(Config = CkFoundation, DefaultConfig, meta = (DisplayName = "GOAP Debugger"))
class CKDEBUGGERCOMMON_API UCkDebuggerStyleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCkDebuggerStyleSettings();

	virtual auto GetCategoryName() const -> FName override { return TEXT("CkFoundation"); }
	virtual auto GetSectionName()  const -> FName override { return TEXT("GOAP"); }

	// ----- Palette: Backgrounds -----
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor BgRoot;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg1;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg2;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg3;

	// ----- Palette: Borders -----
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor Border;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor BorderStrong;

	// ----- Palette: Text -----
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor Text;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextDim;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextMute;

	// ----- Palette: Semantic -----
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Accent;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Ok;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Err;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Warn;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Info;

	// ----- Palette: Action Categories -----
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryGather;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryBuild;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryResearch;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrain;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryAge;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrade;

	// ----- Typography -----
	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH2;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH3;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH4;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeBody;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeSmall;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeMicro;

	// ----- Graph Nodes -----
	// Split fill/border pairs rather than single "tint + alpha" so the user
	// gets direct control over the final rendered look without fighting alpha
	// blending. Defaults give flat-looking nodes opacity / depth.
	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeFill_Inactive;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeBorder_Inactive;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeFill_InPlan;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeBorder_InPlan;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_Goal;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeBorder_Goal;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_GoalInactive;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.5, ClampMax = 5))
	float NodeBorderThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.1, ClampMax = 1))
	float NodeInactiveOpacity;
};

// ====================================================================================================================
