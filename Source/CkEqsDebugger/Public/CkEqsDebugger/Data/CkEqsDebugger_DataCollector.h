#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// Iterates the live ECS registry for entities carrying FFragment_EqsQuery_Params, snapshots their state into the
// DTO types in CkEqsDebugger_Types.h. The shallow per-query view (status, generator, test count, result summary) is
// always populated. The deep candidate + per-test breakdown is populated only for the entity matching InSelectedHandle
// (so a 100-query frame only does the heavy work for the one the user is actually inspecting).
// --------------------------------------------------------------------------------------------------------------------

class FCkEqsDebugger_DataCollector
{
public:
    // InDeepPopulateAll: when true, the per-candidate + per-test breakdown is populated for EVERY query
    // (not just the selected one). The "Show All Queries" overlay mode needs candidate locations on every
    // query to draw them in-world. Defaults to false to keep the typical (selection-driven) collector cheap.
    auto
    Collect(
        UWorld*               InWorld,
        FCk_Handle_EqsQuery   InSelectedHandle,
        bool                  InDeepPopulateAll = false) -> void;

    auto
    Get_AllQueries() const -> const TArray<FCkEqsDebugger_QueryInfo>&;

    // Returns nullptr if the selected handle is invalid or wasn't found in the last Collect pass.
    auto
    Find_QueryInfo(
        FCk_Handle_EqsQuery   InHandle) const -> const FCkEqsDebugger_QueryInfo*;

private:
    static auto
    CollectShallow(
        const FCk_Handle_EqsQuery& InQueryHandle) -> FCkEqsDebugger_QueryInfo;

    static auto
    PopulateCandidatesAndBreakdown(
        const FCk_Handle_EqsQuery& InQueryHandle,
        FCkEqsDebugger_QueryInfo&  InOutInfo) -> void;

    static auto
    DetermineStatus(
        const FCk_Handle_EqsQuery& InQueryHandle) -> ECkEqsDebugger_QueryStatus;

private:
    TArray<FCkEqsDebugger_QueryInfo> _Queries;
};

// --------------------------------------------------------------------------------------------------------------------
