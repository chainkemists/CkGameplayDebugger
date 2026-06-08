#pragma once
#include "CkDebugOverlay_RenderStyle.generated.h"

UENUM(BlueprintType)
enum class ECk_DebugOverlay_Density : uint8 { Sectioned, Compact, Ultra };

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
};
