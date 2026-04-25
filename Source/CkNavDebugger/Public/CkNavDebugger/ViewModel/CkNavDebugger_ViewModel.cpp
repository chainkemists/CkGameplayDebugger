#include "CkNavDebugger/ViewModel/CkNavDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------------------------------------------------
// CVars exposed for selection + draw-layer toggles. The in-world overlay reads these directly,
// so panels and CVars share state and stay in sync.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::nav_debugger::cvars
{
    static TAutoConsoleVariable<int32> CVar_SelectedEntityId(
        TEXT("Ck.NavDebugger.SelectedEntityId"),
        -1,
        TEXT("CkNavDebugger: id of the nav agent to focus (-1 = all agents). The in-world overlay\n")
        TEXT("filters by this id; selecting from the agent-list panel writes here too."),
        ECVF_Default);

    // Master overlay toggle. When 0 (default), the in-world overlay only draws while the
    // debugger window is open. When 1, it draws regardless — useful if you want path
    // visualization without the panel taking screen real estate.
    static TAutoConsoleVariable<int32> CVar_OverlayAlwaysOn(
        TEXT("Ck.NavDebugger.OverlayAlwaysOn"),
        0,
        TEXT("CkNavDebugger: 1 = in-world overlay draws even when the debugger window is\n")
        TEXT("closed. 0 (default) = only draw while the debugger window is open."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_DrawPaths(
        TEXT("Ck.NavDebugger.DrawPaths"),
        1,
        TEXT("CkNavDebugger: draw path waypoints in the world (0/1)."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_DrawProjections(
        TEXT("Ck.NavDebugger.DrawProjections"),
        1,
        TEXT("CkNavDebugger: draw vertical lines from agent to projected-on-navmesh location.\n")
        TEXT("Makes 'agent floating above navmesh' setup mistakes immediately visible (0/1)."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_DrawAgentCapsules(
        TEXT("Ck.NavDebugger.DrawAgentCapsules"),
        1,
        TEXT("CkNavDebugger: draw nav agent capsules at agent locations (0/1)."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_DrawStatusText(
        TEXT("Ck.NavDebugger.DrawStatusText"),
        1,
        TEXT("CkNavDebugger: draw floating status/fail-reason text above each nav agent (0/1)."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_DrawNavmeshBounds(
        TEXT("Ck.NavDebugger.DrawNavmeshBounds"),
        0,
        TEXT("CkNavDebugger: draw the navmesh AABB as a wireframe box (0/1)."),
        ECVF_Default);
}

// --------------------------------------------------------------------------------------------------------------------

FCkNavDebugger_ViewModel::FCkNavDebugger_ViewModel()
{
}

auto
    FCkNavDebugger_ViewModel::
    Tick(
        UWorld* InWorld,
        float InDeltaTime)
    -> void
{
    _LastWorld = InWorld;

    // Pull cvar-driven selection (so script / console can set it). If the cvar changed and
    // matches an agent's entity id, sync our selection.
    const auto CVarSelected = ck::nav_debugger::cvars::CVar_SelectedEntityId.GetValueOnGameThread();
    if (CVarSelected != _LastSelectedIdHint)
    {
        _LastSelectedIdHint = CVarSelected;

        if (CVarSelected < 0)
        {
            if (ck::IsValid(_SelectedEntityHandle))
            {
                _SelectedEntityHandle = FCk_Handle{};
                OnSelectionChanged.Broadcast(_SelectedEntityHandle);
            }
        }
        // CVarSelected >= 0 — try to find the matching agent on next collect (handled below).
    }

    if (_IsPaused)
    { return; }

    _DataCollector.Collect(InWorld);

    // If the cvar selected an entity by id and we haven't matched it yet, look it up.
    if (CVarSelected >= 0
        && (NOT ck::IsValid(_SelectedEntityHandle) || GetTypeHash(_SelectedEntityHandle) != static_cast<uint32>(CVarSelected)))
    {
        for (const auto& Agent : _DataCollector.Get_AllAgents())
        {
            if (static_cast<int32>(GetTypeHash(Agent.EntityHandle)) == CVarSelected)
            {
                _SelectedEntityHandle = Agent.EntityHandle;
                OnSelectionChanged.Broadcast(_SelectedEntityHandle);
                break;
            }
        }
    }

    // Drop selection if the entity disappeared from the registry.
    if (ck::IsValid(_SelectedEntityHandle))
    {
        const auto SelectedHash = GetTypeHash(_SelectedEntityHandle);
        auto StillPresent = false;
        for (const auto& Agent : _DataCollector.Get_AllAgents())
        {
            if (GetTypeHash(Agent.EntityHandle) == SelectedHash)
            { StillPresent = true; break; }
        }
        if (NOT StillPresent)
        {
            _SelectedEntityHandle = FCk_Handle{};
            OnSelectionChanged.Broadcast(_SelectedEntityHandle);
        }
    }

    OnDataRefreshed.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_ViewModel::
    Set_SelectedEntityHandle(
        FCk_Handle InHandle)
    -> void
{
    _SelectedEntityHandle = InHandle;
    const auto NewId = ck::IsValid(InHandle) ? static_cast<int32>(GetTypeHash(InHandle)) : -1;
    ck::nav_debugger::cvars::CVar_SelectedEntityId->Set(NewId, ECVF_SetByConsole);
    _LastSelectedIdHint = NewId;
    OnSelectionChanged.Broadcast(_SelectedEntityHandle);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_ViewModel::
    Get_SelectedAgentInfo() const
    -> const FCkNavDebugger_AgentInfo*
{
    if (NOT ck::IsValid(_SelectedEntityHandle))
    { return nullptr; }

    const auto SelectedHash = GetTypeHash(_SelectedEntityHandle);
    for (const auto& Agent : _DataCollector.Get_AllAgents())
    {
        if (GetTypeHash(Agent.EntityHandle) == SelectedHash)
        { return &Agent; }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_ViewModel::
    Clear_FailureLog()
    -> void
{
    _DataCollector.Clear_FailureLog();
    OnDataRefreshed.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_ViewModel::
    Run_HealthCheck()
    -> void
{
    _HealthCheck = FCkNavDebugger_HealthCheck::Run(_LastWorld.Get());
    OnHealthCheckUpdated.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------
