#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkSmDebugger_ViewModel;
class FCkSmDebugger_DataCollector;
class UCkSmDebugGraph;
class SCkSmDebugger_HistoryList;
class SCkSmDebugger_Timeline;
class SGraphEditor;

// --------------------------------------------------------------------------------------------------------------------
// Top-level debugger window — placed inside the NomadTab.
// Composes graph editor, timeline, history list, detail panel, and toolbar.
// --------------------------------------------------------------------------------------------------------------------

class SCkSmDebuggerWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSmDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkSmDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildDetailPanel() -> TSharedRef<SWidget>;
    auto RefreshSmSelector() -> void;

    TSharedPtr<FCkSmDebugger_ViewModel> _ViewModel;
    TSharedPtr<FCkSmDebugger_DataCollector> _DataCollector;

    // Graph
    UCkSmDebugGraph* _Graph = nullptr;
    TSharedPtr<SGraphEditor> _GraphEditor;

    // Sub-widgets
    TSharedPtr<SCkSmDebugger_HistoryList> _HistoryList;
    TSharedPtr<SCkSmDebugger_Timeline> _Timeline;

    // SM selector
    TArray<TSharedPtr<FString>> _SmSelectorItems;
    TArray<FCk_Handle_StateMachine> _SmSelectorHandles;
    TSharedPtr<STextBlock> _SmSelectorLabel;

    UWorld* _CachedWorld = nullptr;
    bool _IsTestMode = false;

    // Selection — transition selection tracked locally (not in ViewModel)
    int32 _SelectedTransitionIndex = -1;  // index into SmInfo.Transitions, -1 = none
};

// --------------------------------------------------------------------------------------------------------------------
