#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"
#include "CkEqsDebugger/Data/CkEqsDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Delegates
// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FCkEqsDebugger_OnQueryListChanged, const TArray<FCkEqsDebugger_QueryInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkEqsDebugger_OnSelectedQueryChanged, FCk_Handle_EqsQuery);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkEqsDebugger_OnSelectedCandidateChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkEqsDebugger_OnPausedChanged, bool);

// --------------------------------------------------------------------------------------------------------------------
// Shared state model for all EQS debugger sub-widgets. Owns the DataCollector, holds selection, broadcasts
// refresh / selection / pause events the panels listen to.
//
// PIE lifetime contract (from CkSmDebugger/CLAUDE.md):
//   - FCk_Handle / FCk_Handle_EqsQuery hold a TOptional<FCk_Registry> by value. Outliving the registry crashes
//     the destructor.
//   - Persistent handle fields (_SelectedQueryHandle here) MUST be cleared via Reset_ForWorldChange() on
//     FEditorDelegates::EndPIE / BeginPIE — runs while the registry is still live, so assignment-replacing
//     releases shared refs cleanly.
//   - Handles inside _DataCollector._Queries are frame-local (Reset() at the top of each Collect call), so
//     the only persistent handle on this ViewModel is _SelectedQueryHandle.
// --------------------------------------------------------------------------------------------------------------------

class FCkEqsDebugger_ViewModel
{
public:
    FCkEqsDebugger_OnQueryListChanged       OnQueryListChanged;
    FCkEqsDebugger_OnSelectedQueryChanged   OnSelectedQueryChanged;
    FCkEqsDebugger_OnSelectedCandidateChanged OnSelectedCandidateChanged;
    FCkEqsDebugger_OnPausedChanged          OnPausedChanged;

    // -----------------------------------------------------------------------------------------------------------------
    // Tick — collect data and broadcast refresh
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Tick(
        UWorld* InWorld,
        float   InDeltaTime) -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Query selection
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedQueryHandle(
        FCk_Handle_EqsQuery InHandle) -> void;

    auto
    Get_SelectedQueryHandle() const -> FCk_Handle_EqsQuery;

    auto
    Has_SelectedQuery() const -> bool;

    auto
    Get_AllQueries() const -> const TArray<FCkEqsDebugger_QueryInfo>&;

    // Returns nullptr if no query is selected, or the selected handle wasn't in the last collect pass.
    auto
    Get_CurrentQueryInfo() const -> const FCkEqsDebugger_QueryInfo*;

    // -----------------------------------------------------------------------------------------------------------------
    // Candidate selection (within the selected query)
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedCandidateIndex(
        int32 InIndex) -> void;

    auto
    Get_SelectedCandidateIndex() const -> int32;

    // Returns nullptr if no query selected, or candidate index out of range.
    auto
    Get_CurrentCandidateInfo() const -> const FCkEqsDebugger_CandidateInfo*;

    // -----------------------------------------------------------------------------------------------------------------
    // Pause state
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_Paused(
        bool InPaused) -> void;

    auto
    Get_Paused() const -> bool;

    // -----------------------------------------------------------------------------------------------------------------
    // PIE lifetime — call from window's EndPIE / BeginPIE handler
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Reset_ForWorldChange() -> void;

private:
    FCkEqsDebugger_DataCollector _DataCollector;

    FCk_Handle_EqsQuery _SelectedQueryHandle;
    bool                _HasSelectedQuery = false;

    int32               _SelectedCandidateIndex = -1;

    bool                _IsPaused = false;
};

// --------------------------------------------------------------------------------------------------------------------
