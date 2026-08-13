#pragma once

#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"

#include "CkStateMachine/Condition/EntityScripts/CkSmCondition_EntityScript.h"
#include "CkStateMachine/State/EntityScripts/CkSmState_EntityScript.h"
#include "CkStateMachine/Task/EntityScripts/CkSmTask_EntityScript.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_ConditionInfo
{
    FCk_Handle Handle;
    FString ClassName;
    TSubclassOf<UCk_SmCondition_EntityScript> ScriptClass;
    ECk_SmConditionResult Result = ECk_SmConditionResult::Undetermined;
    ECk_SmConditionMode Mode = ECk_SmConditionMode::Polled;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TaskInfo
{
    FCk_Handle Handle;
    FString ClassName;
    TSubclassOf<UCk_SmTask_EntityScript> ScriptClass;
    ECk_SmTaskMode Mode = ECk_SmTaskMode::EnterExitOnly;
    ECk_SmTaskResult LastResult = ECk_SmTaskResult::Running;

    bool HasSubStateMachine = false;
    FCk_Handle_StateMachine SubSmHandle;
    TSubclassOf<UCk_SmState_EntityScript> SubSmInitialStateClass;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_StateInfo
{
    FCk_Handle Handle;
    FCk_Handle_StateMachine OwningSmHandle;
    TSubclassOf<UCk_SmState_EntityScript> StateClass;
    TSubclassOf<UCk_SmState_EntityScript> ScriptClass;
    TSubclassOf<UCk_SmState_EntityScript> RequestedScriptClass;
    FString StateName;
    bool IsCurrentState = false;

    TArray<FCkSmDebugger_TaskInfo> Tasks;

    double DwellTimeSeconds = 0.0;
    bool HasBeenVisited = false;
    bool IsCurrentDwellLive = false;

    bool HasEntryBreakpoint = false;
    bool HasExitBreakpoint = false;
    bool IsBreakpointHit = false;

    bool IsSubSmNode = false;
    FString SubSmParentStateName;
    int32 SubSmParentStateIndex = -1;
    bool HasSubStateMachine = false;

    // True when this sub-SM state was re-merged from the persistent cache after
    // the live sub-SM entity was destroyed (parent state exited). Read-only:
    // OverlayLiveData will not touch these because they're not children of the
    // current live state. Used by the inspector to suppress live-only fields.
    bool IsHistoricalSubSm = false;

    bool IsCompoundNode = false;
    float CompoundNodeWidth = 0.0f;
    float CompoundNodeHeight = 0.0f;
    int32 CompoundNodeParentStateIndex = -1;

    FVector2D NodePosition = FVector2D::ZeroVector;
    FVector2D NodeSize = FVector2D::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------

// Per-frame snapshot samples for scrub-mode reconstruction. We record changes
// only (not every frame), so a polled condition that flips a few times per
// segment costs a few entries instead of one-per-tick. Reading at a scrub
// time is a binary search for the latest sample with Time <= scrubTime.
struct FCkSmDebugger_ConditionResultSample
{
    uint64 FrameNumber = 0;
    double Time = 0.0;
    ECk_SmConditionResult Result = ECk_SmConditionResult::Undetermined;
};

struct FCkSmDebugger_TransitionResultSample
{
    uint64 FrameNumber = 0;
    double Time = 0.0;
    int32  SatisfiedCount = 0;
    int32  TotalCount = 0;
};

// Cap to keep memory bounded for pathological flip-every-frame conditions.
// When exceeded, the oldest half is dropped — the recent past is what matters
// for scrub. Typical event-driven SMs stay well below this.
inline constexpr int32 GCkSmDebugger_MaxResultSamplesPerCondition = 4096;

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TransitionInfo
{
    FCk_Handle Handle;
    FCk_Handle_StateMachine OwningSmHandle;
    int32 SourceStateIndex = -1;
    int32 TargetStateIndex = -1;
    int32 Order = 0;
    TSubclassOf<UCk_SmState_EntityScript> SourceStateClass;
    FString SourceStateName;
    TSubclassOf<UCk_SmState_EntityScript> TargetStateClass;
    FString TargetStateName;

    TArray<FCkSmDebugger_ConditionInfo> Conditions;
    bool AreAllConditionsSatisfied = false;
    ECk_SmTransitionResult TransitionResult = ECk_SmTransitionResult::Undetermined;
    int32 SatisfiedCount = 0;
    int32 TotalCount = 0;

    // Per-frame history (delta-encoded). Outer index aligns with Conditions.
    // Populated by DataCollector on every Collect tick; cleared on run start
    // and PIE world teardown.
    TArray<TArray<FCkSmDebugger_ConditionResultSample>> ConditionResultHistory;
    TArray<FCkSmDebugger_TransitionResultSample>        ResultHistory;

    bool HasBreakpoint = false;

    bool IsSubSmTransition = false;
    bool IsSubSmConnector = false;

    TArray<FVector2D> RouteWaypoints;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_HistoryTaskSnapshot
{
    FString TaskName;
    ECk_SmTaskResult Result = ECk_SmTaskResult::Running;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_HistoryEntry
{
    // Collector-assigned, nonzero identity for one concrete backend history record. It remains
    // stable while the collector observes the same run, including after recursive/cached merges.
    uint64 LiveEventId = 0;
    // Exact backend timestamp bits used only to re-scope the same occurrence when a live child
    // history entry is promoted into its parent graph. Logical time is intentionally separate.
    uint64 LiveEventRawTimeBits = 0;
    FCk_Handle_StateMachine OwningSmHandle;
    TSubclassOf<UCk_SmState_EntityScript> FromStateClass;
    TSubclassOf<UCk_SmState_EntityScript> ToStateClass;
    FString FromStateName;
    FString ToStateName;
    FString SubSmParentStateName;
    uint64 FrameNumber = 0;
    int32 TransitionOrder = -1;
    TArray<FString> ConditionNames;
    TArray<FCkSmDebugger_HistoryTaskSnapshot> TaskSnapshots;
    double RealTimeSeconds = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TimelineSegment
{
    FString StateName;
    int32 StateIndex = -1;
    double StartTime = 0.0;
    double EndTime = 0.0;
    uint64 StartFrame = 0;
    uint64 EndFrame = 0;
    FLinearColor Color = FLinearColor::White;
    int32 HierarchyDepth = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TimelineBusyFrame
{
    double Time = 0.0;       // start: first transition at this engine frame
    double EndTime = 0.0;    // end: time of the next-frame transition (where the busy run finishes)
    uint64 FrameNumber = 0;
    int32 TransitionCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TimelinePauseMarker
{
    double Time = 0.0;
    bool IsBreakpoint = false;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_FrameSegment
{
    double StartTime = 0.0;
    double EndTime = 0.0;
    uint64 StartFrame = 0;
    uint64 EndFrame = 0;
};

// --------------------------------------------------------------------------------------------------------------------

// Snapshot of a transition's per-frame condition / count history at run end.
// Keyed by transition order so it survives renumbering across PIE sessions.
struct FCkSmDebugger_TransitionRunSnapshot
{
    TSubclassOf<UCk_SmState_EntityScript> SourceStateClass;
    TSubclassOf<UCk_SmState_EntityScript> TargetStateClass;
    int32 Order = 0;
    TArray<TArray<FCkSmDebugger_ConditionResultSample>> ConditionResultHistory;
    TArray<FCkSmDebugger_TransitionResultSample>        ResultHistory;
};

struct FCkSmDebugger_RunInfo
{
    int32 RunIndex = 0;
    double StartTime = 0.0;
    double EndTime = 0.0;
    double Duration = 0.0;
    TArray<FCkSmDebugger_HistoryEntry> History;
    TArray<FCkSmDebugger_TimelineSegment> Segments;
    TArray<FCkSmDebugger_TimelineBusyFrame> BusyFrames;
    TArray<FCkSmDebugger_TimelinePauseMarker> PauseMarkers;
    TArray<FCkSmDebugger_FrameSegment> FrameSegments;
    TArray<FCkSmDebugger_TransitionRunSnapshot> TransitionSnapshots;
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkSmDebugger_BreakpointHitKind : uint8
{
    None,
    StateEntry,
    StateExit,
    Transition,
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_SmInfo
{
    FCk_Handle_StateMachine Handle;
    FCk_Handle GameEntity;
    FString DebugName;
    ECk_SmRunStatus RunStatus = ECk_SmRunStatus::Stopped;
    TSubclassOf<UCk_SmState_EntityScript> CurrentStateClass;
    TSubclassOf<UCk_SmState_EntityScript> InitialStateClass;
    bool IsTransitionQueued = false;

    int32 CurrentStateIndex = -1;
    TArray<FCkSmDebugger_StateInfo> States;
    TArray<FCkSmDebugger_TransitionInfo> Transitions;
    TArray<FCkSmDebugger_HistoryEntry> History;

    FCkSmDebugger_RunInfo CurrentRun;
    TArray<FCkSmDebugger_RunInfo> CompletedRuns;

    bool IsPieDebugPaused = false;
    bool HasBreakpointHit = false;
    ECkSmDebugger_BreakpointHitKind BreakpointHitKind = ECkSmDebugger_BreakpointHitKind::None;
    TSubclassOf<UCk_SmState_EntityScript> BreakpointHitStateClass;
    TSubclassOf<UCk_SmState_EntityScript> BreakpointHitSourceStateClass;
    TSubclassOf<UCk_SmState_EntityScript> BreakpointHitTargetStateClass;
    FString BreakpointHitDescription;
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkSmDebugger_ViewMode : uint8
{
    Live,
    Scrub
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_ScrubSnapshot
{
    int32 ActiveStateIndex = -1;
    FString ActiveStateName;
    double TimeInState = 0.0;
    int32 HistoryIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------

// NOTE: the timeline's view window (start + duration) is NOT here. `SCkDebug_ScrubTimeline` owns
// pan/zoom itself and reports through OnViewChanged, so the old TimelineScrollX / TimelineViewDuration
// pair was deleted rather than rewired — two owners of one window is what made the old widget need a
// scrub anchor to stop the track sliding under the drag.
struct FCkSmDebugger_ScrubState
{
    ECkSmDebugger_ViewMode ViewMode = ECkSmDebugger_ViewMode::Live;
    int32 SelectedRunIndex = -1;
    double ScrubTime = 0.0;
    int32 SelectedHistoryIndex = -1;
    bool ShowFramesOnTimeline = true;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_Command
{
    enum class EType
    {
        None,
        ForceTransition,
        ResumeFromBreakpoint,
        ToggleStateEntryBreakpoint,
        ToggleStateExitBreakpoint,
        ToggleTransitionBreakpoint,
        PauseExecution
    };

    EType Type = EType::None;
    TSubclassOf<UCk_SmState_EntityScript> TargetStateClass;
    int32 StateIndex = -1;
    int32 TransitionIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------

namespace CkSmDebugger
{

// KEPT LOCAL (semantic canvas colour): per-state identity hue for the graph accent bar and the
// timeline segments. A palette categorical ramp wraps after 8 entries, which would make distinct
// states share a colour in exactly the visualization whose whole job is telling them apart.
inline auto
    ComputeStateColor(
        const FString& InStateName)
    -> FLinearColor
{
    auto Hash = GetTypeHash(InStateName);
    auto Hue = static_cast<float>(Hash % 360) / 360.0f;
    constexpr auto Saturation = 0.70f;
    constexpr auto Value = 0.85f;

    return FLinearColor::MakeFromHSV8(
        static_cast<uint8>(Hue * 255.0f),
        static_cast<uint8>(Saturation * 255.0f),
        static_cast<uint8>(Value * 255.0f));
}

inline auto
    GetTaskResultColor(
        ECk_SmTaskResult InResult)
    -> FLinearColor
{
    switch (InResult)
    {
    case ECk_SmTaskResult::Running:   return CkStyle::Warn();
    case ECk_SmTaskResult::Succeeded: return CkStyle::Ok();
    case ECk_SmTaskResult::Failed:    return CkStyle::Err();
    default:                          return CkStyle::TextDim();
    }
}

inline auto
    GetConditionResultLabel(
        ECk_SmConditionResult InResult)
    -> const TCHAR*
{
    switch (InResult)
    {
    case ECk_SmConditionResult::Pass:         return TEXT("[+]");
    case ECk_SmConditionResult::Fail:         return TEXT("[-]");
    case ECk_SmConditionResult::Undetermined: return TEXT("[?]");
    default:                                  return TEXT("[?]");
    }
}

inline auto
    GetTransitionResultLabel(
        ECk_SmTransitionResult InResult)
    -> const TCHAR*
{
    switch (InResult)
    {
    case ECk_SmTransitionResult::Pass:         return TEXT("PASS");
    case ECk_SmTransitionResult::Fail:         return TEXT("FAIL");
    case ECk_SmTransitionResult::Undetermined: return TEXT("...");
    default:                                   return TEXT("...");
    }
}

// Latest sample at-or-before the given time. Binary search; returns nullptr if
// the history is empty or every sample is in the future.
template <typename SampleT>
inline auto
    FindSampleAtTime(
        const TArray<SampleT>& InHistory,
        double InTime)
    -> const SampleT*
{
    if (InHistory.IsEmpty())
    { return nullptr; }

    auto Lo = 0;
    auto Hi = InHistory.Num() - 1;
    auto Result = -1;
    while (Lo <= Hi)
    {
        auto Mid = (Lo + Hi) / 2;
        if (InHistory[Mid].Time <= InTime)
        { Result = Mid; Lo = Mid + 1; }
        else
        { Hi = Mid - 1; }
    }
    return Result >= 0 ? &InHistory[Result] : nullptr;
}

inline auto
    PointToLineSegmentDistanceSq(
        FVector2D InPoint,
        FVector2D InLineA,
        FVector2D InLineB)
    -> float
{
    auto Ab = InLineB - InLineA;
    auto Ap = InPoint - InLineA;
    auto LenSq = Ab.SizeSquared();

    if (LenSq < 0.0001f)
    { return static_cast<float>(Ap.SizeSquared()); }

    auto T = FMath::Clamp(FVector2D::DotProduct(Ap, Ab) / LenSq, 0.0, 1.0);
    auto Closest = InLineA + T * Ab;
    return static_cast<float>((InPoint - Closest).SizeSquared());
}

inline auto
    FindTransitionSnapshot(
        const FCkSmDebugger_RunInfo& InRun,
        const TSubclassOf<UCk_SmState_EntityScript>& InSourceClass,
        const TSubclassOf<UCk_SmState_EntityScript>& InTargetClass,
        int32 InOrder)
    -> const FCkSmDebugger_TransitionRunSnapshot*
{
    for (const auto& Snap : InRun.TransitionSnapshots)
    {
        if (Snap.SourceStateClass == InSourceClass
            && Snap.TargetStateClass == InTargetClass
            && Snap.Order == InOrder)
        { return &Snap; }
    }
    return nullptr;
}

} // namespace CkSmDebugger

// --------------------------------------------------------------------------------------------------------------------
