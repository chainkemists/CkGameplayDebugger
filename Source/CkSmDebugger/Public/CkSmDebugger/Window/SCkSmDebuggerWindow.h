#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

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

class SCkSmDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkSmDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkSmDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto TargetEntity(const FCk_Handle& InEntity) -> void;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    auto SupportsKeyboardFocus() const -> bool override { return true; }

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK SM Debugger")); }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildMenuActions() -> TSharedRef<SWidget>;
    auto BuildTimelineToolbar() -> TSharedRef<SWidget>;
    auto BuildDetailPanel() -> TSharedRef<SWidget>;
    auto BuildDetailContent() -> TSharedRef<SWidget>;
    auto RefreshDetailContent() -> void;
    auto RefreshSmSelector() -> void;

    // Move the scrub needle to the previous (-1) or next (+1) history transition.
    auto StepScrubToTransition(int32 InDirection) -> void;
    // Jump to a specific frame number (entered via the toolbar input field).
    auto JumpScrubToFrame(int64 InFrame) -> void;

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
    TOptional<FCk_Entity> _PendingTarget;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TWeakObjectPtr<UWorld> _CachedWorld;
    bool _IsTestMode = false;
    bool _IsPreviewOpen = false;

    // Right-side preview pane (50/50 split) — toggled via the PREVIEW toolbar button
    TSharedPtr<SCkSmDebugger_PreviewPane> _PreviewPane;
    TSharedPtr<SSplitter> _RootSplitter;

    // Common debugger-session boundary — unsubscribed in destructor.
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldChangedHandle;

    auto HandleWorldTornDown() -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;

    // Selection — transition selection tracked locally (not in ViewModel)
    int32 _SelectedTransitionIndex = -1;  // index into SmInfo.Transitions, -1 = none
    TSharedPtr<FCkSmDebugger_HistoryEntry> _SelectedHistoryEntry;

    // Breakpoint tracking — detect state transitions to trigger pause
    int32 _LastCurrentStateIdx = -1;

    // When true, the detail panel follows the current active state automatically.
    // Cleared when the user explicitly picks a different state/transition in the
    // graph, and restored when they click empty graph space.
    bool _AutoSelectActiveState = true;

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
        // Scrub-driven branch: when the panel falls through to the scrub-snapshot
        // case, the snapshot's HistoryIndex changes the structural content (different
        // active state, different next transition). Including it forces a rebuild as
        // the needle crosses state boundaries. ActiveStateIdx is also tracked so a
        // transient -1 (snapshot computed before States array stabilized) flipping to
        // a real index triggers a rebuild rather than leaving stale "No selection".
        int32 ScrubHistoryIdx = -1;
        int32 ScrubActiveStateIdx = -1;
        int32 ScrubMode = 0; // 0 = not scrubbing, 1 = scrubbing
        // True when the scrub needle is sitting on the selected history entry's time
        // (within epsilon). When this flips false during scrub, Case 1 yields to the
        // live scrub-snapshot (Case 4) and the panel must rebuild.
        int32 ScrubAlignsWithSelectedEntry = 1;

        auto operator==(const FDetailSignature& Other) const -> bool
        {
            return SmInfo == Other.SmInfo
                && HistoryEntry == Other.HistoryEntry
                && NodeIdx == Other.NodeIdx
                && TransitionIdx == Other.TransitionIdx
                && TaskCount == Other.TaskCount
                && TransitionCount == Other.TransitionCount
                && ScrubHistoryIdx == Other.ScrubHistoryIdx
                && ScrubActiveStateIdx == Other.ScrubActiveStateIdx
                && ScrubMode == Other.ScrubMode
                && ScrubAlignsWithSelectedEntry == Other.ScrubAlignsWithSelectedEntry;
        }
        auto operator!=(const FDetailSignature& Other) const -> bool { return !(*this == Other); }
    };
    FDetailSignature _LastDetailSig;
};

// --------------------------------------------------------------------------------------------------------------------
