#pragma once

#include "CkSmDebugger/Graph/CkSmRuntimeGraphModel.h"

enum class ECkSmRuntimeHistoryStyle : uint8
{
    ArrowCards,
    ClassicArrows,
    CompactBlocks
};

// Runtime replacement for the window-facing subset of UCkSmDebugGraph. It owns
// only value state and never exposes UEdGraph/GraphEditor objects.
struct FCkSmRuntimeGraphLayoutParams
{
    bool UndirectedBFS = false;
    bool ExpandTasks = true;
    int32 SpacingX = 350;
    int32 SpacingY = 120;
    int32 CrossingReductionPasses = 4;
    int32 NameDepth = 1;
    float BadgeSpread = 20.0f;
    int32 StateBreakpointStyle = 23;
    int32 TransitionBreakpointStyle = 5;
    ECkSmRuntimeHistoryStyle HistoryStyle = ECkSmRuntimeHistoryStyle::ClassicArrows;
};

class CKSMDEBUGGER_API FCkSmRuntimeGraphFacade
{
  public:
    auto UpdateFromSmInfo(const FCkSmDebugger_SmInfo& InInfo) -> void;
    auto ForceRebuild() -> void
    {
        _NeedsRebuild = true;
        _Model.Clear();
    }
    auto ResetForWorldChange() -> void;
    auto RequestRelayout() -> void
    {
        _NeedsRebuild = true;
    }
    auto GetLayoutParams() const -> const FCkSmRuntimeGraphLayoutParams&
    {
        return _LayoutParams;
    }
    auto EditLayoutParams() -> FCkSmRuntimeGraphLayoutParams&
    {
        return _LayoutParams;
    }
    auto GetMaxNameDepth() const -> int32;
    auto GetScene() const -> const FCkSmRuntimeGraphScene&
    {
        return _Model.GetScene();
    }
    auto HasScene() const -> bool
    {
        return NOT _Model.GetScene().Nodes.IsEmpty();
    }
    auto SetSelectedState(int32 InStateIndex) -> void
    {
        _SelectedStateIndex = InStateIndex;
    }
    auto GetSelectedState() const -> int32
    {
        return _SelectedStateIndex;
    }
    auto SetScrubHighlight(int32 InActiveStateIndex, int32 InExitedStateIndex) -> void;
    auto ClearScrubHighlight() -> void;
    auto TickLivePresentation(float InDeltaTime,
                              int32 InPreviousStateIndex,
                              int32 InCurrentStateIndex,
                              const TSet<FString>& InPreviousStateNames) -> void;
    auto GetScrubActiveState() const -> int32
    {
        return _ScrubActiveStateIndex;
    }
    auto GetScrubExitedState() const -> int32
    {
        return _ScrubExitedStateIndex;
    }

  private:
    FCkSmRuntimeGraphLayoutParams _LayoutParams;
    FCkSmRuntimeGraphModel _Model;
    FCkSmDebugger_SmInfo _LastInfo;
    bool _HasInfo = false;
    bool _NeedsRebuild = true;
    int32 _SelectedStateIndex = INDEX_NONE;
    int32 _ScrubActiveStateIndex = INDEX_NONE;
    int32 _ScrubExitedStateIndex = INDEX_NONE;
};
