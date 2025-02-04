#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkSettings/ProjectSettings/CkProjectSettings.h"
#include "CkSettings/UserSettings/CkUserSettings.h"

#include <CoreMinimal.h>

#include "CkDebugger_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_UserSettings_UE : public UCk_Plugin_UserSettings_UE
{
    GENERATED_BODY()

private:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _UserOverride_DebugProfile;

public:
    CK_PROPERTY_GET(_UserOverride_DebugProfile);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(defaultconfig, meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_ProjectSettings_UE : public UCk_Plugin_ProjectSettings_UE
{
    GENERATED_BODY()

private:
    // Default Gameplay Debugger Debug Profile to use if there is no override in the Editor Preferences
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _ProjectDefault_DebugProfile;

public:
    CK_PROPERTY_GET(_ProjectDefault_DebugProfile);
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_Settings_UE
{
public:
    static auto Get_UserOverride_DebugProfile() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
    static auto Get_ProjectDefault_DebugProfile() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
};

// --------------------------------------------------------------------------------------------------------------------
