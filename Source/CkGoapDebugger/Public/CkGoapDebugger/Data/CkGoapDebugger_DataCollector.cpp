#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/EntityScripts/CkGoapGoal_EntityScript.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkGoap/CkGoap_Fragment.h"
#include "CkAStar/CkAStar_Fragment.h"
#include "CkLabel/CkLabel_Utils.h"

#include "HAL/PlatformTime.h"
#include "Engine/World.h"

// ====================================================================================================================
// LOCAL HELPERS — boolean GOAP semantics
// ====================================================================================================================
// "Effect helps precondition": same key AND same required value. That's the
// whole rule — there's no direction (Set vs Add), no operator (==, >=, <=).
// ====================================================================================================================

namespace
{
	auto EffectHelpsCondition(
		const FCkGoapDebugger_Effect& InEffect,
		const FCkGoapDebugger_Condition& InCondition)
		-> bool
	{
		return InEffect.Key == InCondition.Key && InEffect.Value == InCondition.Value;
	}

	// Regressive reachability walker — builds a flat DFS of condition →
	// candidate actions → their preconditions → ... bounded by depth and a
	// visited-set of action class names in the current path.
	struct FPlanTreeBuilder
	{
		const FCkGoapDebugger_GoapInfo* Info = nullptr;
		TArray<FCkGoapDebugger_PlanTreeNode>* Out = nullptr;
		int32* DeepUnreachableOut = nullptr;
		TArray<FString> ActionStack;

		static constexpr int32 MaxDepth = 10;

		auto FindCurrentValue(const FGameplayTag& InKey, bool& OutValue) const -> bool
		{
			for (const auto& E : Info->WorldState)
			{
				if (E.Key == InKey) { OutValue = E.Value; return true; }
			}
			return false;
		}

		auto VisitCondition(const FCkGoapDebugger_Condition& InCond, int32 InDepth) -> void
		{
			auto Node = FCkGoapDebugger_PlanTreeNode{};
			Node.Depth = InDepth;
			Node.Kind = ECkGoapDebugger_TreeNodeKind::Condition;
			Node.Condition = InCond;
			Node.HasCurrentValue = FindCurrentValue(InCond.Key, Node.CurrentValue);
			Node.IsSatisfiedByWorldState = Node.HasCurrentValue && Node.CurrentValue == InCond.Value;

			auto Candidates = TArray<const FCkGoapDebugger_ActionInfo*>{};
			for (const auto& Action : Info->Actions)
			{
				for (const auto& Effect : Action.Effects)
				{
					if (EffectHelpsCondition(Effect, InCond))
					{
						Candidates.Add(&Action);
						break;
					}
				}
			}
			Candidates.Sort([](const FCkGoapDebugger_ActionInfo& A, const FCkGoapDebugger_ActionInfo& B)
			{
				return A.Cost < B.Cost;
			});

			Node.CandidateActionCount = Candidates.Num();
			Node.IsUnreachable = NOT Node.IsSatisfiedByWorldState && Candidates.Num() == 0;

			if (Node.IsUnreachable && DeepUnreachableOut != nullptr)
			{
				++(*DeepUnreachableOut);
			}

			Out->Add(Node);

			if (Node.IsSatisfiedByWorldState || Candidates.Num() == 0) { return; }

			if (InDepth >= MaxDepth)
			{
				auto Limit = FCkGoapDebugger_PlanTreeNode{};
				Limit.Depth = InDepth + 1;
				Limit.Kind = ECkGoapDebugger_TreeNodeKind::Action;
				Limit.Note = TEXT("(depth limit — chain continues but not shown)");
				Out->Add(Limit);
				return;
			}

			for (const auto* C : Candidates)
			{
				VisitAction(*C, InDepth + 1);
			}
		}

		auto VisitAction(const FCkGoapDebugger_ActionInfo& InAction, int32 InDepth) -> void
		{
			auto Node = FCkGoapDebugger_PlanTreeNode{};
			Node.Depth = InDepth;
			Node.Kind = ECkGoapDebugger_TreeNodeKind::Action;
			Node.ActionClassName = InAction.ClassName;
			Node.ActionCost = InAction.Cost;

			if (ActionStack.Contains(InAction.ClassName))
			{
				Node.Note = TEXT("(cycle — action already on current chain)");
				Out->Add(Node);
				return;
			}

			if (InAction.Preconditions.Num() == 0)
			{
				Node.Note = TEXT("(no preconditions — always fireable)");
			}

			Out->Add(Node);

			ActionStack.Push(InAction.ClassName);
			for (const auto& Pre : InAction.Preconditions)
			{
				VisitCondition(Pre, InDepth + 1);
			}
			ActionStack.Pop();
		}
	};
}

