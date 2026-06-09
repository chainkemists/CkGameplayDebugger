#pragma once

#include "Engine/DeveloperSettings.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"

#include "CkDebugOverlay_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ck On-Screen Debugger"))
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UCk_DebugOverlay_Settings();

    virtual FName GetCategoryName() const override { return TEXT("Ck"); }

    // Inline layout definitions; highest-priority when resolving a layout tag.
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<FCk_DebugOverlay_Layout> Layouts;

    // Additional layouts loaded from data assets (lower priority than inline Layouts).
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<TSoftObjectPtr<class UCk_DebugOverlay_Layout_PDA>> LayoutAssets;

    // Layout that is active when the overlay first opens.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(Categories="Ck.OnScreenDebugger.Layout"))
    FGameplayTag StartingLayout;
};

// --------------------------------------------------------------------------------------------------------------------
