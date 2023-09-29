#include "CkDebugger_Submenu.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/IO/CkIO_Utils.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerTypes.h"

#if WITH_GAMEPLAY_DEBUGGER
#include <Engine/Canvas.h>
#include <Engine/Engine.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    ActivateSubmenu()
    -> void
{
    DoActivateSubmenu();
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    DeactivateSubmenu()
    -> void
{
    DoDeactivateSubmenu();
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    DrawData(
        const FCk_GameplayDebugger_DrawSubmenuData_Params& InParams)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& dataToDraw = DoDrawData(InParams);
    const auto& canvasContext = InParams.Get_DrawData().Get_CanvasContext();

    if (ck::Is_NOT_Valid(canvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    canvasContext->Print(ck::Format_UE(TEXT("{{white}}== {{yellow}}{} {{white}}=="), _MenuName));

    const auto originalFont = canvasContext->Font;

    if (_UseCustomFontSize)
    {
        canvasContext->Font = UCk_Utils_IO_UE::Get_Engine_DefaultTextFont(_CustomTextFontSize);
    }

    for (const auto& line : dataToDraw.Get_DebugDataToDraw())
    {
        canvasContext->Print(ck::Format_UE(TEXT("{}"), line.ToString()));
    }

    canvasContext->Font = originalFont;
#endif
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    ToggleShowState()
    -> ECk_GameplayDebugger_DebugSubmenu_ShowState
{
    _ShowState = _ShowState == ECk_GameplayDebugger_DebugSubmenu_ShowState::Hidden
                                ? ECk_GameplayDebugger_DebugSubmenu_ShowState::Visible
                                : ECk_GameplayDebugger_DebugSubmenu_ShowState::Hidden;

    return _ShowState;
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    Get_HasValidSubmenuName() const
    -> bool
{
    return _MenuName != NAME_None && _MenuName != CK_GAMEPLAY_DEBUGGER_INVALID_SUBMENU_NAME;
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    Get_HasValidSubmenuActivationKey() const
    -> bool
{
    return ck::IsValid(_KeyToShowMenu);
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    DoActivateSubmenu_Implementation()
    -> void
{
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    DoDeactivateSubmenu_Implementation()
    -> void
{
}

auto
    UCk_GameplayDebugger_DebugSubmenu_UE::
    DoDrawData_Implementation(
        const FCk_GameplayDebugger_DrawSubmenuData_Params& InParams)
    -> FCk_GameplayDebugger_DrawSubmenuData_Result
{
    return {};
}

// --------------------------------------------------------------------------------------------------------------------

