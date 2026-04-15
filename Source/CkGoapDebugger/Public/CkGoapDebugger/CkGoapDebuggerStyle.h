#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// GOAP DEBUGGER STYLE — Centralized colors and layout constants
// ====================================================================================================================

namespace CkGoapDebuggerStyle
{
	// Status colors
	inline const auto StatusIdle = FLinearColor{0.4f, 0.4f, 0.4f, 1.0f};
	inline const auto StatusPlanning = FLinearColor{0.2f, 0.5f, 1.0f, 1.0f};
	inline const auto StatusPlanFound = FLinearColor{0.13f, 0.77f, 0.37f, 1.0f};
	inline const auto StatusPlanFailed = FLinearColor{0.94f, 0.27f, 0.27f, 1.0f};
	inline const auto StatusCostThreshold = FLinearColor{0.96f, 0.62f, 0.04f, 1.0f};

	// World state colors
	inline const auto WorldStateTrue = FLinearColor{0.13f, 0.77f, 0.37f, 1.0f};
	inline const auto WorldStateFalse = FLinearColor{0.94f, 0.27f, 0.27f, 1.0f};

	// Plan action colors
	inline const auto PlanActionBackground = FLinearColor{0.08f, 0.12f, 0.18f, 1.0f};
	inline const auto PlanActionBorder = FLinearColor{0.2f, 0.5f, 1.0f, 0.6f};
	inline const auto PlanCostText = FLinearColor{0.96f, 0.62f, 0.04f, 1.0f};

	// Goal colors
	inline const auto GoalActive = FLinearColor{0.13f, 0.77f, 0.37f, 1.0f};
	inline const auto GoalInactive = FLinearColor{0.4f, 0.4f, 0.4f, 1.0f};

	// Budget bar
	inline const auto BudgetNormal = FLinearColor{0.2f, 0.5f, 1.0f, 1.0f};
	inline const auto BudgetWarning = FLinearColor{0.96f, 0.62f, 0.04f, 1.0f};
	inline const auto BudgetOver = FLinearColor{0.94f, 0.27f, 0.27f, 1.0f};

	// Panel
	inline const auto PanelBackground = FLinearColor{0.02f, 0.04f, 0.08f, 1.0f};
	inline const auto SectionHeader = FLinearColor{0.7f, 0.7f, 0.7f, 1.0f};

	// Text
	inline const auto TextPrimary = FLinearColor{0.9f, 0.9f, 0.9f, 1.0f};
	inline const auto TextSecondary = FLinearColor{0.6f, 0.6f, 0.6f, 1.0f};
	inline const auto TextMuted = FLinearColor{0.4f, 0.4f, 0.4f, 1.0f};

	// Layout
	constexpr auto PanelPadding = 12.0f;
	constexpr auto SectionSpacing = 16.0f;
	constexpr auto RowHeight = 18.0f;

	// Helpers
	inline auto
	GetStatusColor(ECk_GoapPlanStatus InStatus) -> FLinearColor
	{
		switch (InStatus)
		{
		case ECk_GoapPlanStatus::Idle:                 return StatusIdle;
		case ECk_GoapPlanStatus::Planning:             return StatusPlanning;
		case ECk_GoapPlanStatus::PlanFound:             return StatusPlanFound;
		case ECk_GoapPlanStatus::PlanFailed:            return StatusPlanFailed;
		case ECk_GoapPlanStatus::CostThresholdReached: return StatusCostThreshold;
		default:                                        return StatusIdle;
		}
	}

	inline auto
	GetStatusString(ECk_GoapPlanStatus InStatus) -> FString
	{
		switch (InStatus)
		{
		case ECk_GoapPlanStatus::Idle:                 return TEXT("Idle");
		case ECk_GoapPlanStatus::Planning:             return TEXT("Planning");
		case ECk_GoapPlanStatus::PlanFound:             return TEXT("Plan Found");
		case ECk_GoapPlanStatus::PlanFailed:            return TEXT("Plan Failed");
		case ECk_GoapPlanStatus::CostThresholdReached: return TEXT("Cost Threshold");
		default:                                        return TEXT("Unknown");
		}
	}
}

// ====================================================================================================================
