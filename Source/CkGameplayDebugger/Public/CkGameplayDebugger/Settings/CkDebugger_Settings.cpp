#include "CkDebugger_Settings.h"

#include "CkCore/Object/CkObject_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

UCk_GameplayDebugger_UserSettings_UE::
    UCk_GameplayDebugger_UserSettings_UE(
        const FObjectInitializer& InObjectInitializer)
    : Super(InObjectInitializer)
{
    CategoryName = FName{TEXT("ChainKemists")};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_GameplayDebugger_UserSettings_UE::
    Get_DebugProfileToUse()
    -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>
{
    return UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_GameplayDebugger_UserSettings_UE>()->Get_DebugProfileToUse().LoadSynchronous();
}

// --------------------------------------------------------------------------------------------------------------------
