#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_DataCollector.h"
#include "CkNavDebugger/Data/CkNavDebugger_HealthCheck.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE(FOnNavDebuggerDataRefreshed);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavDebuggerSelectionChanged, FCk_Handle);
DECLARE_MULTICAST_DELEGATE(FOnNavDebuggerHealthCheckUpdated);

// --------------------------------------------------------------------------------------------------------------------
// ViewModel — owns the data collector, drives per-frame collection, and exposes selection
// state + on-demand health-check results to all panels and the in-world debug draw.
//
// Selection mirrors a console var (`Ck.NavDebugger.SelectedEntityId`) so the in-world overlay
// can read it independent of the panel UI. -1 means "draw all agents".
// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_ViewModel
{
public:
    FCkNavDebugger_ViewModel();

    auto Tick(UWorld* InWorld, float InDeltaTime) -> void;

    // Selection
    auto Set_SelectedEntityHandle(FCk_Handle InHandle) -> void;
    auto Get_SelectedEntityHandle() const -> FCk_Handle { return _SelectedEntityHandle; }
    auto Get_SelectedAgentInfo() const -> const FCkNavDebugger_AgentInfo*;

    // Pause
    auto Set_Paused(bool InPaused) -> void { _IsPaused = InPaused; }
    auto Get_Paused() const -> bool { return _IsPaused; }

    // Data accessors
    auto Get_AllAgents() const -> const TArray<FCkNavDebugger_AgentInfo>& { return _DataCollector.Get_AllAgents(); }
    auto Get_NavmeshInfo() const -> const FCkNavDebugger_NavmeshInfo&     { return _DataCollector.Get_NavmeshInfo(); }
    auto Get_FailureLog() const -> const TArray<FCkNavDebugger_FailureLogEntry>& { return _DataCollector.Get_FailureLog(); }
    auto Get_LastWorld() const -> UWorld*                                  { return _LastWorld.Get(); }

    auto Clear_FailureLog() -> void;

    // Health check (one-shot, on demand)
    auto Run_HealthCheck() -> void;
    auto Get_HealthCheckReport() const -> const FCkNavDebugger_HealthCheckReport& { return _HealthCheck; }
    auto Has_HealthCheckReport() const -> bool { return _HealthCheck.Items.Num() > 0; }

    // Delegates
    FOnNavDebuggerDataRefreshed   OnDataRefreshed;
    FOnNavDebuggerSelectionChanged OnSelectionChanged;
    FOnNavDebuggerHealthCheckUpdated OnHealthCheckUpdated;

private:
    FCkNavDebugger_DataCollector _DataCollector;
    FCkNavDebugger_HealthCheckReport _HealthCheck;

    FCk_Handle _SelectedEntityHandle;
    bool _IsPaused = false;

    TWeakObjectPtr<UWorld> _LastWorld;
    int32 _LastSelectedIdHint = -1;   // tracks the cvar so we can sync selection if it changes
};

// --------------------------------------------------------------------------------------------------------------------
