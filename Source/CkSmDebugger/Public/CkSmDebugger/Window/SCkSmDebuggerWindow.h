#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkSmDebugger_ViewModel;
class FCkSmDebugger_DataCollector;
class UCkSmDebugGraph;
class SCkSmDebugger_HistoryList;
class SCkSmDebugger_Timeline;
class SCkSmDebugger_PreviewPane;
class SGraphEditor;
class SSplitter;
class SBox;

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
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildDetailPanel() -> TSharedRef<SWidget>;
    auto BuildDetailContent() -> TSharedRef<SWidget>;
    auto RefreshDetailContent() -> void;
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

    TWeakObjectPtr<UWorld> _CachedWorld;
    bool _IsTestMode = false;
    bool _IsPreviewOpen = false;

    // Right-side preview pane (50/50 split) — toggled via the PREVIEW toolbar button
    TSharedPtr<SCkSmDebugger_PreviewPane> _PreviewPane;
    TSharedPtr<SSplitter> _RootSplitter;

    // Editor delegate handles — unsubscribed in destructor
    FDelegateHandle _OnEndPieHandle;
    FDelegateHandle _OnBeginPieHandle;

    auto HandleWorldTornDown() -> void;

    // Selection — transition selection tracked locally (not in ViewModel)
    int32 _SelectedTransitionIndex = -1;  // index into SmInfo.Transitions, -1 = none
    TSharedPtr<FCkSmDebugger_HistoryEntry> _SelectedHistoryEntry;

    // Breakpoint tracking — detect state transitions to trigger pause
    int32 _LastCurrentStateIdx = -1;

    // Detail panel — swappable content driven by selection changes.
    // Content is rebuilt only when the structural selection changes (new state,
    // transition, or history entry; or task/transition counts change).
    // Live values inside the content use Text_Lambda to update per frame.
    TSharedPtr<SBox> _DetailContentBox;

    struct FDetailSignature
    {
        const void* SmInfo = nullptr;
        const void* HistoryEntry = nullptr;
        int32 NodeIdx = -1;
        int32 TransitionIdx = -1;
        int32 TaskCount = 0;
        int32 TransitionCount = 0;

        auto operator==(const FDetailSignature& Other) const -> bool
        {
            return SmInfo == Other.SmInfo
                && HistoryEntry == Other.HistoryEntry
                && NodeIdx == Other.NodeIdx
                && TransitionIdx == Other.TransitionIdx
                && TaskCount == Other.TaskCount
                && TransitionCount == Other.TransitionCount;
        }
        auto operator!=(const FDetailSignature& Other) const -> bool { return !(*this == Other); }
    };
    FDetailSignature _LastDetailSig;
};

// --------------------------------------------------------------------------------------------------------------------
