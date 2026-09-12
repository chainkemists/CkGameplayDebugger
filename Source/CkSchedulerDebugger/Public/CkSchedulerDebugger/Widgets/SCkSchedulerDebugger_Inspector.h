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
	friend class FCkSchedulerDebugger_InspectorAuthored;

	/** The authored path owns all stable inspector presentation.  The older Slate builders below
	 * remain an atomic startup fallback only; the ViewModel continues to own selection/data. */
	auto DoBuildAuthoredView() -> void;
	auto DoPresentAuthored(const FCkSchedulerDebugger_ProcessorInfo* InProc) -> bool;
	auto DoClearAuthoredProjection() -> void;
	auto DoNavigateDependency(const FString& InRecordKey) -> void;
	auto DoBuildNativeScroll(const TSharedRef<SWidget>& InContent) -> TSharedRef<SWidget>;

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
	TSharedPtr<class FCkUiView> _AuthoredView;
	TSharedPtr<class FCkUiCollection> _AuthoredDependencies;
	TSharedPtr<class FCkUiCollection> _AuthoredDirtyDetails;
	TSharedPtr<class FCkUiCollection> _AuthoredConflicts;
	TSharedPtr<class FCkUiFloatSeries> _AuthoredTimingSeries;
	FString _AuthoredName;
	FString _AuthoredStatus;
	FString _AuthoredGroup;
	FString _AuthoredTickGroup;
	FString _AuthoredExecutionOrder;
	FString _AuthoredCurrentTiming;
	FString _AuthoredPeakTiming;
	FString _AuthoredTotalTicks;
	FString _AuthoredTickRate;
	FString _AuthoredDirtySummary;
	FString _AuthoredConflictSummary;
	FLinearColor _AuthoredStatusForeground = FLinearColor::White;
	FLinearColor _AuthoredStatusBackground = FLinearColor::Transparent;
	bool _AuthoredHasSelection = false;
	bool _AuthoredHasDirty = false;
	bool _AuthoredHasConflicts = false;
	bool _AuthoredHasDependencies = false;
	bool _AuthoredIsParallel = false;
	bool _AuthoredMounted = false;
	FString _AuthoredLoadError;

	FDelegateHandle _SelectionChangedHandle;
	FDelegateHandle _DataRefreshedHandle;
	int32 _LastSeenFrameOffset = 0;
};

// --------------------------------------------------------------------------------------------------------------------
