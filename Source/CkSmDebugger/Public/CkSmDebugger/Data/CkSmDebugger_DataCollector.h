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

    // Persistent per-transition sample histories. Lives on the collector
    // (not on SmInfo) because _StateMachines is rebuilt every Collect tick;
    // restored into matching transitions at the start of each tick and
    // cleared on run-counter advance / PIE teardown.
    struct FTransitionKey
    {
        TSubclassOf<UCk_SmState_EntityScript> SourceStateClass;
        TSubclassOf<UCk_SmState_EntityScript> TargetStateClass;
        int32 Order = 0;

        auto operator==(const FTransitionKey& Other) const -> bool
        {
            return SourceStateClass == Other.SourceStateClass
                && TargetStateClass == Other.TargetStateClass
                && Order == Other.Order;
        }

        friend auto GetTypeHash(const FTransitionKey& InKey) -> uint32
        {
            return HashCombine(
                HashCombine(GetTypeHash(InKey.SourceStateClass), GetTypeHash(InKey.TargetStateClass)),
                ::GetTypeHash(InKey.Order));
        }
    };

    struct FTransitionSampleRing
    {
        TArray<TArray<FCkSmDebugger_ConditionResultSample>> ConditionResultHistory;
        TArray<FCkSmDebugger_TransitionResultSample>        ResultHistory;
    };

    // Keyed by SM debug name (unique enough in practice; collisions would
    // merge harmlessly into a single ring, but BB-side debug names are unique).
    TMap<FString, TMap<FTransitionKey, FTransitionSampleRing>> _TransitionHistoriesBySm;

    // Track each SM's last observed run counter to clear histories on a new run.
    TMap<FString, int32> _PerSmRunCounters;

    auto
    PersistTransitionHistories(
        const FString& InSmDebugName,
        const FCkSmDebugger_SmInfo& InSmInfo) -> void;

    auto
    RestoreTransitionHistories(
        const FString& InSmDebugName,
        FCkSmDebugger_SmInfo& InOutSmInfo) -> void;

private:
    TArray<FCkSmDebugger_SmInfo> _StateMachines;

    bool _IsPieDebugPaused = false;
    bool _WasPausedLastTick = false;
    double _PauseStartTime = 0.0;
    TArray<TPair<double, double>> _CompletedPauseIntervals;
    TArray<double> _BreakpointHitWallTimes;
    int32 _LastObservedRunCounter = -1;

    // Frame-space pause tracking — mirrors the time-space pause tracking above so
    // the live segment's EndFrame stays consistent with history's StartFrame even
    // across pause cycles. GFrameCounter advances every editor tick regardless of
    // PIE pause; subtracting paused-frame counts gives a logical frame counter that
    // matches "frames during which the SM actually ran".
    uint64 _GFrameAtPauseStart = 0;
    TArray<TPair<uint64, uint64>> _CompletedPauseFrameIntervals;

public:
    // Convert a raw GFrameCounter value into the logical (paused-frames-subtracted)
    // frame number the timeline operates in. Static-friendly: only reads internal
    // pause-interval state.
    auto ComputeLogicalFrame(uint64 InRawFrame) const -> uint64;
};

// --------------------------------------------------------------------------------------------------------------------
