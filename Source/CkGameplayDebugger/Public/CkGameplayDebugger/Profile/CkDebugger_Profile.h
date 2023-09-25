#pragma once

#include "CkCore/Types/DataAsset/CkDataAsset.h"
#include "CkCore/Macros/CkMacros.h"

#include <InputCoreTypes.h>

#include "CkDebugger_Profile.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_CyclingControls
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_CyclingControls);

private:
    // Key to press to select the next filter in the list of filters available
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _NextFilterKey = EKeys::Up;

    // Key to press to select the previous filter in the list of filters available
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _PreviousFilterKey = EKeys::Down;

    // Key to press to select the next actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _NextActorKey = EKeys::Right;

    // Key to press to select the previous actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _PreviousActorKey = EKeys::Left;

    // Key to press to select the first actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _FirstActorKey = EKeys::Home;

    // Key to press to select the last actor in the filtered lists of actor
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FKey _LastActorKey = EKeys::End;

public:
    CK_PROPERTY_GET(_NextFilterKey);
    CK_PROPERTY_GET(_PreviousFilterKey);
    CK_PROPERTY_GET(_NextActorKey);
    CK_PROPERTY_GET(_PreviousActorKey);
    CK_PROPERTY_GET(_FirstActorKey);
    CK_PROPERTY_GET(_LastActorKey);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(EditInlineNew, Blueprintable, BlueprintType)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugProfile_PDA : public UCk_DataAsset_PDA
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugProfile_PDA);

private:
    // List of available filters that can be applied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugSubmenu_UE>> _Submenus;

    // List of available filters that can be applied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugFilter_UE>> _Filters;

    // List of actions that are run regardless of which filter is currently active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugAction_UE>> _GlobalActions;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_CyclingControls _ActorCyclingControls;

public:
    CK_PROPERTY_GET(_Submenus);
    CK_PROPERTY_GET(_Filters);
    CK_PROPERTY_GET(_GlobalActions);
    CK_PROPERTY_GET(_ActorCyclingControls);
};

// --------------------------------------------------------------------------------------------------------------------
