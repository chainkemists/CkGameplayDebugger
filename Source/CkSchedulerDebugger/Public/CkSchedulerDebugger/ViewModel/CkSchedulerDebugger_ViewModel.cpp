#include "CkSchedulerDebugger_ViewModel.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_ViewModel::
	Tick(
		UWorld* InWorld,
		float InDeltaTime)
	-> void
{
	if (_IsFrozen)
	{ return; }

	_DataCollector.Collect(InWorld);
	OnDataRefreshed.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_ViewModel::
	Get_SelectedProcessorIndex() const
	-> int32
{
	return _SelectedProcessorIndex;
}

auto
	FCkSchedulerDebugger_ViewModel::
	Set_SelectedProcessorIndex(
		int32 InIndex)
	-> void
{
	if (_SelectedProcessorIndex == InIndex)
	{ return; }

	_SelectedProcessorIndex = InIndex;
	OnSelectionChanged.Broadcast(InIndex);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_ViewModel::
	Get_IsFrozen() const
	-> bool
{
	return _IsFrozen;
}

auto
	FCkSchedulerDebugger_ViewModel::
	Set_IsFrozen(
		bool InFrozen)
	-> void
{
	_IsFrozen = InFrozen;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_ViewModel::
	Get_DataCollector() const
	-> const FCkSchedulerDebugger_DataCollector&
{
	return _DataCollector;
}

// --------------------------------------------------------------------------------------------------------------------
