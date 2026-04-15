#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/EntityScripts/CkGoapGoal_EntityScript.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkGoap/CkGoap_Fragment.h"
#include "CkAStar/CkAStar_Fragment.h"

#include "HAL/PlatformTime.h"
#include "Engine/World.h"

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
	Info.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InHandle).ToString();

	// Current state
	if (InHandle.Has<ck::FFragment_Goap_Current>())
	{
		const auto& Current = InHandle.Get<ck::FFragment_Goap_Current>();
		Info.PlanStatus = Current.Get_PlanStatus();
		Info.PlanCost = Current.Get_PlanCost();

		for (const auto& ActionClass : Current.Get_Plan())
		{
			if (ck::IsValid(ActionClass))
			{
				Info.PlanActionNames.Add(ActionClass->GetName());
			}
		}
	}

	// World state
	if (InHandle.Has<ck::FFragment_Goap_WorldState>())
	{
		const auto& WS = InHandle.Get<ck::FFragment_Goap_WorldState>();
		Info.WorldState = WS.Get_WorldState().GetValues();
	}

	// Actions
	if (InHandle.Has<ck::FFragment_Goap_Actions>())
	{
		const auto& Actions = InHandle.Get<ck::FFragment_Goap_Actions>();
		for (const auto& ActionDef : Actions.Get_ActionDefs())
		{
			auto ActionInfo = FCkGoapDebugger_ActionInfo{};
			ActionInfo.ActionClass = ActionDef.ActionClass;
			ActionInfo.ClassName = ck::IsValid(ActionDef.ActionClass)
				? ActionDef.ActionClass->GetName() : TEXT("Invalid");
			ActionInfo.Preconditions = ActionDef.Preconditions.GetValues();
			ActionInfo.Effects = ActionDef.Effects.GetValues();
			ActionInfo.Cost = ActionDef.Cost;
			Info.Actions.Add(MoveTemp(ActionInfo));
		}
	}

	// Goals
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
			GoalInfo.Conditions = GoalDef.Conditions.GetValues();
			GoalInfo.Priority = GoalDef.Priority;
			GoalInfo.IsActiveGoal = GoalDef.GoalClass == Current.Get_ActiveGoalClass();
			Info.Goals.Add(MoveTemp(GoalInfo));
		}
	}

	// A* debug stats
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

	TrackPlanCompletion(Info);
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
	const auto* LastStatus = _LastKnownStatus.Find(EntityHash);
	const auto WasPlanning = LastStatus != nullptr && *LastStatus == ECk_GoapPlanStatus::Planning;
	const auto IsTerminal = InInfo.PlanStatus == ECk_GoapPlanStatus::PlanFound
		|| InInfo.PlanStatus == ECk_GoapPlanStatus::PlanFailed
		|| InInfo.PlanStatus == ECk_GoapPlanStatus::CostThresholdReached;

	if (WasPlanning && IsTerminal)
	{
		auto Entry = FCkGoapDebugger_HistoryEntry{};
		Entry.WallTime = FPlatformTime::Seconds();
		Entry.FrameNumber = GFrameNumber;
		Entry.FinalStatus = InInfo.PlanStatus;
		Entry.PlanLength = InInfo.PlanActionNames.Num();
		Entry.PlanCost = InInfo.PlanCost;

		auto& History = _PlanHistory.FindOrAdd(EntityHash);
		History.Add(MoveTemp(Entry));
	}

	_LastKnownStatus.FindOrAdd(EntityHash) = InInfo.PlanStatus;
}

// ====================================================================================================================
