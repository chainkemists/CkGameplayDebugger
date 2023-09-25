#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameplayDebugger/Action/CkDebugger_Action_Data.h"

#include <InputCoreTypes.h>

#include "CkDebugger_Action.generated.h"

#define CK_GAMEPLAY_DEBUGGER_INVALID_ACTION_NAME TEXT("[NAMELESS ACTION]")

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Abstract, Blueprintable)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugAction_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugAction_UE);

public:
    auto ActivateAction() -> void;
    auto DeactivateAction() -> void;
    auto ToggleAction() -> void;
    auto PerformDebugAction(const FCk_GameplayDebugger_PerformDebugAction_Params& InParams) -> void;

protected:
    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Action",
              meta     = (DisplayName = "PerformDebugAction"))
    void DoPerformDebugAction(const FCk_GameplayDebugger_PerformDebugAction_Params& InParams);

    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Action",
              meta     = (DisplayName = "ActivateAction"))
    void DoActivateAction();

    UFUNCTION(BlueprintNativeEvent,
              Category = "Ck|GameplayDebugger|Action",
              meta     = (DisplayName = "DeactivateAction"))
    void DoDeactivateAction();

private:
    // Name of the action displayed on screen when it is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Action|Params", meta = (AllowPrivateAccess = true))
    FName _ActionName = CK_GAMEPLAY_DEBUGGER_INVALID_ACTION_NAME;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger|Action|Params", meta = (AllowPrivateAccess = true))
    FKey _ToggleActivateKey;

private:
    bool _IsActive = false;

public:
    CK_PROPERTY_GET(_ActionName);
    CK_PROPERTY_GET(_ToggleActivateKey);
    CK_PROPERTY_GET(_IsActive);
};

// --------------------------------------------------------------------------------------------------------------------
