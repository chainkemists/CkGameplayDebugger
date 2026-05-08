#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Reset_ForWorldChange()
    -> void
{
    _DataCollector.Reset();
    _SelectedSmHandle = FCk_Handle_StateMachine{};
    _HasSelectedSm = false;
    _CurrentSmInfo = FCkSmDebugger_SmInfo{};
    _HasSmInfo = false;
    _SelectedNodeIndex = -1;

    _ViewMode = ECkSmDebugger_ViewMode::Live;
    _ScrubState = FCkSmDebugger_ScrubState{};
    _ScrubSnapshot = FCkSmDebugger_ScrubSnapshot{};
    _ScrubHighlightSource = -1;
    _ScrubHighlightTarget = -1;
    _NeedsRelayout = true;
    _Bookmarks.Empty();

    // Broadcast empties so every listener drops stale data in one pass
    OnSmListChanged.Broadcast(_DataCollector.Get_AllStateMachines());
    OnSelectedNodeChanged.Broadcast(_SelectedNodeIndex);
    OnViewModeChanged.Broadcast(_ViewMode);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Tick(
        UWorld* InWorld,
        float InDeltaTime)
    -> void
{
    if (NOT IsValid(InWorld))
    { return; }

    _DataCollector.Collect(InWorld);

    auto& AllSms = _DataCollector.Get_AllStateMachines();
    OnSmListChanged.Broadcast(AllSms);

    if (NOT _HasSelectedSm)
    {
        _HasSmInfo = false;
        return;
    }

    auto FoundIndex = AllSms.IndexOfByPredicate([this](const FCkSmDebugger_SmInfo& InInfo)
    {
        return InInfo.Handle == _SelectedSmHandle;
    });

    if (FoundIndex == INDEX_NONE)
    {
        _HasSmInfo = false;
        return;
    }

    _CurrentSmInfo = AllSms[FoundIndex];
    _HasSmInfo = true;

    if (_ViewMode == ECkSmDebugger_ViewMode::Scrub)
    { ComputeScrubSnapshot(); }

    OnSmDataRefreshed.Broadcast(_CurrentSmInfo);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Set_SelectedSmHandle(
        FCk_Handle_StateMachine InHandle)
    -> void
{
    if (_SelectedSmHandle == InHandle)
    { return; }

    _SelectedSmHandle = InHandle;
    _HasSelectedSm = true;
    _HasSmInfo = false;
    _SelectedNodeIndex = -1;
    _NeedsRelayout = true;

    _ViewMode = ECkSmDebugger_ViewMode::Live;
    _ScrubState = FCkSmDebugger_ScrubState{};
    _ScrubSnapshot = FCkSmDebugger_ScrubSnapshot{};
    _Bookmarks.Empty();
    ClearScrubTransitionHighlight();

    OnSelectedNodeChanged.Broadcast(_SelectedNodeIndex);
    OnViewModeChanged.Broadcast(_ViewMode);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    AddBookmark(
        double InRunRelativeTime)
    -> void
{
    // Deduplicate within a small tolerance so rapid double-presses don't accumulate.
    constexpr auto Tolerance = 0.01;
    for (auto Existing : _Bookmarks)
    {
        if (FMath::Abs(Existing - InRunRelativeTime) < Tolerance) { return; }
    }
    _Bookmarks.Add(InRunRelativeTime);
    _Bookmarks.Sort();
}

auto
    FCkSmDebugger_ViewModel::
    RemoveNearestBookmark(
        double InRunRelativeTime,
        double InTolerance)
    -> void
{
    auto BestIdx = -1;
    auto BestDist = InTolerance;
    for (auto i = 0; i < _Bookmarks.Num(); ++i)
    {
        auto Dist = FMath::Abs(_Bookmarks[i] - InRunRelativeTime);
        if (Dist <= BestDist)
        {
            BestDist = Dist;
            BestIdx = i;
        }
    }
    if (BestIdx >= 0)
    { _Bookmarks.RemoveAt(BestIdx); }
}

auto
    FCkSmDebugger_ViewModel::
    FindNextBookmark(
        double InRunRelativeTime,
        int32 InDirection) const
    -> double
{
    if (_Bookmarks.IsEmpty()) { return -1.0; }
    constexpr auto Epsilon = 0.001;
    if (InDirection > 0)
    {
        for (auto B : _Bookmarks)
        {
            if (B > InRunRelativeTime + Epsilon) { return B; }
        }
    }
    else
    {
        for (auto i = _Bookmarks.Num() - 1; i >= 0; --i)
        {
            if (_Bookmarks[i] < InRunRelativeTime - Epsilon) { return _Bookmarks[i]; }
        }
    }
    return -1.0;
}

auto
    FCkSmDebugger_ViewModel::
    Get_SelectedSmHandle() const
    -> FCk_Handle_StateMachine
{
    return _SelectedSmHandle;
}

auto
    FCkSmDebugger_ViewModel::
    Has_SelectedSm() const
    -> bool
{
    return _HasSelectedSm;
}

auto
    FCkSmDebugger_ViewModel::
    Get_AllStateMachines() const
    -> const TArray<FCkSmDebugger_SmInfo>&
{
    return _DataCollector.Get_AllStateMachines();
}

auto
    FCkSmDebugger_ViewModel::
    Get_CurrentSmInfo() const
    -> const FCkSmDebugger_SmInfo*
{
    return _HasSmInfo ? &_CurrentSmInfo : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Get_MutableSmInfo()
    -> FCkSmDebugger_SmInfo*
{
    return _HasSmInfo ? &_CurrentSmInfo : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Set_SelectedNodeIndex(
        int32 InIndex)
    -> void
{
    if (_SelectedNodeIndex == InIndex)
    { return; }

    _SelectedNodeIndex = InIndex;
    OnSelectedNodeChanged.Broadcast(_SelectedNodeIndex);
}

auto
    FCkSmDebugger_ViewModel::
    Get_SelectedNodeIndex() const
    -> int32
{
    return _SelectedNodeIndex;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Set_ViewMode(
        ECkSmDebugger_ViewMode InMode)
    -> void
{
    if (_ViewMode == InMode)
    { return; }

    _ViewMode = InMode;

    if (_ViewMode == ECkSmDebugger_ViewMode::Live)
    {
        ClearScrubTransitionHighlight();
        _ScrubSnapshot = FCkSmDebugger_ScrubSnapshot{};
        _LiveSegmentFreezeActive = false;
        _LiveSegmentFreezeStateName.Empty();
    }
    else if (_ViewMode == ECkSmDebugger_ViewMode::Scrub && _HasSmInfo)
    {
        // Capture the live segment's End at the moment scrubbing begins. The renderer
        // overrides the segment's growing tail with these frozen values until the user
        // returns to Live, so per-frame cells stay readable instead of shrinking as
        // real time advances.
        auto& Segs = _CurrentSmInfo.CurrentRun.Segments;
        if (Segs.Num() > 0)
        {
            auto& Last = Segs.Last();
            _LiveSegmentFreezeStateName = Last.StateName;
            _LiveSegmentFreezeEndTime = Last.EndTime;
            _LiveSegmentFreezeEndFrame = Last.EndFrame;
            _LiveSegmentFreezeActive = true;
        }
    }

    OnViewModeChanged.Broadcast(_ViewMode);
}

auto
    FCkSmDebugger_ViewModel::
    Get_ViewMode() const
    -> ECkSmDebugger_ViewMode
{
    return _ViewMode;
}

auto
    FCkSmDebugger_ViewModel::
    Set_ScrubState(
        const FCkSmDebugger_ScrubState& InState)
    -> void
{
    _ScrubState = InState;

    if (_HasSmInfo)
    { ComputeScrubSnapshot(); }

    OnScrubStateChanged.Broadcast(_ScrubState);
}

auto
    FCkSmDebugger_ViewModel::
    Get_ScrubState() const
    -> const FCkSmDebugger_ScrubState&
{
    return _ScrubState;
}

auto
    FCkSmDebugger_ViewModel::
    Get_ScrubSnapshot() const
    -> const FCkSmDebugger_ScrubSnapshot&
{
    return _ScrubSnapshot;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    SetScrubTransitionHighlight(
        int32 InSourceIndex,
        int32 InTargetIndex)
    -> void
{
    _ScrubHighlightSource = InSourceIndex;
    _ScrubHighlightTarget = InTargetIndex;
}

auto
    FCkSmDebugger_ViewModel::
    ClearScrubTransitionHighlight()
    -> void
{
    _ScrubHighlightSource = -1;
    _ScrubHighlightTarget = -1;
}

auto
    FCkSmDebugger_ViewModel::
    Get_ScrubHighlightSource() const
    -> int32
{
    return _ScrubHighlightSource;
}

auto
    FCkSmDebugger_ViewModel::
    Get_ScrubHighlightTarget() const
    -> int32
{
    return _ScrubHighlightTarget;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    RequestRelayout()
    -> void
{
    _NeedsRelayout = true;
    OnRelayoutRequested.Broadcast();
}

auto
    FCkSmDebugger_ViewModel::
    Get_NeedsRelayout() const
    -> bool
{
    return _NeedsRelayout;
}

auto
    FCkSmDebugger_ViewModel::
    ConsumeRelayout()
    -> void
{
    _NeedsRelayout = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    IssueCommand(
        const FCkSmDebugger_Command& InCommand)
    -> void
{
    OnCommandIssued.Broadcast(InCommand);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    Set_ExpandAllNodes(
        bool InExpand)
    -> void
{
    if (_ExpandAllNodes == InExpand)
    { return; }

    _ExpandAllNodes = InExpand;
    _NeedsRelayout = true;
}

auto
    FCkSmDebugger_ViewModel::
    Get_ExpandAllNodes() const
    -> bool
{
    return _ExpandAllNodes;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugger_ViewModel::
    FindRunForScrub() const
    -> const FCkSmDebugger_RunInfo*
{
    if (NOT _HasSmInfo)
    { return nullptr; }

    auto RunIndex = _ScrubState.SelectedRunIndex;

    if (RunIndex < 0)
    { return &_CurrentSmInfo.CurrentRun; }

    if (RunIndex < _CurrentSmInfo.CompletedRuns.Num())
    { return &_CurrentSmInfo.CompletedRuns[RunIndex]; }

    return nullptr;
}

auto
    FCkSmDebugger_ViewModel::
    ComputeScrubSnapshot()
    -> void
{
    _ScrubSnapshot = FCkSmDebugger_ScrubSnapshot{};

    auto Run = FindRunForScrub();
    if (NOT Run)
    { return; }

    auto& History = Run->History;
    if (History.Num() == 0)
    { return; }

    // ScrubTime is run-relative (seconds since run start), but History[i].RealTimeSeconds
    // is absolute logical time (same space as Run->StartTime). Convert ScrubTime to the
    // absolute space once so the loop comparisons are well-formed.
    auto ScrubTime = _ScrubState.ScrubTime + Run->StartTime;

    auto BestIndex = -1;
    for (auto i = 0; i < History.Num(); ++i)
    {
        if (History[i].RealTimeSeconds <= ScrubTime)
        { BestIndex = i; }
        else
        { break; }
    }

    if (BestIndex < 0)
    {
        auto& FirstEntry = History[0];
        _ScrubSnapshot.ActiveStateName = FirstEntry.FromStateName;
        _ScrubSnapshot.TimeInState = 0.0;
        _ScrubSnapshot.HistoryIndex = -1;

        for (auto i = 0; i < _CurrentSmInfo.States.Num(); ++i)
        {
            if (_CurrentSmInfo.States[i].StateName == FirstEntry.FromStateName)
            {
                _ScrubSnapshot.ActiveStateIndex = i;
                break;
            }
        }
        return;
    }

    auto& Entry = History[BestIndex];
    _ScrubSnapshot.ActiveStateName = Entry.ToStateName;
    _ScrubSnapshot.HistoryIndex = BestIndex;

    auto NextTransitionTime = (BestIndex + 1 < History.Num())
        ? History[BestIndex + 1].RealTimeSeconds
        : ScrubTime;

    _ScrubSnapshot.TimeInState = FMath::Max(0.0, ScrubTime - Entry.RealTimeSeconds);

    for (auto i = 0; i < _CurrentSmInfo.States.Num(); ++i)
    {
        if (_CurrentSmInfo.States[i].StateName == Entry.ToStateName)
        {
            _ScrubSnapshot.ActiveStateIndex = i;
            break;
        }
    }

}

// --------------------------------------------------------------------------------------------------------------------
