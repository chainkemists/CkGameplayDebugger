#pragma once

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// What is LEFT in this namespace after the common-widget consolidation:
//   - the scheduler's timing budget and the one heat entry point built on it
//   - the group-accent table, deliberately kept local (justification below)
//   - layout constants, aliased onto the shared spacing scale so they can't drift
//
// Every background / text / status / selection token this namespace used to own is GONE — call
// sites read `CkStyle::` roles directly, so an Editor Preferences -> Ck -> Style edit moves them.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkSchedulerDebuggerStyle
{
	// The main-pass cost at which a processor stops reading as cheap. Feeding it to Get_HeatColor as
	// Value / (Budget * 2) reproduces the four-band Get_TimingColor this namespace used to hardcode:
	// cold well under budget, Warn exactly at 0.15 ms, saturated Err at 0.30 ms. The same number is
	// what the window hands SCkDebug_FrameStrip as its BudgetMs, so strip and rows agree on "slow".
	inline constexpr double TimingBudgetMs = 0.15;

	inline auto
	Get_TimingColor(
		double InTimeMs)
		-> FLinearColor
	{
		return ck::debug_axes::Get_HeatColor(static_cast<float>(InTimeMs / (TimingBudgetMs * 2.0)));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// GROUP ACCENTS — KEPT LOCAL, on purpose.
	//
	// Two reasons, either one sufficient:
	//   1. There are NINE named pipeline groups; ck::debug_axes::Get_CategoricalColor wraps at eight,
	//      so no index or key assignment can keep all nine distinguishable side by side — and these
	//      are rendered as adjacent accent bars in one tree, which is exactly where a collision reads
	//      as "these two are the same thing".
	//   2. The hues are semantic, not arbitrary buckets: destruction is warm-red, physics amber,
	//      transform green, replication violet. That is the same class of per-visualization meaning
	//      the ruling keeps local (AStar cell states, Crowd navmesh paint) rather than a categorical
	//      palette where any hue would do.
	//
	// The ungrouped fallback carries no such meaning and therefore DOES resolve from a role.
	// ----------------------------------------------------------------------------------------------------------------

	inline const FLinearColor Color_Group_PreDestruction      = FLinearColor(0.706f, 0.314f, 0.314f);
	inline const FLinearColor Color_Group_DestructionPipeline = FLinearColor(0.706f, 0.392f, 0.275f);
	inline const FLinearColor Color_Group_Gameplay            = FLinearColor(0.235f, 0.588f, 0.784f);
	inline const FLinearColor Color_Group_Physics             = FLinearColor(0.784f, 0.627f, 0.235f);
	inline const FLinearColor Color_Group_Transform           = FLinearColor(0.510f, 0.706f, 0.314f);
	inline const FLinearColor Color_Group_PostTransform       = FLinearColor(0.392f, 0.627f, 0.510f);
	inline const FLinearColor Color_Group_Replication         = FLinearColor(0.627f, 0.392f, 0.784f);
	inline const FLinearColor Color_Group_EntityLifecycle     = FLinearColor(0.392f, 0.510f, 0.706f);
	inline const FLinearColor Color_Group_Overlap             = FLinearColor(0.784f, 0.471f, 0.627f);

	inline auto
	Get_GroupColor_Default()
		-> FLinearColor
	{
		return CkStyle::None();
	}

	inline auto
	Get_GroupColor(
		const FName& InGroupName)
		-> FLinearColor
	{
		if (InGroupName.ToString().Contains(TEXT("PreDestruction")))       { return Color_Group_PreDestruction; }
		if (InGroupName.ToString().Contains(TEXT("Destruction")))          { return Color_Group_DestructionPipeline; }
		if (InGroupName.ToString().Contains(TEXT("Gameplay")))             { return Color_Group_Gameplay; }
		if (InGroupName.ToString().Contains(TEXT("Physics")))              { return Color_Group_Physics; }
		if (InGroupName.ToString().Contains(TEXT("PostTransform")))        { return Color_Group_PostTransform; }
		if (InGroupName.ToString().Contains(TEXT("Transform")))            { return Color_Group_Transform; }
		if (InGroupName.ToString().Contains(TEXT("Replication")))          { return Color_Group_Replication; }
		if (InGroupName.ToString().Contains(TEXT("EntityLifecycle")))      { return Color_Group_EntityLifecycle; }
		if (InGroupName.ToString().Contains(TEXT("Overlap")))              { return Color_Group_Overlap; }
		return Get_GroupColor_Default();
	}

	// ----------------------------------------------------------------------------------------------------------------
	// LAYOUT — aliases onto the shared spacing scale. Same numbers as before, one authority now.
	// ----------------------------------------------------------------------------------------------------------------

	inline constexpr float Padding_Small  = CkStyle::SpaceS;    // 4
	inline constexpr float Padding_Medium = CkStyle::SpaceM;    // 8
	inline constexpr float Padding_Large  = CkStyle::SpaceXL;   // 16

	inline constexpr float Node_AccentWidth = 4.0f;
}

// --------------------------------------------------------------------------------------------------------------------
