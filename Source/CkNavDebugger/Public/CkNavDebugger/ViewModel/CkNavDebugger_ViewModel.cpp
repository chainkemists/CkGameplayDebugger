#include "CkNavDebugger/ViewModel/CkNavDebugger_ViewModel.h"

#include "CkNavDebugger/Fragment/CkNavDebugger_Fragment.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // Selection state mirrors into the ECS as a single-instance tag on the
    // selected agent — that's what the EnsureX_Selected / RecolorX processors
    // match on. Keeping the tag and the cvar in sync from a single helper
    // avoids the "panel and processors disagree about who's selected" class
    // of bug.
    auto Apply_SelectionTag(
        FCk_Handle& InOldSelection,
        FCk_Handle& InNewSelection)
        -> void
    {
        if (ck::IsValid(InOldSelection))
        {
            InOldSelection.Try_Remove<ck::FTag_NavDebugger_AgentSelected>();
        }
        if (ck::IsValid(InNewSelection))
        {
            InNewSelection.AddOrGet<ck::FTag_NavDebugger_AgentSelected>();
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CVars exposed for selection + draw-layer toggles. The in-world overlay reads these directly,
// so panels and CVars share state and stay in sync.
// --------------------------------------------------------------------------------------------------------------------

// All draw-toggle CVars now live on UCk_NavDebugger_UserSettings_UE
// (with `meta = (ConsoleVariable = "Ck.NavDebugger.*")`) so the same names
// stay console-tweakable AND get persisted per-user. Only SelectedEntityId
// remains here — it's transient runtime state (the active selection),
// not a setting we want persisted across editor sessions.
namespace ck::nav_debugger::cvars
{
    static TAutoConsoleVariable<int32> CVar_SelectedEntityId(
        TEXT("Ck.NavDebugger.SelectedEntityId"),
        -1,
        TEXT("CkNavDebugger: id of the nav agent to focus (-1 = all agents). The processors\n")
        TEXT("filter by this id via FTag_NavDebugger_AgentSelected; selecting from the agent\n")
        TEXT("list panel writes here too."),
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
                auto OldSelection = _SelectedEntityHandle;
                auto Empty = FCk_Handle{};
                _SelectedEntityHandle = FCk_Handle{};
                Apply_SelectionTag(OldSelection, Empty);
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
                auto OldSelection = _SelectedEntityHandle;
                _SelectedEntityHandle = Agent.EntityHandle;
                Apply_SelectionTag(OldSelection, _SelectedEntityHandle);
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
            // Entity gone — no agent handle to remove the tag from anyway,
            // since the entity destruction takes the tag with it.
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
    auto OldSelection = _SelectedEntityHandle;
    _SelectedEntityHandle = InHandle;

    Apply_SelectionTag(OldSelection, _SelectedEntityHandle);

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
