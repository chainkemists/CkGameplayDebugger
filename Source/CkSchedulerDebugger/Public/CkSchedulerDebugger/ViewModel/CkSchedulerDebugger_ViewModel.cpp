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
	const auto& Snapshots = _DataCollector.Get_FrameSnapshots();
	const auto MaxOffset = FMath::Max(0, Snapshots.Num() - 1);
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

	// Track the absolute frame number of the selected frame so the selection
	// sticks to that frame even as the history buffer scrolls (buffer is evicted
	// oldest-first, so an unchanged offset would otherwise point to a different frame).
	if (_SelectedFrameOffset > 0)
	{
		const auto SnapshotIdx = Snapshots.Num() - 1 - _SelectedFrameOffset;
		_SelectedFrameNumber = Snapshots.IsValidIndex(SnapshotIdx)
			? Snapshots[SnapshotIdx].FrameNumber
			: 0;

		_DataCollector.ApplyFrameSnapshot(_SelectedFrameOffset);
	}
	else
	{
		_SelectedFrameNumber = 0;
	}

	OnFrameSelectionChanged.Broadcast(_SelectedFrameOffset);
	OnDataRefreshed.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_ViewModel::
	Get_SelectedFrameNumber() const
	-> uint64
{
	return _SelectedFrameNumber;
}

auto
	FCkSchedulerDebugger_ViewModel::
	CycleSelectedFrame(
		int32 InDirection)
	-> void
{
	// Positive direction moves toward older frames (larger offset),
	// negative moves toward newer frames (smaller offset, toward LIVE).
	Set_SelectedFrameOffset(_SelectedFrameOffset + InDirection);
}

auto
	FCkSchedulerDebugger_ViewModel::
	GoToOldestFrame()
	-> void
{
	const auto MaxOffset = FMath::Max(0, _DataCollector.Get_FrameSnapshotCount() - 1);
	Set_SelectedFrameOffset(MaxOffset);
}

auto
	FCkSchedulerDebugger_ViewModel::
	GoToLiveFrame()
	-> void
{
	Set_SelectedFrameOffset(0);
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
