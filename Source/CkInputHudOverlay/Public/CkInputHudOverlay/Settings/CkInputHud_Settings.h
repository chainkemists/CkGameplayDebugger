#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkInputHud_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// Project-level tuning for the on-screen input overlay's RIBBON.
//
// The cvar surface (ck.InputOverlay / .Scale / .Corner) deliberately stays at master/scale/corner only: those three
// are what a QA session toggles mid-play, and everything below is a look the project settles once. Every consumer
// reads the static getters rather than the UPROPERTY, so an out-of-range value hand-edited into DefaultGame.ini is
// clamped at the read instead of reaching the paint path.
// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ck Input HUD Overlay"))
class CKINPUTHUDOVERLAY_API UCk_InputHud_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return TEXT("Ck"); }

public:
    // How many RELEASED chips the ribbon keeps. Held chips are pinned and never counted against this.
    UPROPERTY(Config, EditAnywhere, Category="Ribbon", meta=(ClampMin="3", ClampMax="20"))
    int32 HistoryCap = 8;

    // Lifetime of a released chip. It holds full opacity for the first 30% and fades linearly to nothing over the
    // rest, at which point it is pruned from the model.
    UPROPERTY(Config, EditAnywhere, Category="Ribbon", meta=(ClampMin="3.0", ClampMax="30.0"))
    float FadeLifetimeSeconds = 10.0f;

    // Down-time past which a press reads as a HOLD rather than a tap — the line the pulse glyph switches from a
    // dot to a growing bar on.
    UPROPERTY(Config, EditAnywhere, Category="Ribbon", meta=(ClampMin="50.0", ClampMax="2000.0"))
    float TapHoldThresholdMs = 300.0f;

    // Draw the frame deck (press frame for taps and live holds, "down-up" for completed ones).
    UPROPERTY(Config, EditAnywhere, Category="Ribbon")
    bool ShowFrameNumbers = true;

    // Width the hold bar saturates at. It is the chip's width budget as much as a visual cap.
    UPROPERTY(Config, EditAnywhere, Category="Ribbon", meta=(ClampMin="8.0", ClampMax="64.0"))
    float HoldBarMaxPx = 16.0f;

public:
    static auto Get_HistoryCap()           -> int32;
    static auto Get_FadeLifetimeSeconds()  -> float;
    static auto Get_TapHoldThresholdMs()   -> float;
    static auto Get_ShowFrameNumbers()     -> bool;
    static auto Get_HoldBarMaxPx()         -> float;
};

// --------------------------------------------------------------------------------------------------------------------
