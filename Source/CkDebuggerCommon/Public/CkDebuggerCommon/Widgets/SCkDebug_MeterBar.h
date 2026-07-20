#pragma once

#include "Widgets/SLeafWidget.h"

// ====================================================================================================================
// Thin horizontal meter — cost bars on decision cards, the nerd-strip budget
// bar, the catalog key-budget meter. A rounded track with a rounded fill from
// the left, both attribute-bound so live data animates without rebuilds.
//
// Deliberately minimal: no labels, no ticks. Pair with SCkDebug_StatPair or a
// text block when the number matters.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_MeterBar : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_MeterBar)
        : _Fraction(0.0f)
        , _FillColor(FLinearColor::White)
        , _TrackColor(FLinearColor::Transparent)   // transparent → CkStyle track default
        , _DesiredSize(FVector2D(110.0f, 5.0f))
    {}
        // 0..1, clamped.
        SLATE_ATTRIBUTE(float, Fraction)
        SLATE_ATTRIBUTE(FLinearColor, FillColor)
        SLATE_ARGUMENT(FLinearColor, TrackColor)
        SLATE_ARGUMENT(FVector2D, DesiredSize)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

private:
    TAttribute<float> _Fraction;
    TAttribute<FLinearColor> _FillColor;
    FLinearColor _TrackColor = FLinearColor::Transparent;
    FVector2D _DesiredSize = FVector2D(110.0f, 5.0f);
};

// ====================================================================================================================
