#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_DataCollector.h"

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE(FCkSchedulerDebugger_OnDataRefreshed);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSchedulerDebugger_OnSelectionChanged, int32);

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

	auto Get_DataCollector() const -> const FCkSchedulerDebugger_DataCollector&;

public:
	FCkSchedulerDebugger_OnDataRefreshed OnDataRefreshed;
	FCkSchedulerDebugger_OnSelectionChanged OnSelectionChanged;

private:
	FCkSchedulerDebugger_DataCollector _DataCollector;
	int32 _SelectedProcessorIndex = INDEX_NONE;
	bool _IsFrozen = false;
};

// --------------------------------------------------------------------------------------------------------------------
