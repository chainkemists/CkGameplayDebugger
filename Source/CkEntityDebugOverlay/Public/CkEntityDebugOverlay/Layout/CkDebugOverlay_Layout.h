#pragma once

#include "GameplayTagContainer.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"

#include "CkDebugOverlay_Layout.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FCk_DebugOverlay_ProviderEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta=(Categories="Ck.OnScreenDebugger.Provider"))
    FGameplayTag ProviderTag;

    UPROPERTY(EditAnywhere, meta=(Categories="Ck.OnScreenDebugger.Provider"))
    FGameplayTagContainer EnabledFields;

    UPROPERTY(EditAnywhere)
    FGameplayTagQuery EntryFilter;

    UPROPERTY(EditAnywhere)
    bool bOverrideStyle = false;

    UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideStyle"))
    FCk_DebugOverlay_RenderStyle StyleOverride;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FCk_DebugOverlay_Layout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta=(Categories="Ck.OnScreenDebugger.Layout"))
    FGameplayTag LayoutTag;

    UPROPERTY(EditAnywhere)
    FCk_DebugOverlay_RenderStyle DefaultStyle;

    UPROPERTY(EditAnywhere, meta=(Categories="Ck.OnScreenDebugger.Provider"))
    FGameplayTagContainer EnabledProviders;

    UPROPERTY(EditAnywhere)
    TArray<FCk_DebugOverlay_ProviderEntry> Entries;
};

// --------------------------------------------------------------------------------------------------------------------
