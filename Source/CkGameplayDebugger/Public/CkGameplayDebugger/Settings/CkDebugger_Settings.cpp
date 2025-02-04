#include "CkDebugger_Settings.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"

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
    Get_ProjectDefault_DebugProfile()
    -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_ProjectSettings_UE>()->Get_ProjectDefault_DebugProfile().LoadSynchronous();
}

// --------------------------------------------------------------------------------------------------------------------
