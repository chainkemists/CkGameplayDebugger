#pragma once

#include "CkSmDebugger/Graph/CkSmRuntimeGraphModel.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkSmDebugger_ViewModel;
class SCkDebug_GraphCanvas;
struct FCkSmRuntimeGraphCardPresentation;

struct FCkSmRuntimeGraphLayout
{
    bool ExpandTasks = true;
    bool UndirectedBFS = false;
    int32 SpacingX = 350;
    int32 SpacingY = 120;
    int32 NameDepth = 1;
    int32 StateBreakpointStyle = 23;
    int32 TransitionBreakpointStyle = 5;

    friend auto operator==(const FCkSmRuntimeGraphLayout&, const FCkSmRuntimeGraphLayout&)
        -> bool = default;
};

DECLARE_DELEGATE_TwoParams(FOnCkSmRuntimeGraphSelection, int32, int32);

enum class ECkSmRuntimeBreakpointTarget : uint8
{
    StateEntry,
    StateExit,
    Transition,
};

DECLARE_DELEGATE_TwoParams(FOnCkSmRuntimeGraphBreakpointRequested,
                           ECkSmRuntimeBreakpointTarget,
                           int32);

// Shared Slate graph surface for editor and packaged SM debugger windows.
// Navigation/selection is canvas-owned; data and commands remain in the window/view-model.
class CKSMDEBUGGER_API SCkSmRuntimeGraph : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCkSmRuntimeGraph) {}
    SLATE_EVENT(FOnCkSmRuntimeGraphSelection, OnSelectionChanged)
    SLATE_EVENT(FOnCkSmRuntimeGraphBreakpointRequested, OnBreakpointRequested)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto SetSmInfo(const FCkSmDebugger_SmInfo* InInfo) -> void;
    auto SetExpandTasks(bool bInExpandTasks) -> void;
    auto SetNameDepth(int32 InNameDepth) -> void;
    auto SetLayout(const FCkSmRuntimeGraphLayout& InParams) -> void;
    // Matches the editor graph's configurable state breakpoint presets (0-23).
    // The independent transition preset (0-8) is carried by SetLayout.
    auto SetBreakpointStyle(int32 InBreakpointStyle) -> void;
    auto ApplyScrubHighlight(int32 InActiveStateIndex, int32 InExitedStateIndex) -> void;
    auto ClearPresentation() -> void;
    auto TriggerLivePresentation(const TArray<FCkSmDebugger_HistoryEntry>& InEvents) -> void;
    auto TriggerLivePresentation(int32 InPreviousStateIndex,
                                 int32 InCurrentStateIndex,
                                 const TSet<FString>& InPreviousStateNames) -> void;
    auto TickLivePresentation(float InDeltaTime) -> void;
    auto FrameAll() -> void;
    auto HasManualNodePositions() const -> bool { return NOT _PositionOverrides.IsEmpty(); }
    auto ResetNodePositions() -> void;
    auto Clear() -> void;

  private:
    // Position overrides are window-session presentation state. They deliberately never feed back
    // into the diagnostic DTO or value-model layout.
    auto RebuildScene(bool bInClearPositionOverrides = false) -> void;
    auto InstallScene() -> void;
    auto MakeCard(const TSharedRef<FCkSmRuntimeGraphCardPresentation>& InPresentation)
        -> TSharedRef<SWidget>;
    auto HandleSelectionChanged(const TSet<uint64>& InSelection) -> void;
    auto HandleNodeMoved(uint64 InNodeId, const FVector2D& InPosition) -> void;
    auto HandleNodeContextMenu(uint64 InNodeId, const FPointerEvent& InMouseEvent) -> void;
    auto ResolveDragGroup(uint64 InNodeId) const -> TSet<uint64>;
    auto HandleBreakpointClicked(ECkSmRuntimeBreakpointTarget InTarget, int32 InIndex) -> FReply;
    auto RequestBreakpointToggle(ECkSmRuntimeBreakpointTarget InTarget, int32 InIndex) -> void;
    auto GetEffectivePosition(const FCkSmRuntimeGraphNode& InNode) const -> FVector2D;
    auto GetEffectiveSize(const FCkSmRuntimeGraphNode& InNode) const -> FVector2D;
    auto GetEffectiveRoutePoints(const FCkSmRuntimeGraphEdge& InEdge) const -> TArray<FVector2D>;
    auto GetEffectiveTransitionBadgePosition(const FCkSmRuntimeGraphNode& InNode) const -> FVector2D;
    auto IsStateDescendantOf(int32 InStateIndex, int32 InCompoundOwnerStateIndex) const -> bool;
    auto GetCompoundDescendantIds(int32 InCompoundOwnerStateIndex) const -> TSet<uint64>;
    auto HasSelectedAncestorCompound(uint64 InNodeId) const -> bool;

    const FCkSmDebugger_SmInfo* _SmInfo = nullptr;
    FCkSmRuntimeGraphLayout _Layout;
    FCkSmRuntimeGraphModel _Model;
    TSharedPtr<SCkDebug_GraphCanvas> _Canvas;
    FOnCkSmRuntimeGraphSelection _OnSelectionChanged;
    FOnCkSmRuntimeGraphBreakpointRequested _OnBreakpointRequested;
    TMap<uint64, TSharedPtr<SWidget>> _CardCache;
    TMap<uint64, uint32> _CardStructureHashes;
    TMap<uint64, TSharedPtr<FCkSmRuntimeGraphCardPresentation>> _CardPresentations;
    TMap<uint64, TSharedPtr<class SCkDebug_NodePill>> _StatePills;
    TMap<uint64, FVector2D> _PositionOverrides;
    FCk_Handle_StateMachine _PositionScopeHandle;
    uint32 _StructureHash = 0;
    bool _HasStructureHash = false;
    int32 _BreakpointStyle = 23;
    int32 _TransitionBreakpointStyle = 5;
    bool _IsInstallingScene = false;
};
