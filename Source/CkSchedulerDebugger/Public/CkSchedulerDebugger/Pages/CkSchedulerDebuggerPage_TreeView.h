#pragma once

#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_ProcessorTree;
class SCkSchedulerDebugger_Inspector;
class UCkSchedulerDebugGraph;
#if WITH_EDITOR
class SGraphEditor;
#endif

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebuggerPage_TreeView : public ICkSchedulerDebuggerPage
{
public:
	~FCkSchedulerDebuggerPage_TreeView() override;

	auto Get_PageName() const -> FText override;
	auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget> override;
	auto Tick(float InDeltaTime) -> void override;
	auto OnSelectionChanged(int32 InProcessorIndex) -> void override;
	auto OnStyleRevisionChanged() -> void override;

private:
	auto DoBuildDetailGraph() -> TSharedRef<SWidget>;
	auto DoRebuildDetailGraph() -> void;
	auto DoOnSelectionChanged(int32 InProcessorIndex) -> void;

private:
	TSharedPtr<SCkSchedulerDebugger_ProcessorTree> _ProcessorTree;
	TSharedPtr<SCkSchedulerDebugger_Inspector> _Inspector;
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	UCkSchedulerDebugGraph* _FullGraph = nullptr;

	UCkSchedulerDebugGraph* _DetailGraph = nullptr;
#if WITH_EDITOR
	TSharedPtr<SGraphEditor> _DetailGraphEditor;
#endif
	TSharedPtr<SBox> _DetailGraphContainer;
	int32 _LastDetailProcessorIndex = INDEX_NONE;

	FDelegateHandle _SelectionChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
