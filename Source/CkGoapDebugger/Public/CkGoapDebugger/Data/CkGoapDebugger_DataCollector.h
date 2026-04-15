#pragma once

#include "CkGoapDebugger_Types.h"
#include "CkEcs/Handle/CkHandle.h"

// ====================================================================================================================

class FCkGoapDebugger_DataCollector
{
public:
	auto
	Collect(UWorld* InWorld) -> void;

	auto
	Get_AllGoapEntities() const -> const TArray<FCkGoapDebugger_GoapInfo>& { return _GoapEntities; }

	auto
	Get_PlanHistory() const -> const TMap<uint32, TArray<FCkGoapDebugger_HistoryEntry>>& { return _PlanHistory; }

private:
	auto
	CollectGoapEntity(
		FCk_Handle InHandle) -> void;

	auto
	TrackPlanCompletion(
		const FCkGoapDebugger_GoapInfo& InInfo) -> void;

private:
	TArray<FCkGoapDebugger_GoapInfo> _GoapEntities;
	TMap<uint32, TArray<FCkGoapDebugger_HistoryEntry>> _PlanHistory;
	TMap<uint32, ECk_GoapPlanStatus> _LastKnownStatus;
};

// ====================================================================================================================
