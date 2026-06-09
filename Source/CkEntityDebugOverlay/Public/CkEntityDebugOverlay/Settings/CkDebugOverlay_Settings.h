#pragma once

#include "Engine/DeveloperSettings.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "InputCoreTypes.h"

#include "CkDebugOverlay_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ck On-Screen Debugger"))
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UCk_DebugOverlay_Settings();

    virtual FName GetCategoryName() const override { return TEXT("Ck"); }

    // ---- Layouts ----

    // Inline layout definitions; highest-priority when resolving a layout tag.
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<FCk_DebugOverlay_Layout> Layouts;

    // Additional layouts loaded from data assets (lower priority than inline Layouts).
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<TSoftObjectPtr<class UCk_DebugOverlay_Layout_PDA>> LayoutAssets;

    // Layout that is active when the overlay first opens.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(Categories="Ck.OnScreenDebugger.Layout"))
    FGameplayTag StartingLayout;

    // ---- World Tags (B1 — distance-scaled / faded / culled pills) ----

    // Distance below which pills appear at full size (unscaled).
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float NearDist = 600.0f;

    // Distance at which pills reach MinScale. Lerp from NearDist to FarDist.
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float FarDist = 4000.0f;

    // Minimum scale factor applied to pills at FarDist and beyond.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0.1", ClampMax="1.0"))
    float MinScale = 0.5f;

    // Distance at which pill opacity begins fading toward MinOpacity (0.15).
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float FadeStartDist = 3000.0f;

    // Hard cull distance: pills beyond this range are not emitted at all.
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float MaxDist = 5000.0f;

    // ---- Input (B3 — double-tap lock) ----

    // Key to tap twice quickly to toggle focus lock.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey LockKey = EKeys::LeftShift;

    // Maximum interval (seconds) between two taps to count as a double-tap.
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="0.05", ClampMax="1.0"))
    float LockDoubleTapWindowSeconds = 0.3f;
};

// --------------------------------------------------------------------------------------------------------------------
