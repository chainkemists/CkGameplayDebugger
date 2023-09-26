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
    // Should draw the debug information of all existing Sensor Fragments
    // CVar: ck.OverlapBody.ShouldPreviewAllSensors
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Profile",
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA> _DebugProfileToUse;

public:
    CK_PROPERTY_GET(_DebugProfileToUse);
};

// --------------------------------------------------------------------------------------------------------------------

class CKGAMEPLAYDEBUGGER_API UCk_Utils_GameplayDebugger_UserSettings_UE
{
public:
    static auto Get_DebugProfileToUse() -> TObjectPtr<class UCk_GameplayDebugger_DebugProfile_PDA>;
};

// --------------------------------------------------------------------------------------------------------------------
