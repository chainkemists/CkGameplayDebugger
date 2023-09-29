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
    Get_HasValidActionName() const
    -> bool
{
    return _ActionName != NAME_None && _ActionName != CK_GAMEPLAY_DEBUGGER_INVALID_ACTION_NAME;
}

auto
    UCk_GameplayDebugger_DebugAction_UE::
    Get_HasValidActionActivationKey() const
    -> bool
{
    return ck::IsValid(_ToggleActivateKey);
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
