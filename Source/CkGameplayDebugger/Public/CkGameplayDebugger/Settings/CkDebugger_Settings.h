#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <Engine/DeveloperSettings.h>

#include <CoreMinimal.h>
#include <Templates/SubclassOf.h>

#include "CkDebugger_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_UserSettings_UE : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    explicit UCk_GameplayDebugger_UserSettings_UE(const FObjectInitializer& ObjectInitializer);

private:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _DebugProfileUserOverride;

public:
    CK_PROPERTY_GET(_DebugProfileUserOverride);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Gameplay Debugger"))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_ProjectSettings_UE : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    explicit UCk_GameplayDebugger_ProjectSettings_UE(const FObjectInitializer& ObjectInitializer);

private:
    // Default Gameplay Debugger Debug Profile to use if there is no override in the Editor Preferences
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _DefaultDebugProfileToUse;

public:
    CK_PROPERTY_GET(_DefaultDebugProfileToUse);
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_UserSettings_UE
{
public:
    static auto Get_DebugProfileUserOverride() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_ProjectSettings_UE
{
public:
    static auto Get_DefaultDebugProfileToUse() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
};

// --------------------------------------------------------------------------------------------------------------------
