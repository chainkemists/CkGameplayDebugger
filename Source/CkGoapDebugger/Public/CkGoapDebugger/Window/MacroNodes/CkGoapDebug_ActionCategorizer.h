#pragma once

#include "CoreMinimal.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

// ====================================================================================================================
// Pure helpers for classifying GOAP actions into visual groupings. No Slate.
// ====================================================================================================================

enum class ECkGoapDebug_ActionCategory : uint8
{
	Gather,
	Build,
	Age,
	Research,
	Train,
	Trade,
	Other,
};

// A "tier" groups actions by the deepest age-gate-like precondition they require.
// Tiers are discovered dynamically from the world-state keys: we treat tags that
// look like `IsIn<Something>Age` as age gates ordered by dependency depth. This
// keeps the panel usable on any GOAP graph, not just Empire.
struct FCkGoapDebug_Tier
{
	// Stable index (0 = base, higher = later). Lower tiers have no gate.
	int32 Index = 0;

	// Human-readable label, e.g. "Dark Age", "Feudal Age", or "Tier 0" when
	// the key doesn't end in "Age".
	FString Label;

	// The precondition tag key that gates this tier, empty for tier 0.
	FName GateTagKey = NAME_None;
};

// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebug_ActionCategorizer
{
public:
	// Classify by class-name heuristic — matches the AS-side `cat` field
	// shape: GatherX, BuildX, AdvanceToXAge, ResearchX, TrainX, TradeX.
	static auto ClassifyCategory(const FCkGoapDebugger_ActionInfo& InAction) -> ECkGoapDebug_ActionCategory;

	static auto CategoryLabel(ECkGoapDebug_ActionCategory InCat) -> FText;
	static auto CategoryColor(ECkGoapDebug_ActionCategory InCat) -> FLinearColor;
	static auto CategoryDisplayOrder() -> const TArray<ECkGoapDebug_ActionCategory>&;

	// Discover age-gate tiers from the union of all preconditions across the
	// given actions + goals. Returned list is deduped and ordered by dependency
	// depth (producers → consumers). Tier 0 = "no gate required".
	static auto DiscoverTiers(
		const TArray<FCkGoapDebugger_ActionInfo>& InActions,
		const TArray<FCkGoapDebugger_GoalInfo>& InGoals) -> TArray<FCkGoapDebug_Tier>;

	// Compute the tier an action lives in = the deepest age-gate in its preconditions.
	static auto TierOfAction(
		const FCkGoapDebugger_ActionInfo& InAction,
		const TArray<FCkGoapDebug_Tier>& InTiers) -> int32;

	// Compute the tier a goal lives in = the deepest age-gate in its conditions.
	static auto TierOfGoal(
		const FCkGoapDebugger_GoalInfo& InGoal,
		const TArray<FCkGoapDebug_Tier>& InTiers) -> int32;
};

// ====================================================================================================================
