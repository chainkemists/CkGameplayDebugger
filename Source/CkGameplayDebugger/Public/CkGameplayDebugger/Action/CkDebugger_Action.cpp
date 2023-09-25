#include "CkDebugger_Action.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_DebugAction_UE::
    ActivateAction()
    -> void
{
    if (_IsActive)
    { return; }

    _IsActive = true;
    DoActivateAction();
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    DeactivateAction()
    -> void
{
    if (NOT _IsActive)
    { return; }

    _IsActive = false;
    DoDeactivateAction();
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    ToggleAction()
    -> void
{
    if (_IsActive)
    {
        DeactivateAction();
    }
    else
    {
        ActivateAction();
    }
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    PerformDebugAction(
        const FCk_GameplayDebugger_PerformDebugAction_Params& InParams)
    -> void
{
    if (NOT _IsActive)
    { return; }

    DoPerformDebugAction(InParams);
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    DoPerformDebugAction_Implementation(
        const FCk_GameplayDebugger_PerformDebugAction_Params& InParams)
    -> void
{
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    DoActivateAction_Implementation()
    -> void
{
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    DoDeactivateAction_Implementation()
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------
