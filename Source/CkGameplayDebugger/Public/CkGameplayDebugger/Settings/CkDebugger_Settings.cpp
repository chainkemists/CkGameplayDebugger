#include "CkDebugger_Settings.h"

#include "CkCore/Object/CkObject_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_UserOverride_DebugProfile()
    -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_UserOverride_DebugProfile().LoadSynchronous();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_UserOverride_FontSize()
    -> TOptional<ECk_Engine_TextFontSize>
{
    const auto& DebuggerUserSettings = UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>();

    if (NOT DebuggerUserSettings->Get_Has_UserOverride_FontSize())
    { return {}; }

    return DebuggerUserSettings->Get_UserOverride_FontSize();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_DisplayTranslucentBackground()
    -> bool
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_DisplayTranslucentBackground();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_BackgroundColor()
    -> FLinearColor
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_BackgroundColor();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_BackgroundWidth()
    -> ECk_GameplayDebugger_BackgroundWidth
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_BackgroundWidth();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_EnableTextDropShadow()
    -> bool
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_EnableTextDropShadow();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_ProjectDefault_DebugProfile()
    -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_ProjectSettings_UE>()->Get_ProjectDefault_DebugProfile().LoadSynchronous();
}

auto
    UCk_Utils_GameplayDebugger_Settings_UE::
    Get_ProjectDefault_FontSize()
    -> ECk_Engine_TextFontSize
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_ProjectSettings_UE>()->Get_ProjectDefault_FontSize();
}

// --------------------------------------------------------------------------------------------------------------------
