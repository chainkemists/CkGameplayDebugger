#include "CkGoapDebug_ActionCategorizer.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

// ====================================================================================================================

auto
	FCkGoapDebug_ActionCategorizer::
	ClassifyCategory(const FCkGoapDebugger_ActionInfo& InAction)
	-> ECkGoapDebug_ActionCategory
{
	// Strip a common "UCk_*_Action_" prefix so the heuristic is stable against
	// framework-level class renames. The Empire gym class names look like
	// "UCk_Empire_Action_GatherWoodFromCamp".
	auto Name = InAction.ClassName;
	int32 Idx = INDEX_NONE;
	if (Name.FindLastChar(TEXT('_'), Idx))
	{
		Name = Name.RightChop(Idx + 1);
	}

	if (Name.StartsWith(TEXT("Gather")))  { return ECkGoapDebug_ActionCategory::Gather; }
	if (Name.StartsWith(TEXT("Build")))   { return ECkGoapDebug_ActionCategory::Build; }
	if (Name.StartsWith(TEXT("Research"))){ return ECkGoapDebug_ActionCategory::Research; }
	if (Name.StartsWith(TEXT("Train")))   { return ECkGoapDebug_ActionCategory::Train; }
	if (Name.StartsWith(TEXT("Advance"))) { return ECkGoapDebug_ActionCategory::Age; }
	if (Name.StartsWith(TEXT("Trade")))   { return ECkGoapDebug_ActionCategory::Trade; }
	return ECkGoapDebug_ActionCategory::Other;
}

auto
	FCkGoapDebug_ActionCategorizer::
	CategoryLabel(ECkGoapDebug_ActionCategory InCat)
	-> FText
{
	switch (InCat)
	{
		case ECkGoapDebug_ActionCategory::Gather:   return FText::FromString(TEXT("Gather"));
		case ECkGoapDebug_ActionCategory::Build:    return FText::FromString(TEXT("Build"));
		case ECkGoapDebug_ActionCategory::Age:      return FText::FromString(TEXT("Age"));
		case ECkGoapDebug_ActionCategory::Research: return FText::FromString(TEXT("Research"));
		case ECkGoapDebug_ActionCategory::Train:    return FText::FromString(TEXT("Train"));
		case ECkGoapDebug_ActionCategory::Trade:    return FText::FromString(TEXT("Trade"));
		default:                                    return FText::FromString(TEXT("Other"));
	}
}

auto
	FCkGoapDebug_ActionCategorizer::
	CategoryColor(ECkGoapDebug_ActionCategory InCat)
	-> FLinearColor
{
	switch (InCat)
	{
		case ECkGoapDebug_ActionCategory::Gather:   return CkDebugStyle::CategoryGather;
		case ECkGoapDebug_ActionCategory::Build:    return CkDebugStyle::CategoryBuild;
		case ECkGoapDebug_ActionCategory::Research: return CkDebugStyle::CategoryResearch;
		case ECkGoapDebug_ActionCategory::Train:    return CkDebugStyle::CategoryTrain;
		case ECkGoapDebug_ActionCategory::Age:      return CkDebugStyle::CategoryAge;
		case ECkGoapDebug_ActionCategory::Trade:    return CkDebugStyle::CategoryTrade;
		default:                                    return CkDebugStyle::TextMute;
	}
}

auto
	FCkGoapDebug_ActionCategorizer::
	CategoryDisplayOrder()
	-> const TArray<ECkGoapDebug_ActionCategory>&
{
	static const TArray<ECkGoapDebug_ActionCategory> Order = {
		ECkGoapDebug_ActionCategory::Gather,
		ECkGoapDebug_ActionCategory::Build,
		ECkGoapDebug_ActionCategory::Age,
		ECkGoapDebug_ActionCategory::Research,
		ECkGoapDebug_ActionCategory::Trade,
		ECkGoapDebug_ActionCategory::Train,
		ECkGoapDebug_ActionCategory::Other,
	};
	return Order;
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	// A precondition key is considered an age gate if the tag's leaf name
	// matches the shape `IsIn<Anything>Age` — captures `IsInDarkAge`,
	// `IsInFeudalAge`, etc. without hardcoding the specific ages.
	auto DoIsAgeGateKey(const FName& InKey) -> bool
	{
		const auto Str = InKey.ToString();
		// Leaf = portion after the last '.'
		int32 DotIdx = INDEX_NONE;
		auto Leaf = Str;
		if (Str.FindLastChar(TEXT('.'), DotIdx)) { Leaf = Str.RightChop(DotIdx + 1); }
		return Leaf.StartsWith(TEXT("IsIn")) && Leaf.EndsWith(TEXT("Age"));
	}

	auto DoLabelForAgeGate(const FName& InKey) -> FString
	{
		auto Str = InKey.ToString();
		int32 DotIdx = INDEX_NONE;
		auto Leaf = Str;
		if (Str.FindLastChar(TEXT('.'), DotIdx)) { Leaf = Str.RightChop(DotIdx + 1); }
		// "IsInFeudalAge" -> "Feudal Age"
		if (Leaf.StartsWith(TEXT("IsIn")))
		{
			Leaf.RightChopInline(4);
			if (Leaf.EndsWith(TEXT("Age")))
			{
				Leaf.LeftChopInline(3);
				return Leaf + TEXT(" Age");
			}
		}
		return Leaf;
	}
}

