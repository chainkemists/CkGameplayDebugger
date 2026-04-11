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
	Get_SelectedFrameOffset() const
	-> int32
{
	return _SelectedFrameOffset;
}

auto
	FCkSchedulerDebugger_ViewModel::
	Set_SelectedFrameOffset(
		int32 InOffset)
	-> void
{
	const auto MaxOffset = FMath::Max(0, _DataCollector.Get_FrameSnapshotCount() - 1);
	const auto ClampedOffset = FMath::Clamp(InOffset, 0, MaxOffset);

	if (ClampedOffset == _SelectedFrameOffset)
	{ return; }

	if (ClampedOffset > 0 && _SelectedFrameOffset == 0)
	{
		// Entering historical mode: remember freeze state, then auto-freeze
		_WasFrozenBeforeFrameScrub = _IsFrozen;
		_IsFrozen = true;
	}
	else if (ClampedOffset == 0 && _SelectedFrameOffset > 0)
	{
		// Returning to live: restore previous freeze state
		_IsFrozen = _WasFrozenBeforeFrameScrub;
		_DataCollector.RestoreLiveData();
	}

	_SelectedFrameOffset = ClampedOffset;

	if (_SelectedFrameOffset > 0)
	{
		_DataCollector.ApplyFrameSnapshot(_SelectedFrameOffset);
	}

	OnFrameSelectionChanged.Broadcast(_SelectedFrameOffset);
	OnDataRefreshed.Broadcast();
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

auto
	FCkSchedulerDebugger_ViewModel::
	Set_FrameHistoryMaxSize(
		UWorld* InWorld,
		int32 InMaxFrames)
	-> void
{
	_DataCollector.Set_FrameHistoryMaxSize(InWorld, InMaxFrames);
}

// --------------------------------------------------------------------------------------------------------------------
