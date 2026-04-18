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

	auto
	Get_SearchLog() const -> const TMap<uint32, TArray<FCkGoapDebugger_SearchSnapshot>>& { return _SearchLog; }

private:
	auto
	CollectGoapEntity(
		FCk_Handle InHandle) -> void;

	auto
	TrackPlanCompletion(
		const FCkGoapDebugger_GoapInfo& InInfo) -> void;

	auto
	TrackSearchProgress(
		const FCkGoapDebugger_GoapInfo& InInfo) -> void;

private:
	TArray<FCkGoapDebugger_GoapInfo> _GoapEntities;
	TMap<uint32, TArray<FCkGoapDebugger_HistoryEntry>> _PlanHistory;
	TMap<uint32, ECk_GoapPlanStatus> _LastKnownStatus;
	TMap<uint32, int32> _LastKnownAttemptCount;
	TMap<uint32, int32> _LastRecordedAttemptCount;

	// Rolling per-frame snapshots of in-progress searches. Capped at
	// SearchLog_MaxEntries per entity so the UI always has recent context
	// without unbounded memory growth during very long searches.
	TMap<uint32, TArray<FCkGoapDebugger_SearchSnapshot>> _SearchLog;
	static constexpr int32 SearchLog_MaxEntries = 120;
};

// ====================================================================================================================