auto
	FCkGoapDebug_ActionCategorizer::
	DiscoverTiers(
		const TArray<FCkGoapDebugger_ActionInfo>& InActions,
		const TArray<FCkGoapDebugger_GoalInfo>& InGoals)
	-> TArray<FCkGoapDebug_Tier>
{
	// Collect all age-gate tag keys that appear as a precondition on any
	// action or as a condition on any goal.
	auto GateKeys = TSet<FName>{};
	for (const auto& A : InActions)
	{
		for (const auto& P : A.Preconditions)
		{
			if (DoIsAgeGateKey(P.Key.GetTagName())) { GateKeys.Add(P.Key.GetTagName()); }
		}
		for (const auto& E : A.Effects)
		{
			if (DoIsAgeGateKey(E.Key.GetTagName())) { GateKeys.Add(E.Key.GetTagName()); }
		}
	}
	for (const auto& G : InGoals)
	{
		for (const auto& C : G.Conditions)
		{
			if (DoIsAgeGateKey(C.Key.GetTagName())) { GateKeys.Add(C.Key.GetTagName()); }
		}
	}

	// Order gates by dependency depth: a gate A depends on B if there is an
	// action with Effect A whose Preconditions include B (also an age gate).
	// Topo sort: start with gates that have no age-gate predecessors.
	auto EdgesFromPredecessor = TMap<FName, TArray<FName>>{};
	for (const auto& A : InActions)
	{
		for (const auto& E : A.Effects)
		{
			const auto EffKey = E.Key.GetTagName();
			if (NOT GateKeys.Contains(EffKey)) { continue; }
			for (const auto& P : A.Preconditions)
			{
				const auto PreKey = P.Key.GetTagName();
				if (GateKeys.Contains(PreKey) && PreKey != EffKey)
				{
					EdgesFromPredecessor.FindOrAdd(PreKey).AddUnique(EffKey);
				}
			}
		}
	}

	// Compute in-degree for each gate
	auto InDegree = TMap<FName, int32>{};
	for (const auto& K : GateKeys) { InDegree.FindOrAdd(K, 0); }
	for (const auto& Pair : EdgesFromPredecessor)
	{
		for (const auto& To : Pair.Value) { InDegree.FindOrAdd(To)++; }
	}

	auto Queue = TArray<FName>{};
	for (const auto& Pair : InDegree)
	{
		if (Pair.Value == 0) { Queue.Add(Pair.Key); }
	}
	// Stable sort for determinism when multiple roots exist.
	Queue.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	auto Ordered = TArray<FName>{};
	auto Head = 0;
	while (Head < Queue.Num())
	{
		const auto K = Queue[Head++];
		Ordered.Add(K);
		if (const auto* Successors = EdgesFromPredecessor.Find(K))
		{
			for (const auto& S : *Successors)
			{
				auto& D = InDegree.FindOrAdd(S);
				if (--D == 0) { Queue.Add(S); }
			}
		}
	}
	// If any cycles leave nodes unvisited, append them at the end to avoid
	// losing data in the UI.
	for (const auto& K : GateKeys)
	{
		if (NOT Ordered.Contains(K)) { Ordered.Add(K); }
	}

	auto Result = TArray<FCkGoapDebug_Tier>{};
	// Tier 0 is always "no gate required".
	Result.Add({0, TEXT("Base (no gate)"), NAME_None});
	for (auto i = 0; i < Ordered.Num(); ++i)
	{
		auto T = FCkGoapDebug_Tier{};
		T.Index = i + 1;
		T.GateTagKey = Ordered[i];
		T.Label = DoLabelForAgeGate(Ordered[i]);
		Result.Add(T);
	}
	return Result;
}

auto
	FCkGoapDebug_ActionCategorizer::
	TierOfAction(
		const FCkGoapDebugger_ActionInfo& InAction,
		const TArray<FCkGoapDebug_Tier>& InTiers)
	-> int32
{
	// Deepest age-gate mentioned in preconditions wins.
	auto Best = 0;
	for (const auto& P : InAction.Preconditions)
	{
		for (const auto& T : InTiers)
		{
			if (T.Index == 0) { continue; }
			if (T.GateTagKey == P.Key.GetTagName()) { Best = FMath::Max(Best, T.Index); }
		}
	}
	return Best;
}

auto
	FCkGoapDebug_ActionCategorizer::
	TierOfGoal(
		const FCkGoapDebugger_GoalInfo& InGoal,
		const TArray<FCkGoapDebug_Tier>& InTiers)
	-> int32
{
	auto Best = 0;
	for (const auto& C : InGoal.Conditions)
	{
		for (const auto& T : InTiers)
		{
			if (T.Index == 0) { continue; }
			if (T.GateTagKey == C.Key.GetTagName()) { Best = FMath::Max(Best, T.Index); }
		}
	}
	return Best;
}

// ====================================================================================================================