// ====================================================================================================================

auto
	FCkGoapDebugger_DataCollector::
	Collect(UWorld* InWorld)
	-> void
{
	_GoapEntities.Reset();

	if (NOT IsValid(InWorld))
	{
		return;
	}

	auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

	if (NOT ck::IsValid(TransientEntity))
	{
		return;
	}

	TransientEntity.View<ck::FFragment_Goap_Current>().ForEach(
		[this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_Goap_Current&)
		{
			auto Handle = ck::MakeHandle(InEntity, TransientEntity);
			CollectGoapEntity(Handle);
		});
}

// ====================================================================================================================

auto
	FCkGoapDebugger_DataCollector::
	CollectGoapEntity(
		FCk_Handle InHandle)
	-> void
{
	auto Info = FCkGoapDebugger_GoapInfo{};
	Info.Handle = InHandle;

	if (UCk_Utils_GameplayLabel_UE::Has(InHandle))
	{
		const auto Label = UCk_Utils_GameplayLabel_UE::Get_Label(InHandle);
		Info.DebugName = Label.IsValid() ? Label.ToString() : TEXT("<unnamed>");
	}
	else
	{
		Info.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InHandle).ToString();
	}

	if (InHandle.Has<ck::FFragment_Goap_Current>())
	{
		const auto& Current = InHandle.Get<ck::FFragment_Goap_Current>();
		Info.PlanStatus = Current.Get_PlanStatus();
		Info.PlanCost = Current.Get_PlanCost();
		Info.PlanAttemptCount = Current.Get_PlanAttemptCount();

		for (const auto& ActionClass : Current.Get_Plan())
		{
			if (ck::IsValid(ActionClass))
			{
				Info.PlanActionNames.Add(ActionClass->GetName());
			}
		}
	}

	const auto* KeyRegistry = InHandle.Has<ck::FFragment_Goap_KeyRegistry>()
		? &InHandle.Get<ck::FFragment_Goap_KeyRegistry>().Get_Registry()
		: nullptr;

	const auto ConditionFromRaw = [&](const ck::goap::FWorldStateCondition& C) -> FCkGoapDebugger_Condition
	{
		auto Out = FCkGoapDebugger_Condition{};
		Out.Key = KeyRegistry ? KeyRegistry->GetTag(C.Key) : FGameplayTag{};
		Out.Value = C.Value;
		return Out;
	};

	const auto EffectFromRaw = [&](const ck::goap::FWorldStateEffect& E) -> FCkGoapDebugger_Effect
	{
		auto Out = FCkGoapDebugger_Effect{};
		Out.Key = KeyRegistry ? KeyRegistry->GetTag(E.Key) : FGameplayTag{};
		Out.Value = E.Value;
		return Out;
	};

	// World state — iterate registered keys and emit every one with its bool
	// value. Unlike the typed layout there's no "Unused" — every registered
	// key always has a bool (default false).
	if (InHandle.Has<ck::FFragment_Goap_WorldState>() && KeyRegistry != nullptr)
	{
		const auto& WS = InHandle.Get<ck::FFragment_Goap_WorldState>().Get_WorldState();
		for (auto KeyIdx = int32{0}; KeyIdx < KeyRegistry->Num(); ++KeyIdx)
		{
			auto Entry = FCkGoapDebugger_WorldStateEntry{};
			Entry.Key = KeyRegistry->GetTag(KeyIdx);
			Entry.Value = WS.Get(KeyIdx);
			Info.WorldState.Add(MoveTemp(Entry));
		}
	}

	if (InHandle.Has<ck::FFragment_Goap_Actions>())
	{
		const auto& Actions = InHandle.Get<ck::FFragment_Goap_Actions>();
		for (const auto& ActionDef : Actions.Get_ActionDefs())
		{
			auto ActionInfo = FCkGoapDebugger_ActionInfo{};
			ActionInfo.ActionClass = ActionDef.ActionClass;
			ActionInfo.ClassName = ck::IsValid(ActionDef.ActionClass)
				? ActionDef.ActionClass->GetName() : TEXT("Invalid");
			ActionInfo.Cost = ActionDef.Cost;
			for (const auto& Pre : ActionDef.Preconditions) { ActionInfo.Preconditions.Add(ConditionFromRaw(Pre)); }
			for (const auto& Eff : ActionDef.Effects)        { ActionInfo.Effects.Add(EffectFromRaw(Eff)); }
			Info.Actions.Add(MoveTemp(ActionInfo));
		}
	}

	if (InHandle.Has<ck::FFragment_Goap_Goals>())
	{
		const auto& Goals = InHandle.Get<ck::FFragment_Goap_Goals>();
		const auto& Current = InHandle.Get<ck::FFragment_Goap_Current>();

		for (const auto& GoalDef : Goals.Get_GoalDefs())
		{
			auto GoalInfo = FCkGoapDebugger_GoalInfo{};
			GoalInfo.GoalClass = GoalDef.GoalClass;
			GoalInfo.ClassName = ck::IsValid(GoalDef.GoalClass)
				? GoalDef.GoalClass->GetName() : TEXT("Invalid");
			GoalInfo.Priority = GoalDef.Priority;
			GoalInfo.IsActiveGoal = GoalDef.GoalClass == Current.Get_ActiveGoalClass();
			for (const auto& C : GoalDef.Conditions) { GoalInfo.Conditions.Add(ConditionFromRaw(C)); }
			Info.Goals.Add(MoveTemp(GoalInfo));
		}
	}

	if (InHandle.Has<ck::FFragment_AStar_Debug>())
	{
		const auto& Debug = InHandle.Get<ck::FFragment_AStar_Debug>();
		Info.OpenSetSize = Debug.Get_OpenSetSize();
		Info.ClosedSetSize = Debug.Get_ClosedSetSize();
		Info.IterationsThisFrame = Debug.Get_IterationsThisFrame();
		Info.TimeThisFrameMicroseconds = Debug.Get_TimeThisFrameMicroseconds();
		Info.BudgetUsagePercent = Debug.Get_BudgetUsagePercent();
	}

	if (InHandle.Has<ck::FFragment_AStar_Params>())
	{
		const auto& Params = InHandle.Get<ck::FFragment_AStar_Params>();
		Info.BudgetMicroseconds = Params.Get_BudgetMicroseconds();
	}

	if (InHandle.Has<ck::FFragment_Goap_Diagnostics>())
	{
		const auto& Diag = InHandle.Get<ck::FFragment_Goap_Diagnostics>();

		for (const auto& Cycle : Diag.Get_DependencyCycles())
		{
			auto CycleInfo = FCkGoapDebugger_CycleInfo{};
			for (const auto& Cls : Cycle.Get_ActionsInCycle())
			{
				CycleInfo.ActionNames.Add(ck::IsValid(Cls) ? Cls->GetName() : TEXT("<invalid>"));
			}
			CycleInfo.CycleConditions = Cycle.Get_CycleConditions();
			Info.Diagnostics.DependencyCycles.Add(MoveTemp(CycleInfo));
		}

		for (const auto& Pair : Diag.Get_LastUnreachableGoalConditions())
		{
			auto Cond = FCkGoapDebugger_Condition{};
			Cond.Key = Pair.Get_Key();
			Cond.Value = Pair.Get_Value();
			Info.Diagnostics.UnreachableGoalConditions.Add(MoveTemp(Cond));
		}

		if (ck::IsValid(Diag.Get_LastFailedGoalClass()))
		{
			Info.Diagnostics.LastFailedGoalName = Diag.Get_LastFailedGoalClass()->GetName();
		}
	}

	// Failure analysis — per goal condition status + plan tree.
	{
		const auto* GoalToAnalyze = static_cast<const FCkGoapDebugger_GoalInfo*>(nullptr);
		for (const auto& Goal : Info.Goals)
		{
			if (Goal.IsActiveGoal) { GoalToAnalyze = &Goal; break; }
		}
		if (GoalToAnalyze == nullptr)
		{
			for (const auto& Goal : Info.Goals)
			{
				if (GoalToAnalyze == nullptr || Goal.Priority > GoalToAnalyze->Priority)
				{
					GoalToAnalyze = &Goal;
				}
			}
		}

		if (GoalToAnalyze != nullptr)
		{
			Info.FailureAnalysis.HasActiveGoal = GoalToAnalyze->IsActiveGoal;
			Info.FailureAnalysis.ActiveGoalClassName = GoalToAnalyze->ClassName;

			for (const auto& Cond : GoalToAnalyze->Conditions)
			{
				auto Analysis = FCkGoapDebugger_ConditionAnalysis{};
				Analysis.Condition = Cond;

				for (const auto& Entry : Info.WorldState)
				{
					if (Entry.Key == Cond.Key)
					{
						Analysis.HasCurrentValue = true;
						Analysis.CurrentValue = Entry.Value;
						break;
					}
				}

				Analysis.IsSatisfiedByWorldState = Analysis.HasCurrentValue && Analysis.CurrentValue == Cond.Value;

				for (const auto& Action : Info.Actions)
				{
					for (const auto& Effect : Action.Effects)
					{
						if (EffectHelpsCondition(Effect, Cond))
						{
							Analysis.RelevantActionClassNames.Add(Action.ClassName);
							break;
						}
					}
				}

				Analysis.IsUnreachable =
					NOT Analysis.IsSatisfiedByWorldState
					&& Analysis.RelevantActionClassNames.Num() == 0;

				if (NOT Analysis.IsSatisfiedByWorldState)
				{
					++Info.FailureAnalysis.UnsatisfiedConditionCount;
				}
				if (Analysis.IsUnreachable)
				{
					++Info.FailureAnalysis.UnreachableConditionCount;
				}

				Info.FailureAnalysis.ConditionAnalyses.Add(MoveTemp(Analysis));
			}

			auto Builder = FPlanTreeBuilder{};
			Builder.Info = &Info;
			Builder.Out = &Info.FailureAnalysis.PlanTree;
			Builder.DeepUnreachableOut = &Info.FailureAnalysis.DeepUnreachableNodeCount;
			for (const auto& Cond : GoalToAnalyze->Conditions)
			{
				Builder.VisitCondition(Cond, 0);
			}
		}
	}

	TrackPlanCompletion(Info);
	TrackSearchProgress(Info);
	_GoapEntities.Add(MoveTemp(Info));
}

