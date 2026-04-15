#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_DataCollector.h"

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE(FCkSchedulerDebugger_OnDataRefreshed);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSchedulerDebugger_OnSelectionChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSchedulerDebugger_OnFrameSelectionChanged, int32);

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebugger_ViewModel
{
public:
	CK_GENERATED_BODY(FCkSchedulerDebugger_ViewModel);

public:
	auto Tick(UWorld* InWorld, float InDeltaTime) -> void;

	auto Get_SelectedProcessorIndex() const -> int32;
	auto Set_SelectedProcessorIndex(int32 InIndex) -> void;

	auto Get_IsFrozen() const -> bool;
	auto Set_IsFrozen(bool InFrozen) -> void;

	auto Get_SelectedFrameOffset() const -> int32;
	auto Set_SelectedFrameOffset(int32 InOffset) -> void;

	auto Get_SelectedFrameNumber() const -> uint64;
	auto CycleSelectedFrame(int32 InDirection) -> void;
	auto GoToOldestFrame() -> void;
	auto GoToLiveFrame() -> void;

	auto Get_DataCollector() const -> const FCkSchedulerDebugger_DataCollector&;

	auto Set_FrameHistoryMaxSize(UWorld* InWorld, int32 InMaxFrames) -> void;

public:
	FCkSchedulerDebugger_OnDataRefreshed OnDataRefreshed;
	FCkSchedulerDebugger_OnSelectionChanged OnSelectionChanged;
	FCkSchedulerDebugger_OnFrameSelectionChanged OnFrameSelectionChanged;

private:
	FCkSchedulerDebugger_DataCollector _DataCollector;
	int32 _SelectedProcessorIndex = INDEX_NONE;
	int32 _SelectedFrameOffset = 0;
	uint64 _SelectedFrameNumber = 0;
	bool _IsFrozen = false;
	bool _WasFrozenBeforeFrameScrub = false;
};

// --------------------------------------------------------------------------------------------------------------------
