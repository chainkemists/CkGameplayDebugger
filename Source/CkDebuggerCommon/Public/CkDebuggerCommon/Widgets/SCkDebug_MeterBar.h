#pragma once

#include "Widgets/SLeafWidget.h"

// ====================================================================================================================
// Thin horizontal meter — cost bars on decision cards, the nerd-strip budget
// bar, the catalog key-budget meter. A rounded track with a rounded fill from
// the left, both attribute-bound so live data animates without rebuilds.
//
// An optional TargetFraction draws a thin rule where the value is HEADED, so a
// meter can show "current filling toward a target" as one picture rather than
// as two numbers the reader has to difference in their head (the audio mixer's
// crossfade lane, an attribute refilling toward its max).
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
        , _TargetFraction(TOptional<float>{})
        , _TargetColor(FLinearColor::White)
        , _DesiredSize(FVector2D(110.0f, 5.0f))
    {}
        // 0..1, clamped.
        SLATE_ATTRIBUTE(float, Fraction)
        SLATE_ATTRIBUTE(FLinearColor, FillColor)
        SLATE_ARGUMENT(FLinearColor, TrackColor)

        // 0..1, clamped. Unset draws nothing — which is every call site that predates this.
        SLATE_ATTRIBUTE(TOptional<float>, TargetFraction)
        SLATE_ATTRIBUTE(FLinearColor, TargetColor)

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
    TAttribute<TOptional<float>> _TargetFraction;
    TAttribute<FLinearColor> _TargetColor;
    FVector2D _DesiredSize = FVector2D(110.0f, 5.0f);
};

// ====================================================================================================================
