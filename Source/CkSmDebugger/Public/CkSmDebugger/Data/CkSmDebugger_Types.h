#pragma once

#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

#include "CkStateMachine/State/EntityScripts/CkSmState_EntityScript.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_ConditionInfo
{
    FCk_Handle Handle;
    FString ClassName;
    ECk_SmConditionResult Result = ECk_SmConditionResult::Undetermined;
    ECk_SmConditionMode Mode = ECk_SmConditionMode::Polled;
    ECk_SmConditionResetBehavior ResetBehavior = ECk_SmConditionResetBehavior::ResetEveryFrame;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TaskInfo
{
    FCk_Handle Handle;
    FString ClassName;
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
    TSubclassOf<UCk_SmState_EntityScript> StateClass;
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

    bool IsCompoundNode = false;
    float CompoundNodeWidth = 0.0f;
    float CompoundNodeHeight = 0.0f;
    int32 CompoundNodeParentStateIndex = -1;

    FVector2D NodePosition = FVector2D::ZeroVector;
    FVector2D NodeSize = FVector2D::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TransitionInfo
{
    FCk_Handle Handle;
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
    FString FromStateName;
    FString ToStateName;
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
    FLinearColor Color = FLinearColor::White;
    int32 HierarchyDepth = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_TimelineBusyFrame
{
    double Time = 0.0;
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
    int32 TakenTransitionSourceIdx = -1;
    int32 TakenTransitionTargetIdx = -1;
    int32 HistoryIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmDebugger_ScrubState
{
    ECkSmDebugger_ViewMode ViewMode = ECkSmDebugger_ViewMode::Live;
    int32 SelectedRunIndex = -1;
    double ScrubTime = 0.0;
    int32 SelectedHistoryIndex = -1;
    double TimelineViewDuration = 10.0;
    float TimelineScrollX = 0.0f;
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
    case ECk_SmTaskResult::Running:   return FLinearColor(1.0f, 0.757f, 0.028f);   // #FFC107 amber
    case ECk_SmTaskResult::Succeeded: return FLinearColor(0.263f, 0.627f, 0.278f);  // #43A047 green
    case ECk_SmTaskResult::Failed:    return FLinearColor(0.937f, 0.325f, 0.314f);  // #EF5350 red
    default:                          return FLinearColor(0.565f, 0.565f, 0.565f);  // grey
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

} // namespace CkSmDebugger

// --------------------------------------------------------------------------------------------------------------------
