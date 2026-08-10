#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mission Control — center "Search trace" tab (nerd-gated by the window).
//
// Renders the last regressive-A* search of the selected Planner from
// PlannerInfo.SearchDebug (engine hook, P3): one row per explored constraint
// set — the conditions still unsatisfied at that state, the action that
// introduced it ("via"), and its unsatisfied-count heuristic. Rows whose
// constraint set is already satisfied by the world state highlight green —
// those are the states the search terminates on.
//
// A stats strip (iterations · state pool · elapsed µs · plan length · cost)
// summarizes SearchStats above the rows.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SVerticalBox;

class CKGOAPDEBUGGER_API SCkGoapDebugger_SearchTracePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_SearchTracePanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Called by the window when the ViewModel publishes a change.
    auto RefreshFromViewModel() -> void;
    /**
     * Drop the rebuild debounce so the next refresh re-emits structure. The window calls this on a
     * style-revision bump: colours / fonts / paddings are attribute-bound and already live, but a
     * panel's STRUCTURE (which rows and slots exist at all) is composed once against the axes, so a
     * structural axis change needs the re-emit. Flipping the hash rather than zeroing it keeps a
     * genuine zero hash from swallowing the invalidation.
     */
    auto Invalidate_StyleCache() -> void
    {
        _LastHash = ~_LastHash;
        RefreshFromViewModel();
    }


private:
    auto DoBuildRow(const FCk_Goap_SearchDebugRow& InRow, int32 InIndex) -> TSharedRef<SWidget>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    TSharedPtr<SVerticalBox> _Body;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
