#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

// ====================================================================================================================

enum class ECk_GoapDebugger_ViewMode : uint8
{
	Live,
	Scrub
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapListChanged, const TArray<FCkGoapDebugger_GoapInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapDataRefreshed, const FCkGoapDebugger_GoapInfo*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapSelectedEntityChanged, FCk_Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapPausedChanged, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapViewModeChanged, ECk_GoapDebugger_ViewMode);

// ====================================================================================================================

class FCkGoapDebugger_ViewModel
{
public:
	auto
	Tick(UWorld* InWorld, float InDeltaTime) -> void;

	// ----------------------------------------------------------------------------------------------------------------
	// SELECTION
	// ----------------------------------------------------------------------------------------------------------------

	auto
	Set_SelectedEntityHandle(FCk_Handle InHandle) -> void;

	auto
	Get_SelectedEntityHandle() const -> FCk_Handle { return _SelectedEntityHandle; }

	auto
	Get_CurrentGoapInfo() const -> const FCkGoapDebugger_GoapInfo*;

	auto
	Get_AllGoapEntities() const -> const TArray<FCkGoapDebugger_GoapInfo>& { return _DataCollector.Get_AllGoapEntities(); }

	auto
	Get_PlanHistory(FCk_Handle InHandle) const -> const TArray<FCkGoapDebugger_HistoryEntry>*;

	auto
	Get_SearchLog(FCk_Handle InHandle) const -> const TArray<FCkGoapDebugger_SearchSnapshot>*;

	// ----------------------------------------------------------------------------------------------------------------
	// PAUSE
	// ----------------------------------------------------------------------------------------------------------------

	auto
	Set_Paused(bool InPaused) -> void;

	auto
	Get_Paused() const -> bool { return _IsPaused; }

	// ----------------------------------------------------------------------------------------------------------------
	// VIEW MODE + SCRUB
	// ----------------------------------------------------------------------------------------------------------------
	//
	// Live  — Get_CurrentGoapInfo() returns the live snapshot from the data collector.
	// Scrub — Get_CurrentGoapInfo() returns the frozen snapshot from the history entry
	//         at _ScrubHistoryIndex, so the whole UI (graph, world state, details) reads
	//         from that historical moment.

	auto
	Set_ViewMode(ECk_GoapDebugger_ViewMode InMode) -> void;

	auto
	Get_ViewMode() const -> ECk_GoapDebugger_ViewMode { return _ViewMode; }

	auto
	Set_ScrubHistoryIndex(int32 InIndex) -> void;

	auto
	Get_ScrubHistoryIndex() const -> int32 { return _ScrubHistoryIndex; }

	// ----------------------------------------------------------------------------------------------------------------
	// DELEGATES
	// ----------------------------------------------------------------------------------------------------------------

	FOnGoapListChanged OnGoapListChanged;
	FOnGoapDataRefreshed OnGoapDataRefreshed;
	FOnGoapSelectedEntityChanged OnSelectedEntityChanged;
	FOnGoapPausedChanged OnPausedChanged;
	FOnGoapViewModeChanged OnViewModeChanged;

private:
	FCkGoapDebugger_DataCollector _DataCollector;
	FCk_Handle _SelectedEntityHandle;
	bool _IsPaused = false;
	int32 _LastEntityCount = -1;

	ECk_GoapDebugger_ViewMode _ViewMode = ECk_GoapDebugger_ViewMode::Live;
	int32 _ScrubHistoryIndex = -1;
};

// ====================================================================================================================
