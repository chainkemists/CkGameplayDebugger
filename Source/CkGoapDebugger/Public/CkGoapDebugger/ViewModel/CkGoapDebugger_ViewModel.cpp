#include "CkGoapDebugger_ViewModel.h"

// ====================================================================================================================

auto
	FCkGoapDebugger_ViewModel::
	Tick(UWorld* InWorld, float InDeltaTime)
	-> void
{
	if (_IsPaused)
	{
		return;
	}

	_DataCollector.Collect(InWorld);

	const auto& Entities = _DataCollector.Get_AllGoapEntities();

	// Broadcast list change if entity count changed
	if (Entities.Num() != _LastEntityCount)
	{
		_LastEntityCount = Entities.Num();
		OnGoapListChanged.Broadcast(Entities);

		// Auto-select first entity if nothing selected
		if (NOT ck::IsValid(_SelectedEntityHandle) && Entities.Num() > 0)
		{
			Set_SelectedEntityHandle(Entities[0].Handle);
		}
	}

	// Broadcast data refresh for selected entity
	const auto* CurrentInfo = Get_CurrentGoapInfo();
	OnGoapDataRefreshed.Broadcast(CurrentInfo);
}

// ====================================================================================================================

auto
	FCkGoapDebugger_ViewModel::
	Set_SelectedEntityHandle(FCk_Handle InHandle)
	-> void
{
	if (_SelectedEntityHandle == InHandle)
	{
		return;
	}

	_SelectedEntityHandle = InHandle;
	OnSelectedEntityChanged.Broadcast(InHandle);
}

// ====================================================================================================================

auto
	FCkGoapDebugger_ViewModel::
	Get_CurrentGoapInfo() const
	-> const FCkGoapDebugger_GoapInfo*
{
	for (const auto& Info : _DataCollector.Get_AllGoapEntities())
	{
		if (Info.Handle == _SelectedEntityHandle)
		{
			return &Info;
		}
	}

	return nullptr;
}

// ====================================================================================================================

auto
	FCkGoapDebugger_ViewModel::
	Get_PlanHistory(FCk_Handle InHandle) const
	-> const TArray<FCkGoapDebugger_HistoryEntry>*
{
	const auto EntityHash = GetTypeHash(InHandle);
	return _DataCollector.Get_PlanHistory().Find(EntityHash);
}

// ====================================================================================================================

auto
	FCkGoapDebugger_ViewModel::
	Set_Paused(bool InPaused)
	-> void
{
	if (_IsPaused == InPaused)
	{
		return;
	}

	_IsPaused = InPaused;
	OnPausedChanged.Broadcast(InPaused);
}

// ====================================================================================================================
