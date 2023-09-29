#pragma once

#include "CkCore/Types/DataAsset/CkDataAsset.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkGameplayDebugger/CkDebugger_Common.h"

#include "CkDebugger_Profile.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(EditInlineNew, Blueprintable, BlueprintType)
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugProfile_PDA : public UCk_DataAsset_PDA
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugProfile_PDA);

private:
    // List of available filters that can be applied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugSubmenu_UE>> _Submenus;

    // List of available filters that can be applied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugFilter_UE>> _Filters;

    // List of actions that are run regardless of which filter is currently active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<class UCk_GameplayDebugger_DebugAction_UE>> _GlobalActions;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugNavControls _DebugNavControls;

    // Optional widget to display while the GameplayDebugger is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TSubclassOf<class UCk_GameplayDebugger_DebugWidget_UE> _HUD_DebugWidget;

public:
    CK_PROPERTY_GET(_Submenus);
    CK_PROPERTY_GET(_Filters);
    CK_PROPERTY_GET(_GlobalActions);
    CK_PROPERTY_GET(_DebugNavControls);
    CK_PROPERTY_GET(_HUD_DebugWidget);
};

// --------------------------------------------------------------------------------------------------------------------