// ====================================================================================================================

auto
	FCkGoapDebugger_DataCollector::
	TrackPlanCompletion(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	const auto EntityHash = GetTypeHash(InInfo.Handle);

	const auto IsTerminal = InInfo.PlanStatus == ECk_GoapPlanStatus::PlanFound
		|| InInfo.PlanStatus == ECk_GoapPlanStatus::PlanFailed
		|| InInfo.PlanStatus == ECk_GoapPlanStatus::CostThresholdReached;

	const auto LastRecorded = _LastRecordedAttemptCount.FindRef(EntityHash);
	if (IsTerminal && InInfo.PlanAttemptCount > LastRecorded && InInfo.PlanAttemptCount > 0)
	{
		auto Entry = FCkGoapDebugger_HistoryEntry{};
		Entry.WallTime = FPlatformTime::Seconds();
		Entry.FrameNumber = GFrameNumber;
		Entry.FinalStatus = InInfo.PlanStatus;
		Entry.PlanLength = InInfo.PlanActionNames.Num();
		Entry.PlanCost = InInfo.PlanCost;
		Entry.TotalIterations = InInfo.IterationsThisFrame;
		Entry.TotalTimeMicroseconds = InInfo.TimeThisFrameMicroseconds;
		Entry.Snapshot = InInfo;

		auto& History = _PlanHistory.FindOrAdd(EntityHash);
		History.Add(MoveTemp(Entry));

		_LastRecordedAttemptCount.FindOrAdd(EntityHash) = InInfo.PlanAttemptCount;
	}

	_LastKnownStatus.FindOrAdd(EntityHash) = InInfo.PlanStatus;
	_LastKnownAttemptCount.FindOrAdd(EntityHash) = InInfo.PlanAttemptCount;
}

