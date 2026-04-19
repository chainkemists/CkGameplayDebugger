#pragma once

#include "CkSettings/UserSettings/CkUserSettings.h"

#include "CkDebuggerStyleSettings.generated.h"

// ====================================================================================================================
// Editor-time tunables for the Ck debugger UI (GOAP, SM, future).
// Displayed in Editor Preferences under "CkGameplayDebugger → GOAP".
// All values are read live via GetDefault<UCkDebuggerStyleSettings>().
// ====================================================================================================================

UCLASS(meta = (DisplayName = "GOAP"))
class CKDEBUGGERCOMMON_API UCkDebuggerStyleSettings : public UCk_Plugin_UserSettings_UE
{
	GENERATED_BODY()

public:
	UCkDebuggerStyleSettings();

	virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }
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

	// ----- Typography ---------------------------------------------------------
	// Global scale. Per-widget sizes below override these where needed.
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

	// ----- Pane Headings ------------------------------------------------------
	// Every pane header (World State, Action Details, Failure Analysis, Plan
	// History, plan strip "PLAN", goal name, node titles, node meta rows) is
	// a separate tunable so nothing sneaks through with a hardcoded size/color.
	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings", meta = (ClampMin = 6, ClampMax = 24))
	int32 PaneHeadingFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings")
	FLinearColor PaneHeadingColor;

	// ----- Plan Strip ---------------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_TitleFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_MetaFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalLabelFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalNameFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepNameFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepCostFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepStateFontSize;

	// Step pill color trios. Opaque fills, not tints.
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Fill_Pending;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Border_Pending;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Badge_Pending;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Fill_Active;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Border_Active;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Badge_Active;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Fill_Done;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Border_Done;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Badge_Done;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalFill;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalBorder;

	// ----- Graph Nodes --------------------------------------------------------
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

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeTitleFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeCostFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeMetaFontSize;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.5, ClampMax = 5))
	float NodeBorderThickness;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.1, ClampMax = 1))
	float NodeInactiveOpacity;
};

// ====================================================================================================================
