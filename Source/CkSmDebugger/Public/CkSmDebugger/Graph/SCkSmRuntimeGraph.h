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

    friend auto operator==(const FCkSmRuntimeGraphLayout&, const FCkSmRuntimeGraphLayout&)
        -> bool = default;
};

DECLARE_DELEGATE_TwoParams(FOnCkSmRuntimeGraphSelection, int32, int32);

// Shared Slate graph surface for editor and packaged SM debugger windows.
// Navigation/selection is canvas-owned; data and commands remain in the window/view-model.
class CKSMDEBUGGER_API SCkSmRuntimeGraph : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCkSmRuntimeGraph) {}
    SLATE_EVENT(FOnCkSmRuntimeGraphSelection, OnSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto SetSmInfo(const FCkSmDebugger_SmInfo* InInfo) -> void;
    auto SetExpandTasks(bool bInExpandTasks) -> void;
    auto SetNameDepth(int32 InNameDepth) -> void;
    auto SetLayout(const FCkSmRuntimeGraphLayout& InParams) -> void;
    // Matches the editor graph's breakpoint presentation presets: 22 = inline squares,
    // 23 = inline diamonds. Other values retain the ordinary state-colour icon.
    auto SetBreakpointStyle(int32 InBreakpointStyle) -> void;
    auto ApplyScrubHighlight(int32 InActiveStateIndex, int32 InExitedStateIndex) -> void;
    auto ClearPresentation() -> void;
    auto TickLivePresentation(float InDeltaTime,
                              int32 InPreviousStateIndex,
                              int32 InCurrentStateIndex,
                              const TSet<FString>& InPreviousStateNames) -> void;
    auto FrameAll() -> void;
    auto Clear() -> void;

  private:
    auto RebuildScene() -> void;
    auto InstallScene() -> void;
    auto MakeCard(const TSharedRef<FCkSmRuntimeGraphCardPresentation>& InPresentation)
        -> TSharedRef<SWidget>;
    auto HandleSelectionChanged(const TSet<uint64>& InSelection) -> void;
    auto HandleNodeContextMenu(uint64 InNodeId, const FPointerEvent& InMouseEvent) -> void;

    const FCkSmDebugger_SmInfo* _SmInfo = nullptr;
    FCkSmRuntimeGraphLayout _Layout;
    FCkSmRuntimeGraphModel _Model;
    TSharedPtr<SCkDebug_GraphCanvas> _Canvas;
    FOnCkSmRuntimeGraphSelection _OnSelectionChanged;
    TMap<uint64, TSharedPtr<SWidget>> _CardCache;
    TMap<uint64, uint32> _CardStructureHashes;
    TMap<uint64, TSharedPtr<FCkSmRuntimeGraphCardPresentation>> _CardPresentations;
    TMap<uint64, TSharedPtr<class SCkDebug_NodePill>> _StatePills;
    uint32 _StructureHash = 0;
    bool _HasStructureHash = false;
    int32 _BreakpointStyle = 23;
    bool _IsInstallingScene = false;
};