// ====================================================================================================================

auto
	FCkGoapDebugger_DataCollector::
	TrackSearchProgress(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	const auto EntityHash = GetTypeHash(InInfo.Handle);
	auto& Log = _SearchLog.FindOrAdd(EntityHash);

	const auto PreviousAttempt = _LastKnownAttemptCount.FindRef(EntityHash);
	if (InInfo.PlanAttemptCount != PreviousAttempt)
	{
		Log.Reset();
	}

	if (InInfo.PlanStatus == ECk_GoapPlanStatus::Planning && InInfo.IterationsThisFrame > 0)
	{
		auto Snap = FCkGoapDebugger_SearchSnapshot{};
		Snap.FrameNumber = GFrameNumber;
		Snap.IterationsThisFrame = InInfo.IterationsThisFrame;
		Snap.OpenSetSize = InInfo.OpenSetSize;
		Snap.ClosedSetSize = InInfo.ClosedSetSize;
		Snap.TimeThisFrameMicroseconds = InInfo.TimeThisFrameMicroseconds;
		Snap.BudgetUsagePercent = InInfo.BudgetUsagePercent;
		Snap.Status = InInfo.PlanStatus;
		Log.Add(MoveTemp(Snap));

		if (Log.Num() > SearchLog_MaxEntries)
		{
			Log.RemoveAt(0, Log.Num() - SearchLog_MaxEntries);
		}
	}
}
