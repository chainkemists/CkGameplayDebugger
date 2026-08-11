#include "CkSmDebugger/Graph/CkSmRuntimeGraphFacade.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

auto FCkSmRuntimeGraphFacade::UpdateFromSmInfo(const FCkSmDebugger_SmInfo& InInfo) -> void
{
    _LastInfo = InInfo;
    _HasInfo = true;
    _Model.Rebuild(_LastInfo,
                   _LayoutParams.ExpandTasks,
                   _LayoutParams.NameDepth,
                   _LayoutParams.SpacingX,
                   _LayoutParams.SpacingY,
                   _LayoutParams.UndirectedBFS);
    _NeedsRebuild = false;
}

auto FCkSmRuntimeGraphFacade::ResetForWorldChange() -> void
{
    _Model.Clear();
    _LastInfo = FCkSmDebugger_SmInfo{};
    _HasInfo = false;
    _NeedsRebuild = true;
    _SelectedStateIndex = INDEX_NONE;
    ClearScrubHighlight();
}

auto FCkSmRuntimeGraphFacade::GetMaxNameDepth() const -> int32
{
    auto MaxDepth = 1;
    if (NOT _HasInfo)
    {
        return MaxDepth;
    }
    for (const auto& State : _LastInfo.States)
    {
        MaxDepth = FMath::Max(MaxDepth, SCkDebug_NameLabel::Get_SegmentCount(State.StateName));
    }
    return MaxDepth;
}

auto FCkSmRuntimeGraphFacade::SetScrubHighlight(const int32 InActiveStateIndex,
                                                const int32 InExitedStateIndex) -> void
{
    _ScrubActiveStateIndex = InActiveStateIndex;
    _ScrubExitedStateIndex = InExitedStateIndex;
    _Model.ApplyScrubHighlight(InActiveStateIndex, InExitedStateIndex);
}

auto FCkSmRuntimeGraphFacade::ClearScrubHighlight() -> void
{
    _ScrubActiveStateIndex = INDEX_NONE;
    _ScrubExitedStateIndex = INDEX_NONE;
    _Model.ClearPresentation();
}

auto FCkSmRuntimeGraphFacade::TickLivePresentation(const float InDeltaTime,
                                                   const int32 InPreviousStateIndex,
                                                   const int32 InCurrentStateIndex,
                                                   const TSet<FString>& InPreviousStateNames)
    -> void
{
    _Model.TickLivePresentation(InDeltaTime,
                                InPreviousStateIndex,
                                InCurrentStateIndex,
                                InPreviousStateNames);
}
