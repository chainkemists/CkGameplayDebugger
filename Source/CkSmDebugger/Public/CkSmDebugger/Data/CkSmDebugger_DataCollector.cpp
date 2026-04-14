#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "HAL/PlatformTime.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"

#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"

#if CK_BUILD_SM_GRAPH_WALK
#include "CkStateMachine/Debug/CkStateMachine_Debug_GraphWalk_Fragment.h"
#endif
#include "CkStateMachine/StateMachine/CkStateMachine_Utils.h"
#include "CkStateMachine/State/EntityScripts/CkSmState_EntityScript.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _StateMachines.Reset();

    // Track PIE debug pause state for logical time computation
    {
        auto IsPausedNow = UCk_Utils_EditorOnly_UE::Get_IsDebugPauseExecution();

        if (IsPausedNow && NOT _WasPausedLastTick)
        {
            _PauseStartTime = FPlatformTime::Seconds();
        }
        else if (NOT IsPausedNow && _WasPausedLastTick)
        {
            _CompletedPauseIntervals.Add({_PauseStartTime, FPlatformTime::Seconds()});
        }

        _WasPausedLastTick = IsPausedNow;
        _IsPieDebugPaused = IsPausedNow;
    }

    if (NOT IsValid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (NOT ck::IsValid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_Sm_Current, ck::FFragment_Sm_Params>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_Sm_Current&, const ck::FFragment_Sm_Params&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            // Skip sub-SMs (their lifetime owner is a task entity)
            auto OwnerHandle = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Handle);

            if (ck::IsValid(OwnerHandle) && OwnerHandle.Has<ck::FFragment_SmTask_Current>())
            { return; }

            _StateMachines.Add(CollectStateMachine(Handle));
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    Get_AllStateMachines() const
    -> const TArray<FCkSmDebugger_SmInfo>&
{
    return _StateMachines;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    CollectStateMachineByHandle(
        FCk_Handle_StateMachine InSmHandle)
    -> FCkSmDebugger_SmInfo
{
    if (NOT ck::IsValid(InSmHandle))
    {
        return FCkSmDebugger_SmInfo{};
    }

    return CollectStateMachine(static_cast<FCk_Handle>(InSmHandle));
}

// --------------------------------------------------------------------------------------------------------------------

#if CK_BUILD_SM_GRAPH_WALK
auto
    FCkSmDebugger_DataCollector::
    BuildStructuralSubSmData(
        FCk_Handle_StateMachine InParentSmHandle,
        TSubclassOf<UCk_SmState_EntityScript> InParentStateClass,
        TSubclassOf<UCk_SmState_EntityScript> InSubSmInitialStateClass)
    -> FCkSmDebugger_SmInfo
{
    auto SmInfo = FCkSmDebugger_SmInfo{};

    if (NOT ck::IsValid(InParentSmHandle))
    { return SmInfo; }

    auto ParentHandle = static_cast<FCk_Handle>(InParentSmHandle);

    if (NOT ParentHandle.Has<ck::FFragment_Sm_Debug_GraphDefinition>())
    { return SmInfo; }

    const auto& GraphDef = ParentHandle.Get<ck::FFragment_Sm_Debug_GraphDefinition>();

    if (NOT GraphDef.Get_IsComplete())
    { return SmInfo; }

    auto* SubSmDef = GraphDef.Get_SubSmDefinitions().Find(InParentStateClass);

    if (NOT SubSmDef)
    { return SmInfo; }

    SmInfo.InitialStateClass = InSubSmInitialStateClass;
    SmInfo.RunStatus = ECk_SmRunStatus::Stopped;
    SmInfo.DebugName = TEXT("(structural)");

    auto StateClassToIndex = TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>{};

    for (const auto& [StateClass, StateDef] : SubSmDef->StateDefinitions)
    {
        auto StateInfo = FCkSmDebugger_StateInfo{};
        StateInfo.StateClass = StateClass;
        StateInfo.StateName = StateDef.StateName;

        for (const auto& TaskDef : StateDef.Tasks)
        {
            auto TaskInfo = FCkSmDebugger_TaskInfo{};
            TaskInfo.ClassName = TaskDef.ClassName;
            TaskInfo.Mode = TaskDef.Mode;
            TaskInfo.HasSubStateMachine = TaskDef.HasSubStateMachine;
            TaskInfo.SubSmInitialStateClass = TaskDef.SubSmInitialStateClass;
            StateInfo.Tasks.Add(MoveTemp(TaskInfo));
        }

        if (NOT StateDef.Tasks.IsEmpty())
        {
            for (const auto& TaskDef : StateDef.Tasks)
            {
                if (TaskDef.HasSubStateMachine)
                {
                    StateInfo.HasSubStateMachine = true;
                    break;
                }
            }
        }

        auto Index = SmInfo.States.Num();
        StateClassToIndex.Add(StateClass, Index);
        SmInfo.States.Add(MoveTemp(StateInfo));
    }

    for (const auto& [StateClass, StateDef] : SubSmDef->StateDefinitions)
    {
        auto* SourceIndex = StateClassToIndex.Find(StateClass);

        if (NOT SourceIndex)
        { continue; }

        auto TransitionIndex = 0;
        for (const auto& TransDef : StateDef.Transitions)
        {
            auto TransInfo = FCkSmDebugger_TransitionInfo{};
            TransInfo.SourceStateIndex = *SourceIndex;
            TransInfo.SourceStateClass = StateClass;
            TransInfo.SourceStateName = SmInfo.States[*SourceIndex].StateName;
            TransInfo.Order = TransitionIndex++;
            TransInfo.TargetStateClass = TransDef.TargetStateClass;

            if (IsValid(TransDef.TargetStateClass))
            {
                TransInfo.TargetStateName = UCk_Utils_Object_UE::Get_CleanClassName(TransDef.TargetStateClass);
            }

            auto* TargetIndex = StateClassToIndex.Find(TransDef.TargetStateClass);
            TransInfo.TargetStateIndex = TargetIndex ? *TargetIndex : -1;

            SmInfo.Transitions.Add(MoveTemp(TransInfo));
        }
    }

    return SmInfo;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    CollectStateMachine(
        const FCk_Handle& InSmHandle)
    -> FCkSmDebugger_SmInfo
{
    auto SmInfo = FCkSmDebugger_SmInfo{};
    SmInfo.Handle = ck::StaticCast<FCk_Handle_StateMachine>(InSmHandle);

    const auto& Current = InSmHandle.Get<ck::FFragment_Sm_Current>();
    SmInfo.RunStatus = Current.Get_RunStatus();
    SmInfo.CurrentStateClass = Current.Get_CurrentStateClass();
    SmInfo.IsTransitionQueued = InSmHandle.Has<ck::FTag_Sm_TransitionQueued>();

    if (InSmHandle.Has<ck::FFragment_Sm_Params>())
    {
        SmInfo.InitialStateClass = InSmHandle.Get<ck::FFragment_Sm_Params>().Get_InitialStateClass();
    }

    if (InSmHandle.Has<ck::FFragment_Sm_Context>())
    {
        SmInfo.GameEntity = InSmHandle.Get<ck::FFragment_Sm_Context>().Get_GameEntityHandle();
    }

    if (ck::IsValid(SmInfo.GameEntity))
    {
        SmInfo.DebugName = UCk_Utils_Handle_UE::Get_DebugName(SmInfo.GameEntity).ToString();
    }

    if (SmInfo.DebugName.IsEmpty())
    {
        SmInfo.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InSmHandle).ToString();
    }

    if (SmInfo.DebugName.IsEmpty())
    {
        SmInfo.DebugName = TEXT("(unnamed)");
    }

    // Build the graph from the debug fragment's cached data
    if (NOT InSmHandle.Has<ck::FFragment_Sm_Debug>())
    {
        return SmInfo;
    }

    const auto& Debug = InSmHandle.Get<ck::FFragment_Sm_Debug>();

    // Reset accumulated pause duration when a new SM run starts
    auto CurrentRunCounter = Debug.Get_RunCounter();

    if (CurrentRunCounter != _LastObservedRunCounter)
    {
        _CompletedPauseIntervals.Reset();
        _BreakpointHitWallTimes.Reset();
        _LastObservedRunCounter = CurrentRunCounter;
    }

    const auto& CachedStates = Debug.Get_CachedStates();

    auto StateClassToIndex = TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>{};

    // Build state nodes from cache
    for (const auto& [StateClass, CachedState] : CachedStates)
    {
        auto StateInfo = FCkSmDebugger_StateInfo{};
        StateInfo.StateClass = StateClass;
        StateInfo.StateName = CachedState.StateName;
        StateInfo.IsCurrentState = (StateClass == SmInfo.CurrentStateClass);

        // Copy cached tasks
        for (const auto& CachedTask : CachedState.Tasks)
        {
            auto TaskInfo = FCkSmDebugger_TaskInfo{};
            TaskInfo.ClassName = CachedTask.ClassName;
            TaskInfo.Mode = CachedTask.Mode;
            TaskInfo.HasSubStateMachine = CachedTask.HasSubStateMachine;
            TaskInfo.SubSmHandle = CachedTask.SubSmHandle;
            TaskInfo.SubSmInitialStateClass = CachedTask.SubSmInitialStateClass;

            if (CachedTask.HasSubStateMachine)
            {
                StateInfo.HasSubStateMachine = true;
            }

            StateInfo.Tasks.Add(MoveTemp(TaskInfo));
        }

        auto Index = SmInfo.States.Num();
        StateClassToIndex.Add(StateClass, Index);
        SmInfo.States.Add(MoveTemp(StateInfo));

        if (StateClass == SmInfo.CurrentStateClass)
        {
            SmInfo.CurrentStateIndex = Index;
        }
    }

    // Build transitions from cache
    for (const auto& [StateClass, CachedState] : CachedStates)
    {
        auto* SourceIndex = StateClassToIndex.Find(StateClass);

        if (NOT SourceIndex)
        { continue; }

        auto TransitionIndex = 0;
        for (const auto& CachedTransition : CachedState.Transitions)
        {
            auto TransInfo = FCkSmDebugger_TransitionInfo{};
            TransInfo.SourceStateIndex = *SourceIndex;
            TransInfo.SourceStateClass = StateClass;
            TransInfo.SourceStateName = SmInfo.States[*SourceIndex].StateName;
            TransInfo.Order = TransitionIndex++;
            TransInfo.TargetStateClass = CachedTransition.TargetStateClass;

            if (IsValid(CachedTransition.TargetStateClass))
            {
                TransInfo.TargetStateName = UCk_Utils_Object_UE::Get_CleanClassName(CachedTransition.TargetStateClass);
            }

            auto* TargetIndex = StateClassToIndex.Find(CachedTransition.TargetStateClass);
            TransInfo.TargetStateIndex = TargetIndex ? *TargetIndex : -1;

            // Build conditions from cache (no live satisfaction data yet)
            for (const auto& CachedCondition : CachedTransition.Conditions)
            {
                auto CondInfo = FCkSmDebugger_ConditionInfo{};
                CondInfo.ClassName = CachedCondition.ClassName;
                CondInfo.Mode = CachedCondition.Mode;
                CondInfo.Result = ECk_SmConditionResult::Undetermined;
                TransInfo.Conditions.Add(MoveTemp(CondInfo));
            }

            TransInfo.TotalCount = TransInfo.Conditions.Num();
            TransInfo.SatisfiedCount = 0;
            TransInfo.AreAllConditionsSatisfied = false;
            TransInfo.TransitionResult = ECk_SmTransitionResult::Undetermined;

            SmInfo.Transitions.Add(MoveTemp(TransInfo));
        }
    }

    // Recursively merge sub-SM states into the parent SmInfo
    auto SubSmHistories = TArray<FCkSmDebugger_HistoryEntry>{};
    MergeSubStateMachines(SmInfo, StateClassToIndex, SubSmHistories);

    // Overlay live condition satisfaction for the current state's transitions
    auto CurrentStateHandle = Current.Get_CurrentStateHandle();

    if (ck::IsValid(CurrentStateHandle) && IsValid(SmInfo.CurrentStateClass))
    {
        OverlayLiveData(
            CurrentStateHandle,
            SmInfo.CurrentStateIndex,
            StateClassToIndex,
            SmInfo);
    }

    // Copy history
    for (const auto& HistoryEntry : Debug.Get_History())
    {
        auto ViewerEntry = FCkSmDebugger_HistoryEntry{};
        ViewerEntry.FromStateName = HistoryEntry.FromStateName;
        ViewerEntry.ToStateName = HistoryEntry.ToStateName;
        ViewerEntry.SubSmParentStateName = HistoryEntry.SubSmParentStateName;
        ViewerEntry.FrameNumber = HistoryEntry.FrameNumber;
        ViewerEntry.ConditionNames = HistoryEntry.TransitionConditionNames;
        ViewerEntry.RealTimeSeconds = ComputeLogicalTime(HistoryEntry.RealTimeSeconds);

        for (const auto& Snap : HistoryEntry.TaskSnapshots)
        {
            auto ViewSnap = FCkSmDebugger_HistoryTaskSnapshot{};
            ViewSnap.TaskName = Snap.TaskName;
            ViewSnap.Result = Snap.Result;
            ViewerEntry.TaskSnapshots.Add(MoveTemp(ViewSnap));
        }

        SmInfo.History.Add(MoveTemp(ViewerEntry));
    }

    // Compute dwell times using logical time
    auto Now = ComputeLogicalTime(FPlatformTime::Seconds());

    // Current state: live dwell from debug fragment timestamp
    if (SmInfo.CurrentStateIndex >= 0)
    {
        auto EnteredAt = ComputeLogicalTime(Debug.Get_CurrentStateEnteredAtRealTime());

        if (EnteredAt > 0.0)
        {
            SmInfo.States[SmInfo.CurrentStateIndex].DwellTimeSeconds = Now - EnteredAt;
            SmInfo.States[SmInfo.CurrentStateIndex].IsCurrentDwellLive = true;
            SmInfo.States[SmInfo.CurrentStateIndex].HasBeenVisited = true;
        }
    }

    // Past states: walk history backwards to find last dwell per state
    auto VisitedStateClasses = TSet<TSubclassOf<UCk_SmState_EntityScript>>{};

    for (auto HistIdx = SmInfo.History.Num() - 1; HistIdx >= 1; --HistIdx)
    {
        const auto& Entry = Debug.Get_History()[HistIdx];
        auto FromClass = Entry.FromStateClass;

        if (VisitedStateClasses.Contains(FromClass))
        { continue; }

        VisitedStateClasses.Add(FromClass);

        auto* FromIndex = StateClassToIndex.Find(FromClass);

        if (NOT FromIndex)
        { continue; }

        auto& StateInfo = SmInfo.States[*FromIndex];

        if (StateInfo.IsCurrentDwellLive)
        { continue; }

        // Find when this state was entered (the previous history entry's timestamp)
        auto EnteredAt = 0.0;

        if (HistIdx > 0)
        {
            EnteredAt = ComputeLogicalTime(Debug.Get_History()[HistIdx - 1].RealTimeSeconds);
        }

        auto ExitedAt = ComputeLogicalTime(Entry.RealTimeSeconds);

        if (EnteredAt > 0.0 && ExitedAt > EnteredAt)
        {
            StateInfo.DwellTimeSeconds = ExitedAt - EnteredAt;
        }

        StateInfo.HasBeenVisited = true;
    }

    // First history entry's ToState was entered at SM start, mark as visited
    if (NOT Debug.Get_History().IsEmpty())
    {
        auto FirstToClass = Debug.Get_History()[0].ToStateClass;
        auto* FirstToIndex = StateClassToIndex.Find(FirstToClass);

        if (FirstToIndex && NOT SmInfo.States[*FirstToIndex].HasBeenVisited)
        {
            SmInfo.States[*FirstToIndex].HasBeenVisited = true;
        }
    }

    // Merge sub-SM history entries and sort chronologically
    if (NOT SubSmHistories.IsEmpty())
    {
        SmInfo.History.Append(MoveTemp(SubSmHistories));
        SmInfo.History.Sort([](const FCkSmDebugger_HistoryEntry& A, const FCkSmDebugger_HistoryEntry& B)
        {
            return A.RealTimeSeconds < B.RealTimeSeconds;
        });
    }

    // Build CurrentRun from live history
    {
        auto RunStartTime = 0.0;
        auto InitialStateName = FString{};

        if (NOT SmInfo.History.IsEmpty())
        {
            RunStartTime = SmInfo.History[0].RealTimeSeconds;
            InitialStateName = SmInfo.History[0].FromStateName;
        }
        else if (SmInfo.CurrentStateIndex >= 0)
        {
            RunStartTime = ComputeLogicalTime(Debug.Get_CurrentStateEnteredAtRealTime());
            InitialStateName = SmInfo.States[SmInfo.CurrentStateIndex].StateName;
        }

        SmInfo.CurrentRun.RunIndex = Debug.Get_RunCounter();
        SmInfo.CurrentRun.StartTime = RunStartTime;
        SmInfo.CurrentRun.EndTime = 0.0;
        SmInfo.CurrentRun.Duration = Now - RunStartTime;
        SmInfo.CurrentRun.History = SmInfo.History;
        SmInfo.CurrentRun.Segments = BuildTimelineSegments(SmInfo.History, RunStartTime, Now, InitialStateName);
        SmInfo.CurrentRun.BusyFrames = DetectBusyFrames(SmInfo.History, RunStartTime);
        SmInfo.CurrentRun.FrameSegments = BuildFrameSegments(SmInfo.History, RunStartTime, Now);

        // Populate pause markers from breakpoint hit wall times
        for (const auto& HitWallTime : _BreakpointHitWallTimes)
        {
            auto LogicalTime = ComputeLogicalTime(HitWallTime);
            auto RelativeTime = LogicalTime - RunStartTime;

            auto Marker = FCkSmDebugger_TimelinePauseMarker{};
            Marker.Time = RelativeTime;
            Marker.IsBreakpoint = true;
            SmInfo.CurrentRun.PauseMarkers.Add(MoveTemp(Marker));
        }

        // Populate pause markers from non-breakpoint pauses
        for (const auto& [PauseStart, PauseEnd] : _CompletedPauseIntervals)
        {
            auto LogicalTime = ComputeLogicalTime(PauseStart);
            auto RelativeTime = LogicalTime - RunStartTime;

            auto IsBreakpointPause = _BreakpointHitWallTimes.ContainsByPredicate(
                [&](double InHitTime) { return FMath::Abs(InHitTime - PauseStart) < 0.5; });

            if (NOT IsBreakpointPause)
            {
                auto Marker = FCkSmDebugger_TimelinePauseMarker{};
                Marker.Time = RelativeTime;
                Marker.IsBreakpoint = false;
                SmInfo.CurrentRun.PauseMarkers.Add(MoveTemp(Marker));
            }
        }
    }

    // Copy completed runs from debug fragment
#if !UE_BUILD_SHIPPING
    {
        for (const auto& BackendRun : Debug.Get_CompletedRuns())
        {
            auto ViewerRun = FCkSmDebugger_RunInfo{};
            ViewerRun.RunIndex = BackendRun.RunIndex;
            ViewerRun.StartTime = ComputeLogicalTime(BackendRun.StartRealTimeSeconds);
            ViewerRun.EndTime = ComputeLogicalTime(BackendRun.EndRealTimeSeconds);
            ViewerRun.Duration = ViewerRun.EndTime - ViewerRun.StartTime;

            for (const auto& HistEntry : BackendRun.History)
            {
                auto ViewerEntry = FCkSmDebugger_HistoryEntry{};
                ViewerEntry.FromStateName = HistEntry.FromStateName;
                ViewerEntry.ToStateName = HistEntry.ToStateName;
                ViewerEntry.FrameNumber = HistEntry.FrameNumber;
                ViewerEntry.ConditionNames = HistEntry.TransitionConditionNames;
                ViewerEntry.RealTimeSeconds = ComputeLogicalTime(HistEntry.RealTimeSeconds);

                for (const auto& Snap : HistEntry.TaskSnapshots)
                {
                    auto ViewSnap = FCkSmDebugger_HistoryTaskSnapshot{};
                    ViewSnap.TaskName = Snap.TaskName;
                    ViewSnap.Result = Snap.Result;
                    ViewerEntry.TaskSnapshots.Add(MoveTemp(ViewSnap));
                }

                ViewerRun.History.Add(MoveTemp(ViewerEntry));
            }

            auto RunInitialState = ViewerRun.History.IsEmpty() ? FString{} : ViewerRun.History[0].FromStateName;
            ViewerRun.Segments = BuildTimelineSegments(ViewerRun.History, ViewerRun.StartTime, ViewerRun.EndTime, RunInitialState);
            ViewerRun.BusyFrames = DetectBusyFrames(ViewerRun.History, ViewerRun.StartTime);
            ViewerRun.FrameSegments = BuildFrameSegments(ViewerRun.History, ViewerRun.StartTime, ViewerRun.EndTime);

            SmInfo.CompletedRuns.Add(MoveTemp(ViewerRun));
        }
    }

    // Copy breakpoint flags from ECS onto viewer state/transition structs
    if (InSmHandle.Has<ck::FFragment_Sm_Breakpoints>())
    {
        const auto& Breakpoints = InSmHandle.Get<ck::FFragment_Sm_Breakpoints>();

        for (auto& StateInfo : SmInfo.States)
        {
            StateInfo.HasEntryBreakpoint = Breakpoints.Get_EntryBreakpoints().Contains(StateInfo.StateClass);
            StateInfo.HasExitBreakpoint = Breakpoints.Get_ExitBreakpoints().Contains(StateInfo.StateClass);
        }

        for (auto& TransInfo : SmInfo.Transitions)
        {
            if (TransInfo.SourceStateIndex >= 0 && TransInfo.TargetStateClass)
            {
                auto SourceClass = SmInfo.States[TransInfo.SourceStateIndex].StateClass;
                auto Key = ck::FFragment_Sm_Breakpoints::FTransitionKey{SourceClass, TransInfo.TargetStateClass};
                TransInfo.HasBreakpoint = Breakpoints.Get_TransitionBreakpoints().Contains(Key);
            }
        }
    }

    // Read breakpoint hit info (transient fragment added at breakpoint site)
    if (InSmHandle.Has<ck::FFragment_Sm_Debug_BreakpointHit>())
    {
        if (_IsPieDebugPaused)
        {
            const auto& HitFrag = InSmHandle.Get<ck::FFragment_Sm_Debug_BreakpointHit>();
            SmInfo.HasBreakpointHit = true;
            SmInfo.BreakpointHitDescription = HitFrag.Description;

            if (NOT _BreakpointHitWallTimes.Contains(HitFrag.RealTimeSeconds))
            {
                _BreakpointHitWallTimes.Add(HitFrag.RealTimeSeconds);
            }

            if (SmInfo.CurrentStateIndex >= 0)
            {
                SmInfo.States[SmInfo.CurrentStateIndex].IsBreakpointHit = true;
            }
        }
        else
        {
            auto MutableHandle = static_cast<FCk_Handle>(InSmHandle);
            MutableHandle.Remove<ck::FFragment_Sm_Debug_BreakpointHit>();
        }
    }
#endif

    SmInfo.IsPieDebugPaused = _IsPieDebugPaused;

    return SmInfo;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    MergeSubStateMachines(
        FCkSmDebugger_SmInfo& InOutSmInfo,
        TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>& InOutStateClassToIndex,
        TArray<FCkSmDebugger_HistoryEntry>& OutSubSmHistories,
        int32 InDepth,
        int32 InScanFrom)
    -> void
{
    // Guard against infinite recursion
    if (InDepth > 8)
    { return; }

    // Scan states for sub-SMs starting from InScanFrom (avoids re-processing already-merged parents)
    auto OriginalStateCount = InOutSmInfo.States.Num();

    for (auto StateIdx = InScanFrom; StateIdx < OriginalStateCount; ++StateIdx)
    {
        auto& State = InOutSmInfo.States[StateIdx];

        if (NOT State.HasSubStateMachine)
        { continue; }

        for (auto& Task : State.Tasks)
        {
            if (NOT Task.HasSubStateMachine)
            { continue; }

            if (NOT ck::IsValid(static_cast<FCk_Handle>(Task.SubSmHandle)))
            { continue; }

            // Collect the sub-SM as an independent SmInfo
            auto SubSmInfo = CollectStateMachine(static_cast<FCk_Handle>(Task.SubSmHandle));

            if (SubSmInfo.States.Num() == 0)
            { continue; }

            // Merge sub-SM states into parent with index remapping
            auto IndexOffset = InOutSmInfo.States.Num();

            for (auto& SubState : SubSmInfo.States)
            {
                SubState.IsSubSmNode = true;
                SubState.SubSmParentStateIndex = StateIdx;
                SubState.SubSmParentStateName = State.StateName;

                // Remap the state class to avoid collisions (sub-SM states have unique classes)
                auto SubStateIndex = InOutSmInfo.States.Num();
                InOutStateClassToIndex.Add(SubState.StateClass, SubStateIndex);
                InOutSmInfo.States.Add(MoveTemp(SubState));
            }

            // Merge sub-SM transitions with remapped indices
            for (auto& SubTransition : SubSmInfo.Transitions)
            {
                SubTransition.SourceStateIndex += IndexOffset;
                SubTransition.TargetStateIndex += IndexOffset;
                SubTransition.IsSubSmTransition = true;
                InOutSmInfo.Transitions.Add(MoveTemp(SubTransition));
            }

            // Collect sub-SM history for later merge (after parent dwell-time computation)
            for (auto& SubEntry : SubSmInfo.History)
            {
                SubEntry.SubSmParentStateName = State.StateName;
                OutSubSmHistories.Add(MoveTemp(SubEntry));
            }
        }
    }

    // Recurse for nested sub-SMs — only scan newly added states to avoid re-merging
    if (InOutSmInfo.States.Num() > OriginalStateCount)
    {
        MergeSubStateMachines(InOutSmInfo, InOutStateClassToIndex, OutSubSmHistories, InDepth + 1, OriginalStateCount);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    OverlayLiveData(
        const FCk_Handle& InStateHandle,
        int32 InCurrentStateIndex,
        const TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>& InStateClassToIndex,
        FCkSmDebugger_SmInfo& InOutSmInfo)
    -> void
{
    auto StateChildren = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(InStateHandle);

    auto TaskIndex = 0;
    auto TransitionIndex = 0;

    for (const auto& ChildHandle : StateChildren)
    {
        // Overlay live condition data on transitions
        if (ChildHandle.Has<ck::FFragment_SmTransition_Params>())
        {
            const auto& TransParams = ChildHandle.Get<ck::FFragment_SmTransition_Params>();
            auto TargetClass = TransParams.Get_TargetStateClass();
            auto Order = TransitionIndex++;

            auto* MatchingTransition = static_cast<FCkSmDebugger_TransitionInfo*>(nullptr);

            for (auto& Trans : InOutSmInfo.Transitions)
            {
                if (Trans.SourceStateIndex == InCurrentStateIndex
                    && Trans.TargetStateClass == TargetClass
                    && Trans.Order == Order)
                {
                    MatchingTransition = &Trans;
                    break;
                }
            }

            if (NOT MatchingTransition)
            { continue; }

            MatchingTransition->Handle = ChildHandle;

            auto TransChildren = UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(ChildHandle);
            auto SatisfiedCount = 0;
            auto TotalCount = 0;
            auto ConditionIndex = 0;

            for (const auto& CondHandle : TransChildren)
            {
                if (NOT CondHandle.Has<ck::FFragment_SmCondition_Current>())
                { continue; }

                const auto& CondCurrent = CondHandle.Get<ck::FFragment_SmCondition_Current>();
                const auto CondResult = CondCurrent.Get_Result();

                ++TotalCount;

                if (CondResult == ECk_SmConditionResult::Pass)
                {
                    ++SatisfiedCount;
                }

                if (ConditionIndex < MatchingTransition->Conditions.Num())
                {
                    MatchingTransition->Conditions[ConditionIndex].Handle = CondHandle;
                    MatchingTransition->Conditions[ConditionIndex].Result = CondResult;
                }

                ++ConditionIndex;
            }

            MatchingTransition->TotalCount = TotalCount;
            MatchingTransition->SatisfiedCount = SatisfiedCount;
            MatchingTransition->AreAllConditionsSatisfied = (TotalCount > 0 && SatisfiedCount == TotalCount);

            // Read authoritative transition result from FFragment_SmTransition_Current
            if (ChildHandle.Has<ck::FFragment_SmTransition_Current>())
            {
                MatchingTransition->TransitionResult =
                    ChildHandle.Get<ck::FFragment_SmTransition_Current>().Get_Result();
            }
            else
            {
                MatchingTransition->TransitionResult = ECk_SmTransitionResult::Undetermined;
            }
        }

        // Overlay live task results
        if (ChildHandle.Has<ck::FFragment_SmTask_Current>())
        {
            auto& CurrentState = InOutSmInfo.States[InCurrentStateIndex];

            if (TaskIndex < CurrentState.Tasks.Num())
            {
                CurrentState.Tasks[TaskIndex].Handle = ChildHandle;
                CurrentState.Tasks[TaskIndex].LastResult = ChildHandle.Get<ck::FFragment_SmTask_Current>().Get_LastResult();
            }

            ++TaskIndex;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    BuildTimelineSegments(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime,
        double InNow,
        const FString& InInitialStateName)
    -> TArray<FCkSmDebugger_TimelineSegment>
{
    auto Segments = TArray<FCkSmDebugger_TimelineSegment>{};

    if (InHistory.IsEmpty())
    {
        if (NOT InInitialStateName.IsEmpty() && InNow > InRunStartTime)
        {
            auto Segment = FCkSmDebugger_TimelineSegment{};
            Segment.StateName = InInitialStateName;
            Segment.StartTime = 0.0;
            Segment.EndTime = 0.0;
            Segment.Color = CkSmDebugger::ComputeStateColor(InInitialStateName);
            Segments.Add(MoveTemp(Segment));
        }

        return Segments;
    }

    // First segment: initial state (FromState of first entry) from time 0 to first transition
    {
        auto Segment = FCkSmDebugger_TimelineSegment{};
        Segment.StateName = InHistory[0].FromStateName;
        Segment.StartTime = 0.0;
        Segment.EndTime = InHistory[0].RealTimeSeconds - InRunStartTime;
        Segment.Color = CkSmDebugger::ComputeStateColor(Segment.StateName);
        Segments.Add(MoveTemp(Segment));
    }

    // Middle segments: each history entry's ToState occupies from this entry's time to the next entry's time
    for (auto Index = 0; Index < InHistory.Num(); ++Index)
    {
        auto Segment = FCkSmDebugger_TimelineSegment{};
        Segment.StateName = InHistory[Index].ToStateName;
        Segment.StartTime = InHistory[Index].RealTimeSeconds - InRunStartTime;

        if (Index + 1 < InHistory.Num())
        {
            Segment.EndTime = InHistory[Index + 1].RealTimeSeconds - InRunStartTime;
        }
        else
        {
            // Last entry: open-ended (still active or run ended)
            Segment.EndTime = 0.0;
        }

        Segment.Color = CkSmDebugger::ComputeStateColor(Segment.StateName);
        Segments.Add(MoveTemp(Segment));
    }

    return Segments;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    DetectBusyFrames(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime)
    -> TArray<FCkSmDebugger_TimelineBusyFrame>
{
    auto BusyFrames = TArray<FCkSmDebugger_TimelineBusyFrame>{};

    if (InHistory.Num() < 2)
    { return BusyFrames; }

    auto CurrentFrame = InHistory[0].FrameNumber;
    auto Count = 1;
    auto FirstEntryTime = InHistory[0].RealTimeSeconds;

    for (auto Index = 1; Index < InHistory.Num(); ++Index)
    {
        if (InHistory[Index].FrameNumber == CurrentFrame)
        {
            ++Count;
        }
        else
        {
            if (Count >= 2)
            {
                auto BusyFrame = FCkSmDebugger_TimelineBusyFrame{};
                BusyFrame.Time = FirstEntryTime - InRunStartTime;
                BusyFrame.FrameNumber = CurrentFrame;
                BusyFrame.TransitionCount = Count;
                BusyFrames.Add(MoveTemp(BusyFrame));
            }

            CurrentFrame = InHistory[Index].FrameNumber;
            Count = 1;
            FirstEntryTime = InHistory[Index].RealTimeSeconds;
        }
    }

    // Check the last group
    if (Count >= 2)
    {
        auto BusyFrame = FCkSmDebugger_TimelineBusyFrame{};
        BusyFrame.Time = FirstEntryTime - InRunStartTime;
        BusyFrame.FrameNumber = CurrentFrame;
        BusyFrame.TransitionCount = Count;
        BusyFrames.Add(MoveTemp(BusyFrame));
    }

    return BusyFrames;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    BuildFrameSegments(
        const TArray<FCkSmDebugger_HistoryEntry>& InHistory,
        double InRunStartTime,
        double InNow)
    -> TArray<FCkSmDebugger_FrameSegment>
{
    auto Segments = TArray<FCkSmDebugger_FrameSegment>{};

    if (InHistory.IsEmpty())
    { return Segments; }

    for (auto Index = 0; Index < InHistory.Num(); ++Index)
    {
        auto Segment = FCkSmDebugger_FrameSegment{};
        Segment.StartTime = InHistory[Index].RealTimeSeconds - InRunStartTime;
        Segment.StartFrame = InHistory[Index].FrameNumber;

        if (Index + 1 < InHistory.Num())
        {
            Segment.EndTime = InHistory[Index + 1].RealTimeSeconds - InRunStartTime;
            Segment.EndFrame = InHistory[Index + 1].FrameNumber;
        }
        else
        {
            Segment.EndTime = InNow > InRunStartTime ? InNow - InRunStartTime : 0.0;
            Segment.EndFrame = Segment.StartFrame;
        }

        Segments.Add(MoveTemp(Segment));
    }

    return Segments;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    ComputeLogicalTime(
        double InWallClockTime) const
    -> double
{
    auto TotalPauseBefore = 0.0;

    for (const auto& [PauseStart, PauseEnd] : _CompletedPauseIntervals)
    {
        if (InWallClockTime > PauseStart)
        {
            TotalPauseBefore += FMath::Min(InWallClockTime, PauseEnd) - PauseStart;
        }
    }

    if (_WasPausedLastTick && InWallClockTime > _PauseStartTime)
    {
        TotalPauseBefore += FMath::Min(InWallClockTime, FPlatformTime::Seconds()) - _PauseStartTime;
    }

    return InWallClockTime - TotalPauseBefore;
}

// --------------------------------------------------------------------------------------------------------------------
