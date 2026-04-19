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
		case ECkGoapDebug_ActionCategory::Gather:   return CkDebugStyle::CategoryGather();
		case ECkGoapDebug_ActionCategory::Build:    return CkDebugStyle::CategoryBuild();
		case ECkGoapDebug_ActionCategory::Research: return CkDebugStyle::CategoryResearch();
		case ECkGoapDebug_ActionCategory::Train:    return CkDebugStyle::CategoryTrain();
		case ECkGoapDebug_ActionCategory::Age:      return CkDebugStyle::CategoryAge();
		case ECkGoapDebug_ActionCategory::Trade:    return CkDebugStyle::CategoryTrade();
		default:                                    return CkDebugStyle::TextMute();
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
	// Leaf name of a fully-qualified tag: "Goap.WS.Empire.IsInFeudalAge" -> "IsInFeudalAge".
	auto DoLeafName(const FName& InKey) -> FString
	{
		auto Str = InKey.ToString();
		int32 DotIdx = INDEX_NONE;
		if (Str.FindLastChar(TEXT('.'), DotIdx)) { return Str.RightChop(DotIdx + 1); }
		return Str;
	}
}

auto
	FCkGoapDebug_ActionCategorizer::
	DiscoverTiers(
		const TArray<FCkGoapDebugger_ActionInfo>& InActions,
		const TArray<FCkGoapDebugger_GoalInfo>& /*InGoals*/)
	-> TArray<FCkGoapDebug_Tier>
{
	// Generic tier discovery, no domain vocabulary assumptions:
	//
	//   1. Collect tags that appear as BOTH a precondition AND an effect
	//      anywhere in the action set. These are the only tags that can
	//      represent a progression step (a fixed transient resource flag
	//      like "HasEnoughWood" is handled below).
	//
	//   2. Gate = candidate tag produced by EXACTLY ONE action. This is
	//      the key signal: single-producer tags are one-way unlocks
	//      (BuildBarracks, AdvanceToFeudalAge), while multi-producer tags
	//      (GatherWood + GatherWoodFromCamp both setting HasEnoughWood)
	//      are transient resource flags and shouldn't define tiers.
	//
	//   3. Build a DAG between gates: A -> B when an action has Effect=B
	//      and Precondition=A, with both A and B gates.
	//
	//   4. Longest-path depth from topological sort assigns each gate a
	//      tier index. Gates at the same depth share a tier index but
	//      become separate adjacent columns.
	//
	//   5. Tier 0 is always "Base" — actions with no gate preconditions.

	// 1 + 2. Collect candidates with single producer.
	auto EffProducerCount = TMap<FName, int32>{};
	auto PreTags = TSet<FName>{};
	for (const auto& A : InActions)
	{
		for (const auto& P : A.Preconditions) { PreTags.Add(P.Key.GetTagName()); }
		for (const auto& E : A.Effects)       { EffProducerCount.FindOrAdd(E.Key.GetTagName(), 0)++; }
	}

	auto GateKeys = TSet<FName>{};
	for (const auto& Pair : EffProducerCount)
	{
		if (Pair.Value == 1 && PreTags.Contains(Pair.Key))
		{
			GateKeys.Add(Pair.Key);
		}
	}

	// 3. Build gate-to-gate DAG.
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

	auto InDegree = TMap<FName, int32>{};
	for (const auto& K : GateKeys) { InDegree.FindOrAdd(K, 0); }
	for (const auto& Pair : EdgesFromPredecessor)
	{
		for (const auto& To : Pair.Value) { InDegree.FindOrAdd(To)++; }
	}

	// 4. Topological sort with longest-path depth assignment.
	auto Queue = TArray<FName>{};
	auto GateDepth = TMap<FName, int32>{};
	for (const auto& Pair : InDegree)
	{
		if (Pair.Value == 0) { Queue.Add(Pair.Key); GateDepth.Add(Pair.Key, 0); }
	}
	Queue.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	auto Head = 0;
	while (Head < Queue.Num())
	{
		const auto K = Queue[Head++];
		const auto KDepth = GateDepth.FindOrAdd(K, 0);
		if (const auto* Successors = EdgesFromPredecessor.Find(K))
		{
			for (const auto& S : *Successors)
			{
				auto& SDepth = GateDepth.FindOrAdd(S, 0);
				SDepth = FMath::Max(SDepth, KDepth + 1);
				auto& D = InDegree.FindOrAdd(S);
				if (--D == 0) { Queue.Add(S); }
			}
		}
	}
	// Cycles leave nodes unvisited — drop them into the last known depth so
	// the UI still shows them rather than silently hiding.
	for (const auto& K : GateKeys)
	{
		if (NOT GateDepth.Contains(K)) { GateDepth.Add(K, 0); }
	}

	// 5. Emit tiers. Tier 0 = "Base"; depth-N gates become tiers in depth
	//    order. Gates sharing a depth are sorted lexicographically for
	//    deterministic column order.
	auto Result = TArray<FCkGoapDebug_Tier>{};
	Result.Add({0, TEXT("Base"), NAME_None});

	auto MaxDepth = 0;
	for (const auto& Pair : GateDepth) { MaxDepth = FMath::Max(MaxDepth, Pair.Value); }

	for (auto D = 0; D <= MaxDepth; ++D)
	{
		auto GatesAtDepth = TArray<FName>{};
		for (const auto& Pair : GateDepth)
		{
			if (Pair.Value == D) { GatesAtDepth.Add(Pair.Key); }
		}
		GatesAtDepth.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });
		for (const auto& Gate : GatesAtDepth)
		{
			FCkGoapDebug_Tier T;
			T.Index = Result.Num();
			T.GateTagKey = Gate;
			T.Label = DoLeafName(Gate);
			Result.Add(T);
		}
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
