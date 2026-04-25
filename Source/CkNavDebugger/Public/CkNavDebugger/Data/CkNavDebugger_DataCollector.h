#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// Per-frame collector: walks the registry to snapshot all nav agents + records system-level
// navmesh state + maintains a rolling failure log keyed off status transitions.
// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_DataCollector
{
public:
    auto
    Collect(
        UWorld* InWorld) -> void;

    auto
    Get_AllAgents() const -> const TArray<FCkNavDebugger_AgentInfo>& { return _Agents; }

    auto
    Get_NavmeshInfo() const -> const FCkNavDebugger_NavmeshInfo& { return _NavmeshInfo; }

    auto
    Get_FailureLog() const -> const TArray<FCkNavDebugger_FailureLogEntry>& { return _FailureLog; }

    auto
    Clear_FailureLog() -> void { _FailureLog.Reset(); }

    static constexpr int32 FailureLog_MaxEntries = 64;

private:
    auto
    DoCollectAgent(
        const FCk_Handle& InHandle) -> FCkNavDebugger_AgentInfo;

    auto
    DoCollectNavmesh(
        UWorld* InWorld) -> void;

    auto
    DoTrackFailure(
        const FCkNavDebugger_AgentInfo& InInfo) -> void;

private:
    TArray<FCkNavDebugger_AgentInfo> _Agents;
    FCkNavDebugger_NavmeshInfo _NavmeshInfo;
    TArray<FCkNavDebugger_FailureLogEntry> _FailureLog;

    // Per-handle status tracking so we only log the *transition* into Failed, not every frame
    // an agent stays Failed.
    TMap<uint32, ECk_Nav_PathStatus> _LastKnownStatus;
};

// --------------------------------------------------------------------------------------------------------------------
