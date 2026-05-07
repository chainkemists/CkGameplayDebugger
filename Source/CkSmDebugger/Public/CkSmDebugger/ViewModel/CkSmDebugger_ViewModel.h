#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"
#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Delegates
// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnSmListChanged, const TArray<FCkSmDebugger_SmInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnSmDataRefreshed, const FCkSmDebugger_SmInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnSelectedNodeChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnViewModeChanged, ECkSmDebugger_ViewMode);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnScrubStateChanged, const FCkSmDebugger_ScrubState&);
DECLARE_MULTICAST_DELEGATE(FCkSmDebugger_OnRelayoutRequested);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkSmDebugger_OnCommandIssued, const FCkSmDebugger_Command&);

// --------------------------------------------------------------------------------------------------------------------
// ViewModel — shared state model for all SM debugger sub-widgets.
// Owned by the Page, passed to graph view, timeline, and history list.
// --------------------------------------------------------------------------------------------------------------------

class FCkSmDebugger_ViewModel
{
public:
    // -----------------------------------------------------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------------------------------------------------

    FCkSmDebugger_OnSmListChanged OnSmListChanged;
    FCkSmDebugger_OnSmDataRefreshed OnSmDataRefreshed;
    FCkSmDebugger_OnSelectedNodeChanged OnSelectedNodeChanged;
    FCkSmDebugger_OnViewModeChanged OnViewModeChanged;
    FCkSmDebugger_OnScrubStateChanged OnScrubStateChanged;
    FCkSmDebugger_OnRelayoutRequested OnRelayoutRequested;
    FCkSmDebugger_OnCommandIssued OnCommandIssued;

    // -----------------------------------------------------------------------------------------------------------------
    // Tick — collects data and broadcasts refresh
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Tick(
        UWorld* InWorld,
        float InDeltaTime) -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Lifecycle — clears all cached state so the debugger re-links cleanly on the next world
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Reset_ForWorldChange() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // SM selection
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedSmHandle(
        FCk_Handle_StateMachine InHandle) -> void;

    auto
    Get_SelectedSmHandle() const -> FCk_Handle_StateMachine;

    auto
    Has_SelectedSm() const -> bool;

    auto
    Get_AllStateMachines() const -> const TArray<FCkSmDebugger_SmInfo>&;

    auto
    Get_CurrentSmInfo() const -> const FCkSmDebugger_SmInfo*;

    auto
    Get_MutableSmInfo() -> FCkSmDebugger_SmInfo*;

    // -----------------------------------------------------------------------------------------------------------------
    // Node selection
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedNodeIndex(
        int32 InIndex) -> void;

    auto
    Get_SelectedNodeIndex() const -> int32;

    // -----------------------------------------------------------------------------------------------------------------
    // View mode & scrub
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_ViewMode(
        ECkSmDebugger_ViewMode InMode) -> void;

    auto
    Get_ViewMode() const -> ECkSmDebugger_ViewMode;

    auto
    Set_ScrubState(
        const FCkSmDebugger_ScrubState& InState) -> void;

    auto
    Get_ScrubState() const -> const FCkSmDebugger_ScrubState&;

    auto
    Get_ScrubSnapshot() const -> const FCkSmDebugger_ScrubSnapshot&;

    // -----------------------------------------------------------------------------------------------------------------
    // Scrub transition highlight — lets timeline drive graph highlight
    // -----------------------------------------------------------------------------------------------------------------

    auto
    SetScrubTransitionHighlight(
        int32 InSourceIndex,
        int32 InTargetIndex) -> void;

    auto
    ClearScrubTransitionHighlight() -> void;

    auto
    Get_ScrubHighlightSource() const -> int32;

    auto
    Get_ScrubHighlightTarget() const -> int32;

    // -----------------------------------------------------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------------------------------------------------

    auto
    RequestRelayout() -> void;

    auto
    Get_NeedsRelayout() const -> bool;

    auto
    ConsumeRelayout() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Commands — widgets issue commands, Page executes them on ECS
    // -----------------------------------------------------------------------------------------------------------------

    auto
    IssueCommand(
        const FCkSmDebugger_Command& InCommand) -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Expand toggle
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_ExpandAllNodes(
        bool InExpand) -> void;

    auto
    Get_ExpandAllNodes() const -> bool;

private:
    auto
    ComputeScrubSnapshot() -> void;

    auto
    FindRunForScrub() const -> const FCkSmDebugger_RunInfo*;

private:
    FCkSmDebugger_DataCollector _DataCollector;
    FCk_Handle_StateMachine _SelectedSmHandle;
    bool _HasSelectedSm = false;
    FCkSmDebugger_SmInfo _CurrentSmInfo;
    bool _HasSmInfo = false;

    int32 _SelectedNodeIndex = -1;

    ECkSmDebugger_ViewMode _ViewMode = ECkSmDebugger_ViewMode::Live;
    FCkSmDebugger_ScrubState _ScrubState;
    FCkSmDebugger_ScrubSnapshot _ScrubSnapshot;

    int32 _ScrubHighlightSource = -1;
    int32 _ScrubHighlightTarget = -1;

    bool _NeedsRelayout = true;
    bool _ExpandAllNodes = true;

    // Live-segment freeze: while in Scrub mode the live state segment should stop
    // growing so per-frame cells stay readable. Captured when entering Scrub, cleared
    // on Live. State name is stored alongside so the freeze releases automatically if
    // the SM transitions to a different state during the scrub session.
    bool _LiveSegmentFreezeActive = false;
    FString _LiveSegmentFreezeStateName;
    double _LiveSegmentFreezeEndTime = 0.0;
    uint64 _LiveSegmentFreezeEndFrame = 0;

public:
    auto Get_LiveSegmentFreezeActive() const -> bool { return _LiveSegmentFreezeActive; }
    auto Get_LiveSegmentFreezeStateName() const -> const FString& { return _LiveSegmentFreezeStateName; }
    auto Get_LiveSegmentFreezeEndTime() const -> double { return _LiveSegmentFreezeEndTime; }
    auto Get_LiveSegmentFreezeEndFrame() const -> uint64 { return _LiveSegmentFreezeEndFrame; }
};

// --------------------------------------------------------------------------------------------------------------------
