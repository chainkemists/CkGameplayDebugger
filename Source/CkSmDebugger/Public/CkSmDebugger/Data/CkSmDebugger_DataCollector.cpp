#include "CkSmDebugger/Data/CkSmDebugger_DataCollector.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "HAL/PlatformTime.h"

#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
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
        const auto IsPausedNow = UCk_Utils_EditorOnly_UE::Get_IsDebugPauseExecution();

        if (IsPausedNow && NOT _WasPausedLastTick)
        {
            _PauseStartTime = FPlatformTime::Seconds();
            _GFrameAtPauseStart = GFrameCounter;
        }
        else if (NOT IsPausedNow && _WasPausedLastTick)
        {
            _CompletedPauseIntervals.Add({_PauseStartTime, FPlatformTime::Seconds()});
            _CompletedPauseFrameIntervals.Add({_GFrameAtPauseStart, GFrameCounter});
        }

        _WasPausedLastTick = IsPausedNow;
        _IsPieDebugPaused = IsPausedNow;
    }

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (ck::Is_NOT_Valid(TransientEntity))
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
    Reset()
    -> void
{
    _StateMachines.Reset();
    _IsPieDebugPaused = false;
    _WasPausedLastTick = false;
    _PauseStartTime = 0.0;
    _GFrameAtPauseStart = 0;
    _CompletedPauseIntervals.Reset();
    _CompletedPauseFrameIntervals.Reset();
    _BreakpointHitWallTimes.Reset();
    _LastObservedRunCounter = -1;
    _TransitionHistoriesBySm.Reset();
    _PerSmRunCounters.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    CollectStateMachineByHandle(
        FCk_Handle_StateMachine InSmHandle)
    -> FCkSmDebugger_SmInfo
{
    if (ck::Is_NOT_Valid(InSmHandle))
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

    if (ck::Is_NOT_Valid(InParentSmHandle))
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
        StateInfo.ScriptClass = StateDef.ScriptClass;
        StateInfo.RequestedScriptClass = StateDef.RequestedScriptClass;
        StateInfo.StateName = StateDef.StateName;

        for (const auto& TaskDef : StateDef.Tasks)
        {
            auto TaskInfo = FCkSmDebugger_TaskInfo{};
            TaskInfo.ClassName = TaskDef.ClassName;
            TaskInfo.ScriptClass = TaskDef.ScriptClass;
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

            for (const auto& CondDef : TransDef.Conditions)
            {
                auto CondInfo = FCkSmDebugger_ConditionInfo{};
                CondInfo.ClassName = CondDef.ClassName;
                CondInfo.ScriptClass = CondDef.ScriptClass;
                CondInfo.Mode = CondDef.Mode;
                CondInfo.Result = ECk_SmConditionResult::Undetermined;
                TransInfo.Conditions.Add(MoveTemp(CondInfo));
            }

            TransInfo.TotalCount = TransInfo.Conditions.Num();

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
        const FCk_Handle& InSmHandle,
        double InBirthRealTimeSeconds)
    -> FCkSmDebugger_SmInfo
{
    auto SmInfo = FCkSmDebugger_SmInfo{};
    SmInfo.Handle = UCk_Utils_StateMachine_UE::CastChecked(InSmHandle);
    // FFragment_Sm_Context was removed — the SM fragments now live directly on
    // the game entity, so InSmHandle *is* the owning game entity. Surface it
    // explicitly so UI consumers (e.g. the SM Debugger's toolbar EntityRef)
    // don't have to know about that detail.
    SmInfo.GameEntity = InSmHandle;

    const auto& Current = InSmHandle.Get<ck::FFragment_Sm_Current>();
    SmInfo.RunStatus = Current.Get_RunStatus();
    SmInfo.CurrentStateClass = Current.Get_CurrentStateClass();
    SmInfo.IsTransitionQueued = InSmHandle.Has<ck::FTag_Sm_TransitionQueued>();

    if (InSmHandle.Has<ck::FFragment_Sm_Params>())
    {
        SmInfo.InitialStateClass = InSmHandle.Get<ck::FFragment_Sm_Params>().Get_InitialStateClass();
    }

    // FFragment_Sm_Context was removed � SM now lives on the game entity
    // directly rather than referencing one. Fall through to resolving the
    // debug name off the SM handle itself.
    SmInfo.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InSmHandle).ToString();

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

    // Per-SM run-counter tracking — clear this SM's transition histories at
    // run boundary so a fresh run doesn't show stale samples.
    if (auto* PerSmRunCounter = _PerSmRunCounters.Find(SmInfo.DebugName))
    {
        if (*PerSmRunCounter != CurrentRunCounter)
        {
            _TransitionHistoriesBySm.Remove(SmInfo.DebugName);
            *PerSmRunCounter = CurrentRunCounter;
        }
    }
    else
    {
        _PerSmRunCounters.Add(SmInfo.DebugName, CurrentRunCounter);
    }

    const auto& CachedStates = Debug.Get_CachedStates();

    auto StateClassToIndex = TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>{};

    // Build state nodes from cache
    for (const auto& [StateClass, CachedState] : CachedStates)
    {
        auto StateInfo = FCkSmDebugger_StateInfo{};
        StateInfo.StateClass = StateClass;
        StateInfo.ScriptClass = CachedState.ScriptClass;
        StateInfo.RequestedScriptClass = CachedState.RequestedScriptClass;
        StateInfo.StateName = CachedState.StateName;
        StateInfo.IsCurrentState = (StateClass == SmInfo.CurrentStateClass);

        // Copy cached tasks
        for (const auto& CachedTask : CachedState.Tasks)
        {
            auto TaskInfo = FCkSmDebugger_TaskInfo{};
            TaskInfo.ClassName = CachedTask.ClassName;
            TaskInfo.ScriptClass = CachedTask.ScriptClass;
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
                CondInfo.ScriptClass = CachedCondition.ScriptClass;
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

#if CK_BUILD_SM_GRAPH_WALK
    // Hydrate missing / placeholder entries from the graph-walk definition, if available.
    //
    // The runtime cache populates a state's ScriptClass / Tasks / Transitions lazily
    // (only when the state becomes current). Target states of outgoing transitions
    // get placeholder entries (StateClass + StateName only). The graph walker, gated
    // on CK_BUILD_SM_GRAPH_WALK, walks all reachable states up-front and records the
    // full static structure � including conditions and their modes. Using it here
    // means the debugger can show event-driven / override / tick indicators for
    // every state from the first frame, not only after they've been visited.
    if (InSmHandle.Has<ck::FFragment_Sm_Debug_GraphDefinition>())
    {
        const auto& GraphDef = InSmHandle.Get<ck::FFragment_Sm_Debug_GraphDefinition>();
        if (GraphDef.Get_IsComplete())
        {
            // Add states present in GraphDef but not yet in SmInfo.States.
            for (const auto& [StateClass, StateDef] : GraphDef.Get_StateDefinitions())
            {
                if (NOT ck::IsValid(StateClass)) { continue; }
                if (StateClassToIndex.Contains(StateClass)) { continue; }

                auto StateInfo = FCkSmDebugger_StateInfo{};
                StateInfo.StateClass = StateClass;
                StateInfo.ScriptClass = StateDef.ScriptClass;
                StateInfo.RequestedScriptClass = StateDef.RequestedScriptClass;
                StateInfo.StateName = StateDef.StateName;
                StateInfo.IsCurrentState = false;

                auto Index = SmInfo.States.Num();
                StateClassToIndex.Add(StateClass, Index);
                SmInfo.States.Add(MoveTemp(StateInfo));
            }

            // Hydrate placeholder states and synthesize transitions for states that
            // haven't been visited yet (no CachedState.Transitions populated).
            for (const auto& [StateClass, StateDef] : GraphDef.Get_StateDefinitions())
            {
                auto* SourceIndex = StateClassToIndex.Find(StateClass);
                if (NOT SourceIndex) { continue; }

                auto& State = SmInfo.States[*SourceIndex];

                if (NOT IsValid(State.ScriptClass))
                {
                    State.ScriptClass = StateDef.ScriptClass;
                    State.RequestedScriptClass = StateDef.RequestedScriptClass;
                }

                if (State.Tasks.IsEmpty() && NOT StateDef.Tasks.IsEmpty())
                {
                    for (const auto& TaskDef : StateDef.Tasks)
                    {
                        auto TaskInfo = FCkSmDebugger_TaskInfo{};
                        TaskInfo.ClassName = TaskDef.ClassName;
                        TaskInfo.Mode = TaskDef.Mode;
                        TaskInfo.HasSubStateMachine = TaskDef.HasSubStateMachine;
                        TaskInfo.SubSmInitialStateClass = TaskDef.SubSmInitialStateClass;
                        if (TaskDef.HasSubStateMachine) { State.HasSubStateMachine = true; }
                        State.Tasks.Add(MoveTemp(TaskInfo));
                    }
                }

                // Only synthesize transitions if this state has none in SmInfo yet �
                // otherwise the cache path already provided them (with live data).
                auto AlreadyHasTransitions = false;
                for (const auto& ExistingTrans : SmInfo.Transitions)
                {
                    if (ExistingTrans.SourceStateIndex == *SourceIndex)
                    { AlreadyHasTransitions = true; break; }
                }
                if (AlreadyHasTransitions) { continue; }

                auto TransitionIndex = 0;
                for (const auto& TransDef : StateDef.Transitions)
                {
                    auto TransInfo = FCkSmDebugger_TransitionInfo{};
                    TransInfo.SourceStateIndex = *SourceIndex;
                    TransInfo.SourceStateClass = StateClass;
                    TransInfo.SourceStateName = State.StateName;
                    TransInfo.Order = TransitionIndex++;
                    TransInfo.TargetStateClass = TransDef.TargetStateClass;

                    if (IsValid(TransDef.TargetStateClass))
                    {
                        TransInfo.TargetStateName = UCk_Utils_Object_UE::Get_CleanClassName(TransDef.TargetStateClass);
                    }

                    auto* TargetIdx = StateClassToIndex.Find(TransDef.TargetStateClass);
                    TransInfo.TargetStateIndex = TargetIdx ? *TargetIdx : -1;

                    for (const auto& CondDef : TransDef.Conditions)
                    {
                        auto CondInfo = FCkSmDebugger_ConditionInfo{};
                        CondInfo.ClassName = CondDef.ClassName;
                        CondInfo.ScriptClass = CondDef.ScriptClass;
                        CondInfo.Mode = CondDef.Mode;
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
        }
    }
#endif

    // Recursively merge sub-SM states into the parent SmInfo
    auto SubSmHistories = TArray<FCkSmDebugger_HistoryEntry>{};
    MergeSubStateMachines(SmInfo, StateClassToIndex, SubSmHistories, InSmHandle);

    // Overlay live condition satisfaction for the current state's transitions.
    // Per-frame sample histories are persisted on the collector across ticks
    // (since SmInfo is rebuilt each Collect): restore them onto the freshly-
    // built transitions, let OverlayLiveData append today's sample on change,
    // then persist back into the collector's store.
    auto CurrentStateHandle = Current.Get_CurrentStateHandle();

    if (ck::IsValid(CurrentStateHandle) && IsValid(SmInfo.CurrentStateClass))
    {
        RestoreTransitionHistories(SmInfo.DebugName, SmInfo);

        OverlayLiveData(
            CurrentStateHandle,
            SmInfo.CurrentStateIndex,
            StateClassToIndex,
            SmInfo);

        PersistTransitionHistories(SmInfo.DebugName, SmInfo);
    }
    else
    {
        // Even when there's no live state to overlay, restore persisted
        // histories so the inspector still sees them while scrubbing a
        // completed-but-not-restarted run.
        RestoreTransitionHistories(SmInfo.DebugName, SmInfo);
    }

    // Copy history
    for (const auto& HistoryEntry : Debug.Get_History())
    {
        auto ViewerEntry = FCkSmDebugger_HistoryEntry{};
        ViewerEntry.FromStateName = HistoryEntry.FromStateName;
        ViewerEntry.ToStateName = HistoryEntry.ToStateName;
        ViewerEntry.SubSmParentStateName = HistoryEntry.SubSmParentStateName;
        ViewerEntry.FrameNumber = ComputeLogicalFrame(HistoryEntry.FrameNumber);
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

    // Current state: live dwell since we most recently entered it.
    // Walk the backend history backwards to find the latest transition where
    // ToStateClass == CurrentStateClass � that's when this visit began. The
    // debug fragment's CurrentStateEnteredAtRealTime can accumulate across
    // repeated entries in some paths, so we prefer the history timestamp and
    // fall back to the fragment only when there is no matching entry yet.
    if (SmInfo.CurrentStateIndex >= 0)
    {
        auto EnteredAt = 0.0;

        const auto& BackendHistory = Debug.Get_History();
        for (auto HistIdx = BackendHistory.Num() - 1; HistIdx >= 0; --HistIdx)
        {
            if (BackendHistory[HistIdx].ToStateClass == SmInfo.CurrentStateClass)
            {
                EnteredAt = ComputeLogicalTime(BackendHistory[HistIdx].RealTimeSeconds);
                break;
            }
        }

        if (EnteredAt <= 0.0)
        { EnteredAt = ComputeLogicalTime(Debug.Get_CurrentStateEnteredAtRealTime()); }

        // For sub-SMs still on their initial state, the backend has no matching
        // history entry yet � fall back to the birth time supplied by the parent.
        if (EnteredAt <= 0.0 && InBirthRealTimeSeconds > 0.0)
        { EnteredAt = ComputeLogicalTime(InBirthRealTimeSeconds); }

        // Birth time is an authoritative lower bound for sub-SMs � when a parent
        // state re-enters and re-boots its sub-SM, Debug.Get_CurrentStateEnteredAtRealTime
        // can carry a stale value from a previous incarnation (same initial state class),
        // producing massively inflated dwells. Clamp to the most recent birth.
        if (InBirthRealTimeSeconds > 0.0)
        {
            auto BirthLogical = ComputeLogicalTime(InBirthRealTimeSeconds);
            if (EnteredAt < BirthLogical)
            { EnteredAt = BirthLogical; }
        }

        if (EnteredAt > 0.0)
        {
            SmInfo.States[SmInfo.CurrentStateIndex].DwellTimeSeconds = Now - EnteredAt;
            SmInfo.States[SmInfo.CurrentStateIndex].IsCurrentDwellLive = true;
            SmInfo.States[SmInfo.CurrentStateIndex].HasBeenVisited = true;
        }
    }

    // Past states: walk history backwards to find each state's most recent dwell.
    // For each first-seen FromStateClass (== most recent exit of that state),
    // locate the matching entry timestamp by searching backwards for the last
    // prior entry whose ToStateClass matches. Searching (instead of blindly
    // reading HistIdx-1) is needed because the history chain can have gaps
    // when sub-SMs stop/start � the neighbouring entry's ToStateClass is not
    // guaranteed to match.
    auto VisitedStateClasses = TSet<TSubclassOf<UCk_SmState_EntityScript>>{};

    const auto& BackendHistoryForPast = Debug.Get_History();
    for (auto HistIdx = BackendHistoryForPast.Num() - 1; HistIdx >= 0; --HistIdx)
    {
        const auto& Entry = BackendHistoryForPast[HistIdx];
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

        // Sub-SM states already have their dwell computed from the sub-SM's own fresh
        // backend history (see the recursive CollectStateMachine call in MergeSubStateMachines).
        // The outer SM's history accumulates *persisted* sub-SM transitions across all
        // parent re-entries, so processing sub-SM classes here would overwrite the fresh
        // dwell with a stale timestamp from a previous incarnation.
        if (StateInfo.IsSubSmNode)
        { continue; }

        // Find the most recent prior entry where this state was entered.
        auto EnteredAt = 0.0;
        for (auto PrevIdx = HistIdx - 1; PrevIdx >= 0; --PrevIdx)
        {
            if (BackendHistoryForPast[PrevIdx].ToStateClass == FromClass)
            {
                EnteredAt = ComputeLogicalTime(BackendHistoryForPast[PrevIdx].RealTimeSeconds);
                break;
            }
        }

        // Sub-SM initial states never appear as ToStateClass in their own
        // history � their enter time is the birth time supplied by the parent.
        if (EnteredAt <= 0.0 && InBirthRealTimeSeconds > 0.0)
        { EnteredAt = ComputeLogicalTime(InBirthRealTimeSeconds); }

        auto ExitedAt = ComputeLogicalTime(Entry.RealTimeSeconds);

        // Sub-SM re-entry: discard history rows whose exit timestamp predates the
        // current incarnation's birth � those belong to a previous sub-SM run and
        // would otherwise mix old timing into the freshly-rebooted sub-SM.
        if (InBirthRealTimeSeconds > 0.0)
        {
            auto BirthLogical = ComputeLogicalTime(InBirthRealTimeSeconds);
            if (ExitedAt < BirthLogical)
            { continue; }
            if (EnteredAt < BirthLogical)
            { EnteredAt = BirthLogical; }
        }

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

        // Snapshot per-transition sample histories into CurrentRun so the
        // inspector can read them when scrubbing this run. CompletedRuns are
        // not yet populated with TransitionSnapshots — that requires a hook
        // into the run-end transition (TODO).
        for (const auto& Trans : SmInfo.Transitions)
        {
            if (Trans.ConditionResultHistory.IsEmpty() && Trans.ResultHistory.IsEmpty())
            { continue; }

            auto Snap = FCkSmDebugger_TransitionRunSnapshot{};
            Snap.SourceStateClass = Trans.SourceStateClass;
            Snap.TargetStateClass = Trans.TargetStateClass;
            Snap.Order = Trans.Order;
            Snap.ConditionResultHistory = Trans.ConditionResultHistory;
            Snap.ResultHistory = Trans.ResultHistory;
            SmInfo.CurrentRun.TransitionSnapshots.Add(MoveTemp(Snap));
        }
        // Logical frame: GFrameCounter with paused engine frames subtracted, so the
        // live segment's EndFrame stays consistent with history's StartFrame across
        // pause cycles. While paused, ComputeLogicalFrame returns the value at the
        // moment pause began (the active-pause portion is included in the subtraction).
        const uint64 EffectiveFrame = ComputeLogicalFrame(GFrameCounter);
        SmInfo.CurrentRun.Segments = BuildTimelineSegments(SmInfo.History, RunStartTime, Now, InitialStateName, EffectiveFrame);
        SmInfo.CurrentRun.BusyFrames = DetectBusyFrames(SmInfo.History, RunStartTime);
        SmInfo.CurrentRun.FrameSegments = BuildFrameSegments(SmInfo.History, RunStartTime, Now, EffectiveFrame);

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
                ViewerEntry.FrameNumber = ComputeLogicalFrame(HistEntry.FrameNumber);
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
        const FCk_Handle& InParentHandle,
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

            if (ck::Is_NOT_Valid(static_cast<FCk_Handle>(Task.SubSmHandle)))
            { continue; }

            // The parent records a "(start)" marker in its own history when a
            // sub-SM boots � find the most recent one so we can hand the birth
            // time down. The sub-SM's own history never contains this marker,
            // so its initial state's first dwell can only be reconstructed via
            // the parent. Walk backwards so repeated start/stop cycles pick up
            // the most recent birth.
            auto SubSmBirthRealTime = 0.0;
            if (ck::IsValid(InParentHandle) && InParentHandle.Has<ck::FFragment_Sm_Debug>())
            {
                const auto& ParentDebug = InParentHandle.Get<ck::FFragment_Sm_Debug>();
                const auto& ParentHistory = ParentDebug.Get_History();
                for (auto Idx = ParentHistory.Num() - 1; Idx >= 0; --Idx)
                {
                    const auto& ParentEntry = ParentHistory[Idx];
                    if (ck::Is_NOT_Valid(ParentEntry.FromStateClass)
                        && ParentEntry.ToStateClass == Task.SubSmInitialStateClass)
                    {
                        SubSmBirthRealTime = ParentEntry.RealTimeSeconds;
                        break;
                    }
                }
            }

            // Collect the sub-SM as an independent SmInfo
            auto SubSmInfo = CollectStateMachine(static_cast<FCk_Handle>(Task.SubSmHandle), SubSmBirthRealTime);

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

    // Recurse for nested sub-SMs � only scan newly added states to avoid re-merging
    if (InOutSmInfo.States.Num() > OriginalStateCount)
    {
        // Nested sub-SMs within newly-added states: we don't have their owning
        // handle at this depth, so birth-time lookup is skipped (the sub-SM's
        // own CollectStateMachine call above already handles its direct
        // sub-SMs with the correct parent handle).
        MergeSubStateMachines(InOutSmInfo, InOutStateClassToIndex, OutSubSmHistories, FCk_Handle{}, InDepth + 1, OriginalStateCount);
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

            // Per-frame snapshot recording — append on change only. Read at scrub
            // time via binary search over Time. Bounded by GCkSmDebugger_MaxResultSamplesPerCondition.
            const auto SampleFrame = ComputeLogicalFrame(GFrameCounter);
            const auto SampleTime  = ComputeLogicalTime(FPlatformTime::Seconds());

            auto AppendCappedCondition = [](TArray<FCkSmDebugger_ConditionResultSample>& InOutHistory,
                                            FCkSmDebugger_ConditionResultSample InSample) -> void
            {
                InOutHistory.Add(MoveTemp(InSample));
                if (InOutHistory.Num() > GCkSmDebugger_MaxResultSamplesPerCondition)
                {
                    const auto DropCount = GCkSmDebugger_MaxResultSamplesPerCondition / 2;
                    InOutHistory.RemoveAt(0, DropCount, EAllowShrinking::No);
                }
            };

            // Sync per-condition history array length with current Conditions count
            if (MatchingTransition->ConditionResultHistory.Num() != MatchingTransition->Conditions.Num())
            { MatchingTransition->ConditionResultHistory.SetNum(MatchingTransition->Conditions.Num()); }

            for (auto CondIdx = 0; CondIdx < MatchingTransition->Conditions.Num(); ++CondIdx)
            {
                auto& CondHistory = MatchingTransition->ConditionResultHistory[CondIdx];
                const auto NewResult = MatchingTransition->Conditions[CondIdx].Result;
                const auto Changed = CondHistory.IsEmpty() || CondHistory.Last().Result != NewResult;
                if (Changed)
                {
                    AppendCappedCondition(CondHistory,
                        FCkSmDebugger_ConditionResultSample{SampleFrame, SampleTime, NewResult});
                }
            }

            auto& ResultHistory = MatchingTransition->ResultHistory;
            const auto CountChanged = ResultHistory.IsEmpty()
                || ResultHistory.Last().SatisfiedCount != SatisfiedCount
                || ResultHistory.Last().TotalCount     != TotalCount;
            if (CountChanged)
            {
                ResultHistory.Add(FCkSmDebugger_TransitionResultSample{
                    SampleFrame, SampleTime, SatisfiedCount, TotalCount});
                if (ResultHistory.Num() > GCkSmDebugger_MaxResultSamplesPerCondition)
                {
                    const auto DropCount = GCkSmDebugger_MaxResultSamplesPerCondition / 2;
                    ResultHistory.RemoveAt(0, DropCount, EAllowShrinking::No);
                }
            }
        }

        // Overlay live task results
        if (ChildHandle.Has<ck::FFragment_SmTask_Current>())
        {
            if (InOutSmInfo.States.IsValidIndex(InCurrentStateIndex))
            {
                auto& CurrentState = InOutSmInfo.States[InCurrentStateIndex];

                if (TaskIndex < CurrentState.Tasks.Num())
                {
                    CurrentState.Tasks[TaskIndex].Handle = ChildHandle;
                    CurrentState.Tasks[TaskIndex].LastResult = ChildHandle.Get<ck::FFragment_SmTask_Current>().Get_LastResult();
                }
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
        const FString& InInitialStateName,
        uint64 InCurrentFrameNumber)
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
            Segment.EndTime = InNow - InRunStartTime;
            // No history → no known start frame. Estimate at 60 FPS so the timeline
            // still shows reasonable per-frame slicing for the initial state.
            auto FramesElapsed = static_cast<uint64>(FMath::RoundToInt(Segment.EndTime * 60.0));
            Segment.StartFrame = (InCurrentFrameNumber > FramesElapsed)
                ? InCurrentFrameNumber - FramesElapsed
                : 0;
            Segment.EndFrame = InCurrentFrameNumber > Segment.StartFrame
                ? InCurrentFrameNumber
                : Segment.StartFrame;
            Segment.Color = CkSmDebugger::ComputeStateColor(InInitialStateName);
            Segments.Add(MoveTemp(Segment));
        }

        return Segments;
    }

    // First segment: initial state (FromState of first entry) from time 0 to first transition.
    // The first transition's FrameNumber is the EndFrame; StartFrame is unknown (run start),
    // estimated below at 60 FPS from the time delta.
    {
        auto Segment = FCkSmDebugger_TimelineSegment{};
        Segment.StateName = InHistory[0].FromStateName;
        Segment.StartTime = 0.0;
        Segment.EndTime = InHistory[0].RealTimeSeconds - InRunStartTime;
        Segment.EndFrame = InHistory[0].FrameNumber;
        auto FramesElapsed = static_cast<uint64>(FMath::RoundToInt(Segment.EndTime * 60.0));
        Segment.StartFrame = (Segment.EndFrame > FramesElapsed)
            ? Segment.EndFrame - FramesElapsed
            : 0;
        Segment.Color = CkSmDebugger::ComputeStateColor(Segment.StateName);
        Segments.Add(MoveTemp(Segment));
    }

    // Middle/last segments: each history entry's ToState occupies from this entry's time
    // to the next entry's time. Frame range comes from history entry frame numbers (or
    // GFrameCounter for the live last segment).
    for (auto Index = 0; Index < InHistory.Num(); ++Index)
    {
        auto Segment = FCkSmDebugger_TimelineSegment{};
        Segment.StateName = InHistory[Index].ToStateName;
        Segment.StartTime = InHistory[Index].RealTimeSeconds - InRunStartTime;
        Segment.StartFrame = InHistory[Index].FrameNumber;

        if (Index + 1 < InHistory.Num())
        {
            Segment.EndTime = InHistory[Index + 1].RealTimeSeconds - InRunStartTime;
            Segment.EndFrame = InHistory[Index + 1].FrameNumber;
        }
        else
        {
            // Last entry is the currently-active state — extend to "now" so the
            // segment keeps growing while the state is being dwelt in.
            Segment.EndTime = InNow > InRunStartTime ? InNow - InRunStartTime : Segment.StartTime;
            Segment.EndFrame = (InCurrentFrameNumber > Segment.StartFrame)
                ? InCurrentFrameNumber
                : Segment.StartFrame;
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
                // EndTime closes the underline at the next-frame transition's time —
                // that's where this engine frame's chain of state changes ends, so the
                // mark spans every cell entered during this single frame.
                BusyFrame.EndTime = InHistory[Index].RealTimeSeconds - InRunStartTime;
                BusyFrame.FrameNumber = CurrentFrame;
                BusyFrame.TransitionCount = Count;
                BusyFrames.Add(MoveTemp(BusyFrame));
            }

            CurrentFrame = InHistory[Index].FrameNumber;
            Count = 1;
            FirstEntryTime = InHistory[Index].RealTimeSeconds;
        }
    }

    // Check the last group — no trailing transition to read EndTime from, so reuse
    // the last entry's time as both endpoints (single-position mark).
    if (Count >= 2)
    {
        auto BusyFrame = FCkSmDebugger_TimelineBusyFrame{};
        BusyFrame.Time = FirstEntryTime - InRunStartTime;
        BusyFrame.EndTime = InHistory.Last().RealTimeSeconds - InRunStartTime;
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
        double InNow,
        uint64 InCurrentFrameNumber)
    -> TArray<FCkSmDebugger_FrameSegment>
{
    auto Segments = TArray<FCkSmDebugger_FrameSegment>{};

    if (InHistory.IsEmpty())
    {
        // No transitions yet — synthesize a single frame segment from run start to "now"
        // so the timeline can still draw frame-numbered cells for the initial state.
        // We don't know the exact run start frame, so estimate at 60fps; the absolute frame
        // numbers may drift slightly but the cell count and labels are correct.
        if (InNow > InRunStartTime && InCurrentFrameNumber > 0)
        {
            auto Segment = FCkSmDebugger_FrameSegment{};
            Segment.StartTime = 0.0;
            Segment.EndTime = InNow - InRunStartTime;
            auto FramesElapsed = static_cast<uint64>(FMath::RoundToInt(Segment.EndTime * 60.0));
            Segment.StartFrame = (InCurrentFrameNumber > FramesElapsed)
                ? InCurrentFrameNumber - FramesElapsed
                : 0;
            Segment.EndFrame = InCurrentFrameNumber;
            Segments.Add(MoveTemp(Segment));
        }
        return Segments;
    }

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
            // Live segment: terminate at "now" both in time and in frame count, so the
            // timeline can slice the currently-active state into per-frame cells.
            Segment.EndTime = InNow > InRunStartTime ? InNow - InRunStartTime : 0.0;
            Segment.EndFrame = (InCurrentFrameNumber > Segment.StartFrame)
                ? InCurrentFrameNumber
                : Segment.StartFrame;
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

auto
    FCkSmDebugger_DataCollector::
    ComputeLogicalFrame(
        uint64 InRawFrame) const
    -> uint64
{
    uint64 PausedFramesBefore = 0;

    for (const auto& [PauseStart, PauseEnd] : _CompletedPauseFrameIntervals)
    {
        if (InRawFrame > PauseStart)
        {
            PausedFramesBefore += FMath::Min<uint64>(InRawFrame, PauseEnd) - PauseStart;
        }
    }

    if (_WasPausedLastTick && InRawFrame > _GFrameAtPauseStart)
    {
        PausedFramesBefore += FMath::Min<uint64>(InRawFrame, GFrameCounter) - _GFrameAtPauseStart;
    }

    return (InRawFrame > PausedFramesBefore) ? InRawFrame - PausedFramesBefore : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    RestoreTransitionHistories(
        const FString& InSmDebugName,
        FCkSmDebugger_SmInfo& InOutSmInfo)
    -> void
{
    auto* Bucket = _TransitionHistoriesBySm.Find(InSmDebugName);
    if (NOT Bucket)
    { return; }

    for (auto& Trans : InOutSmInfo.Transitions)
    {
        const auto Key = FTransitionKey{Trans.SourceStateClass, Trans.TargetStateClass, Trans.Order};
        if (auto* Ring = Bucket->Find(Key))
        {
            Trans.ConditionResultHistory = Ring->ConditionResultHistory;
            Trans.ResultHistory          = Ring->ResultHistory;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_DataCollector::
    PersistTransitionHistories(
        const FString& InSmDebugName,
        const FCkSmDebugger_SmInfo& InSmInfo)
    -> void
{
    auto& Bucket = _TransitionHistoriesBySm.FindOrAdd(InSmDebugName);

    for (const auto& Trans : InSmInfo.Transitions)
    {
        if (Trans.ConditionResultHistory.IsEmpty() && Trans.ResultHistory.IsEmpty())
        { continue; }

        const auto Key = FTransitionKey{Trans.SourceStateClass, Trans.TargetStateClass, Trans.Order};
        auto& Ring = Bucket.FindOrAdd(Key);
        Ring.ConditionResultHistory = Trans.ConditionResultHistory;
        Ring.ResultHistory          = Trans.ResultHistory;
    }
}

// --------------------------------------------------------------------------------------------------------------------
