#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"

#include "CkEqsDebugger/Settings/CkEqsDebuggerSettings.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_ViewModel::
    Tick(
        UWorld* InWorld,
        float   /*InDeltaTime*/)
    -> void
{
    if (_IsPaused)
    { return; }

    // Deep-populate every query's candidate list when the user has the "Show All Queries" overlay mode on
    // — the OverlayManager needs candidate locations for every query, not just the selected one. Off by
    // default to keep the typical (selection-driven) collector cheap on large query sets.
    const auto* Settings = UCkEqsDebuggerSettings::Get();
    const auto DeepPopulateAll = Settings != nullptr && Settings->bShow_Overlay && Settings->bShow_AllQueriesAlways;

    _DataCollector.Collect(InWorld, _SelectedQueryHandle, DeepPopulateAll);

    OnQueryListChanged.Broadcast(_DataCollector.Get_AllQueries());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_ViewModel::
    Set_SelectedQueryHandle(
        FCk_Handle_EqsQuery InHandle)
    -> void
{
    if (_SelectedQueryHandle == InHandle)
    { return; }

    _SelectedQueryHandle    = InHandle;
    _HasSelectedQuery       = ck::IsValid(InHandle);
    _SelectedCandidateIndex = -1;   // selecting a new query resets the candidate cursor

    OnSelectedQueryChanged.Broadcast(_SelectedQueryHandle);
    OnSelectedCandidateChanged.Broadcast(_SelectedCandidateIndex);
}

auto
    FCkEqsDebugger_ViewModel::
    Get_SelectedQueryHandle() const
    -> FCk_Handle_EqsQuery
{
    return _SelectedQueryHandle;
}

auto
    FCkEqsDebugger_ViewModel::
    Has_SelectedQuery() const
    -> bool
{
    return _HasSelectedQuery && ck::IsValid(_SelectedQueryHandle);
}

auto
    FCkEqsDebugger_ViewModel::
    Get_AllQueries() const
    -> const TArray<FCkEqsDebugger_QueryInfo>&
{
    return _DataCollector.Get_AllQueries();
}

auto
    FCkEqsDebugger_ViewModel::
    Get_CurrentQueryInfo() const
    -> const FCkEqsDebugger_QueryInfo*
{
    if (NOT Has_SelectedQuery())
    { return nullptr; }
    return _DataCollector.Find_QueryInfo(_SelectedQueryHandle);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_ViewModel::
    Set_SelectedCandidateIndex(
        int32 InIndex)
    -> void
{
    if (_SelectedCandidateIndex == InIndex)
    { return; }

    _SelectedCandidateIndex = InIndex;
    OnSelectedCandidateChanged.Broadcast(_SelectedCandidateIndex);
}

auto
    FCkEqsDebugger_ViewModel::
    Get_SelectedCandidateIndex() const
    -> int32
{
    return _SelectedCandidateIndex;
}

auto
    FCkEqsDebugger_ViewModel::
    Get_CurrentCandidateInfo() const
    -> const FCkEqsDebugger_CandidateInfo*
{
    const auto* QueryInfo = Get_CurrentQueryInfo();
    if (QueryInfo == nullptr)
    { return nullptr; }

    if (NOT QueryInfo->Candidates.IsValidIndex(_SelectedCandidateIndex))
    { return nullptr; }

    return &QueryInfo->Candidates[_SelectedCandidateIndex];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_ViewModel::
    Set_Paused(
        bool InPaused)
    -> void
{
    if (_IsPaused == InPaused)
    { return; }

    _IsPaused = InPaused;
    OnPausedChanged.Broadcast(_IsPaused);
}

auto
    FCkEqsDebugger_ViewModel::
    Get_Paused() const
    -> bool
{
    return _IsPaused;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_ViewModel::
    Reset_ForWorldChange()
    -> void
{
    // Release shared refs on persistent handles BEFORE the registry tears down.
    // FCk_Handle_EqsQuery holds TOptional<FCk_Registry> by value; outliving the registry
    // crashes the destructor (CkSmDebugger/CLAUDE.md).
    _SelectedQueryHandle    = FCk_Handle_EqsQuery{};
    _HasSelectedQuery       = false;
    _SelectedCandidateIndex = -1;

    // Frame-local DTOs live inside the collector — they hold FCk_Handle copies. Reset them
    // explicitly so the next Collect() starts from a clean slate even if EndPIE fires mid-tick.
    _DataCollector.Collect(nullptr, FCk_Handle_EqsQuery{});

    OnSelectedQueryChanged.Broadcast(_SelectedQueryHandle);
    OnSelectedCandidateChanged.Broadcast(_SelectedCandidateIndex);
}

// --------------------------------------------------------------------------------------------------------------------
