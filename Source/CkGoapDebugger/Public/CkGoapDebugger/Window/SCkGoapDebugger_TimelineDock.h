#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Mission Control — bottom timeline dock.
//
//   ┌─ Timeline · replans · world-state changes · activations ──────────────┐
//   │  WS      ■ ■   ■        ■×2                                           │
//   │  REPLAN     ◆      ◆        ◆(selected)                               │
//   │  ACTIVE            ▬▬▬▬▬▬▬▬▬▬▬▬                                       │
//   │  ── ticks ─────────────────────────────                               │
//   │  Jump to replan: [#1 · 2.1s] [#2 · 5.4s] …    Pause on: ☑replan ☐fail │
//   ├─ diff card (selected replan: trigger · old → new · Δ cost) ───────────┤
//   ├─ event log (time · kind chip · plain-language line; click-to-scrub) ──┤
//   └───────────────────────────────────────────────────────────────────────┘
//
// Data source: FCkGoapDebugger_DataCollector::GetHistory(selected entity).
// Selecting a diamond / log row / jump button scrubs the ViewModel
// (SetMode(Scrub) + SetScrubEventIndex); the LIVE transport in the chrome
// returns to live mode.
//
// "Pause on" halts PIE (the editor pause) when a matching event first
// appears while in Live mode — catch the exact frame.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SCkDebug_EventTimeline;
class SHorizontalBox;
class SVerticalBox;
class SBox;

class CKGOAPDEBUGGER_API SCkGoapDebugger_TimelineDock : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_TimelineDock) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
        SLATE_ATTRIBUTE(bool, PauseOnReplan)
        SLATE_ATTRIBUTE(bool, PauseOnPlanFailed)
        SLATE_EVENT(FSimpleDelegate, PauseExecution)
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


    // History indices die with the world.
    auto Reset_ForWorldChange() -> void;

private:
    auto DoRebuildTimeline(const TArray<FCkGoapDebugger_HistoryEvent>& InHistory) -> void;
    auto DoRebuildJumpButtons(const TArray<FCkGoapDebugger_HistoryEvent>& InHistory) -> void;
    auto DoRebuildDiffCard(const TArray<FCkGoapDebugger_HistoryEvent>& InHistory) -> void;
    auto DoRebuildEventLog(const TArray<FCkGoapDebugger_HistoryEvent>& InHistory) -> void;

    auto HandleScrubTo(int32 InEventIndex) -> void;

    // "Pause on" — fired from Refresh when a new matching event appears live.
    auto DoMaybePauseOnNewEvents(const TArray<FCkGoapDebugger_HistoryEvent>& InHistory) -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkDebug_EventTimeline> _Timeline;
    TSharedPtr<SHorizontalBox> _JumpButtons;
    TSharedPtr<SBox>           _DiffHost;
    TSharedPtr<SVerticalBox>   _EventLog;

    TAttribute<bool> _PauseOnReplan;
    TAttribute<bool> _PauseOnPlanFailed;
    FSimpleDelegate    _PauseExecution;

    // High-water mark for pause-on detection (events at/below are old).
    int32 _SeenEventCount = 0;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
