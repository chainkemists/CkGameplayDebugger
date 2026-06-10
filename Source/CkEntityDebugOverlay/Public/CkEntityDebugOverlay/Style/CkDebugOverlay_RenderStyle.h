#pragma once
#include "CkDebugOverlay_RenderStyle.generated.h"

UENUM(BlueprintType)
enum class ECk_DebugOverlay_Density : uint8 { Sectioned, Compact, Ultra };

// Viewport corner/edge the focus card (plate) is anchored to.
UENUM(BlueprintType)
enum class ECk_DebugOverlay_PlateAnchor : uint8
{
    TopLeft,
    TopCenter,
    TopRight,
    Left,
    Right,
    BottomLeft,
    BottomCenter,
    BottomRight
};

UENUM(BlueprintType)
enum class ECk_DebugOverlay_HistoryStyle : uint8 { InlineBreadcrumb, TwoLineSub, OwnLine };

USTRUCT(BlueprintType)
struct FCk_DebugOverlay_RenderStyle
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) ECk_DebugOverlay_Density      Density        = ECk_DebugOverlay_Density::Ultra;
    UPROPERTY(EditAnywhere) ECk_DebugOverlay_HistoryStyle HistoryStyle   = ECk_DebugOverlay_HistoryStyle::InlineBreadcrumb;
    UPROPERTY(EditAnywhere) int32                         HistoryDepth   = 1;
    UPROPERTY(EditAnywhere) bool                          bFlashOnChange = true;

    // Multiplier on all plate font sizes. The settings' PlateFontScale is multiplied
    // in on top of this per-layout value when the card is pushed each tick.
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.5", ClampMax="2.0"))
    float FontScale = 1.0f;
};
