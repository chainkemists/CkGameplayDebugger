#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

namespace ck { struct FFragment_Sm_Debug; }

// --------------------------------------------------------------------------------------------------------------------

class FCkSmDebugger_DataCollector
{
public:
    auto
    Collect(
        UWorld* InWorld) -> void;

    // Drop all cached SM data + pause bookkeeping — call when the PIE world ends
    // so the next session doesn't observe stale handles.
    auto
    Reset() -> void;

    auto
    Get_AllStateMachines() const -> const TArray<FCkSmDebugger_SmInfo>&;

    auto
    CollectStateMachineByHandle(
        FCk_Handle_StateMachine InSmHandle) -> FCkSmDebugger_SmInfo;

#if CK_BUILD_SM_GRAPH_WALK
    auto
    BuildStructuralSubSmData(
        FCk_Handle_StateMachine InParentSmHandle,
        TSubclassOf<UCk_SmState_EntityScript> InParentStateClass,
        TSubclassOf<UCk_SmState_EntityScript> InSubSmInitialStateClass) -> FCkSmDebugger_SmInfo;
#endif

private:
    auto
    CollectStateMachine(
        const FCk_Handle& InSmHandle,
        double InBirthRealTimeSeconds = 0.0) -> FCkSmDebugger_SmInfo;

    auto
    MergeSubStateMachines(
        FCkSmDebugger_SmInfo& InOutSmInfo,
        TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>& InOutStateClassToIndex,
        TArray<FCkSmDebugger_HistoryEntry>& OutSubSmHistories,
        const FCk_Handle& InParentHandle = FCk_Handle{},
        int32 InDepth = 0,
        int32 InScanFrom = 0) -> void;

    auto
    OverlayLiveData(
        const FCk_Handle& InStateHandle,
        int32 InCurrentStateIndex,
        const TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>& InStateClassToIndex,
        FCkSmDebugger_SmInfo& InOutSmInfo) -> void;

    static auto
    BuildTimelineSegments(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime,
        double InNow,
        const FString& InInitialStateName,
        uint64 InCurrentFrameNumber = 0) -> TArray<FCkSmDebugger_TimelineSegment>;

    static auto
    DetectBusyFrames(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime) -> TArray<FCkSmDebugger_TimelineBusyFrame>;

    static auto
    BuildFrameSegments(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime,
        double InNow,
        uint64 InCurrentFrameNumber = 0) -> TArray<FCkSmDebugger_FrameSegment>;

    auto
    ComputeLogicalTime(
        double InWallClockTime) const -> double;

private:
    TArray<FCkSmDebugger_SmInfo> _StateMachines;

    bool _IsPieDebugPaused = false;
    bool _WasPausedLastTick = false;
    double _PauseStartTime = 0.0;
    TArray<TPair<double, double>> _CompletedPauseIntervals;
    TArray<double> _BreakpointHitWallTimes;
    int32 _LastObservedRunCounter = -1;

    // Frame-counter snapshot at the start of the current pause. Used so the live
    // segment's EndFrame stays frozen while PIE is paused — GFrameCounter advances
    // every engine tick regardless of pause, which would otherwise make per-frame
    // cells shrink endlessly during pause.
    uint64 _GFrameAtPauseStart = 0;
};

// --------------------------------------------------------------------------------------------------------------------
