#pragma once

#include "CkSettings/UserSettings/CkUserSettings.h"

#include "CkDebuggerStyleSettings.generated.h"

// ====================================================================================================================
// Editor-time tunables for the Ck debugger UI (GOAP, SM, future).
// Displayed in Editor Preferences under "CkGameplayDebugger → GOAP".
// All values are read live via GetDefault<UCkDebuggerStyleSettings>().
//
// Defaults are set inline on each UPROPERTY below — do NOT add a constructor;
// the inherited UCk_Plugin_UserSettings_UE only accepts FObjectInitializer.
// ====================================================================================================================

UCLASS(meta = (DisplayName = "GOAP"))
class CKDEBUGGERCOMMON_API UCkDebuggerStyleSettings : public UCk_Plugin_UserSettings_UE
{
	GENERATED_BODY()

public:
	virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }
	virtual auto GetSectionName()  const -> FName override { return TEXT("GOAP"); }

	// ----- Palette: Backgrounds — each tier slightly lighter than the last --
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor BgRoot = FLinearColor(FColor(0x0b, 0x0e, 0x13, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg1 = FLinearColor(FColor(0x11, 0x15, 0x1c, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg2 = FLinearColor(FColor(0x16, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg3 = FLinearColor(FColor(0x1b, 0x22, 0x30, 255));

	// ----- Palette: Borders --------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor Border = FLinearColor(FColor(0x23, 0x2a, 0x38, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor BorderStrong = FLinearColor(FColor(0x32, 0x39, 0x4a, 255));

	// ----- Palette: Text -----------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor Text = FLinearColor(FColor(0xe5, 0xe7, 0xed, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextDim = FLinearColor(FColor(0x8a, 0x92, 0xa4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextMute = FLinearColor(FColor(0x5a, 0x62, 0x77, 255));

	// ----- Palette: Semantic -------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Accent = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Ok = FLinearColor(FColor(0x55, 0xc4, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Err = FLinearColor(FColor(0xd6, 0x5a, 0x5a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Warn = FLinearColor(FColor(0xe6, 0xa5, 0x45, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Info = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	// ----- Palette: Action Categories ---------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryGather = FLinearColor(FColor(0x4e, 0xa8, 0x4e, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryBuild = FLinearColor(FColor(0xc2, 0x8a, 0x2a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryResearch = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrain = FLinearColor(FColor(0xc7, 0x4c, 0x4c, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryAge = FLinearColor(FColor(0xb4, 0x6f, 0xd0, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrade = FLinearColor(FColor(0xd4, 0xb1, 0x5f, 255));

	// ----- Typography --------------------------------------------------------
	// Global scale. Per-widget sizes below override these where needed.
	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH2 = 12;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH3 = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH4 = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeBody = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeSmall = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeMicro = 8;

	// ----- Pane Headings -----------------------------------------------------
	// Every pane header (World State, Action Details, Failure Analysis, Plan
	// History, plan strip "PLAN") is a separate tunable so nothing sneaks
	// through with a hardcoded size/color.
	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings", meta = (ClampMin = 6, ClampMax = 24))
	int32 PaneHeadingFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings")
	FLinearColor PaneHeadingColor = FLinearColor(FColor(0x8a, 0x92, 0xa4, 255));

	// ----- Plan Strip --------------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_TitleFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_MetaFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalLabelFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalNameFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepNameFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepCostFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepStateFontSize = 7;

	// Step pill color trios. Opaque fills, not tints — flat translucent
	// overlays read as a saturated wash, opaque gives depth.
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Fill_Pending = FLinearColor(FColor(0x16, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Border_Pending = FLinearColor(FColor(0x23, 0x2a, 0x38, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Badge_Pending = FLinearColor(FColor(0x1b, 0x22, 0x30, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Fill_Active = FLinearColor(FColor(0x2a, 0x22, 0x0a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Border_Active = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Badge_Active = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Fill_Done = FLinearColor(FColor(0x10, 0x22, 0x17, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Border_Done = FLinearColor(FColor(0x55, 0xc4, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Badge_Done = FLinearColor(FColor(0x55, 0xc4, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalFill = FLinearColor(FColor(0x2a, 0x22, 0x0a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalBorder = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	// ----- Graph Nodes -------------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeFill_Inactive = FLinearColor(FColor(0x16, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeBorder_Inactive = FLinearColor(FColor(0x23, 0x2a, 0x38, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeFill_InPlan = FLinearColor(FColor(0x14, 0x23, 0x35, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeBorder_InPlan = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_Goal = FLinearColor(FColor(0x2a, 0x22, 0x0a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeBorder_Goal = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_GoalInactive = FLinearColor(FColor(0x15, 0x13, 0x08, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeTitleFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeCostFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeMetaFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.5, ClampMax = 5))
	float NodeBorderThickness = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.1, ClampMax = 1))
	float NodeInactiveOpacity = 0.55f;
};

// ====================================================================================================================
