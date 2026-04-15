#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

// ====================================================================================================================

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapListChanged, const TArray<FCkGoapDebugger_GoapInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapDataRefreshed, const FCkGoapDebugger_GoapInfo*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapSelectedEntityChanged, FCk_Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoapPausedChanged, bool);

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

	// ----------------------------------------------------------------------------------------------------------------
	// PAUSE
	// ----------------------------------------------------------------------------------------------------------------

	auto
	Set_Paused(bool InPaused) -> void;

	auto
	Get_Paused() const -> bool { return _IsPaused; }

	// ----------------------------------------------------------------------------------------------------------------
	// DELEGATES
	// ----------------------------------------------------------------------------------------------------------------

	FOnGoapListChanged OnGoapListChanged;
	FOnGoapDataRefreshed OnGoapDataRefreshed;
	FOnGoapSelectedEntityChanged OnSelectedEntityChanged;
	FOnGoapPausedChanged OnPausedChanged;

private:
	FCkGoapDebugger_DataCollector _DataCollector;
	FCk_Handle _SelectedEntityHandle;
	bool _IsPaused = false;
	int32 _LastEntityCount = -1;
};

// ====================================================================================================================
