#pragma once

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_Inspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkSchedulerDebugger_Inspector) {}
		SLATE_ARGUMENT(TSharedPtr<FCkSchedulerDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	~SCkSchedulerDebugger_Inspector();

	/** Re-emit the inspector body after a Layer-B style revision. Selection is untouched. */
	auto Rebuild_ForStyleChange() -> void { DoRebuildContent(); }

private:
	auto DoOnSelectionChanged(int32 InProcessorIndex) -> void;
	auto DoOnDataRefreshed() -> void;
	auto DoRebuildContent() -> void;

	auto DoBuildEmptyContent() -> TSharedRef<SWidget>;
	auto DoBuildProcessorContent(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;
	auto DoBuildInfoSection(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;
	auto DoBuildTimingSection(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;
	auto DoBuildDirtySection(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;
	auto DoBuildWriteConflictSection(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;
	auto DoBuildDependenciesSection(const FCkSchedulerDebugger_ProcessorInfo& InProc) -> TSharedRef<SWidget>;

	auto DoMakeInfoRow(const FString& InLabel, const FString& InValue) -> TSharedRef<SWidget>;
	auto DoMakeDependencyButton(int32 InNodeIndex) -> TSharedRef<SWidget>;

	static auto DoGetTickGroupName(ETickingGroup InTickGroup) -> FString;

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TSharedPtr<SBox> _ContentBox;

	FDelegateHandle _SelectionChangedHandle;
	FDelegateHandle _DataRefreshedHandle;
	int32 _LastSeenFrameOffset = 0;
};

// --------------------------------------------------------------------------------------------------------------------
