#pragma once

#include "Widgets/SLeafWidget.h"

#include <CoreMinimal.h>

// ====================================================================================================================
// Shared sparkline: paints a sample ring as a polyline with an optional vertical-gradient area fill,
// an optional second "band" series (filled between the primary line and the band line — e.g. in-use
// vs live), an optional dashed horizontal marker (e.g. a high-water mark), and an emphasized
// endpoint dot.
//
// The widget is volatile — the owner mutates the ring(s) in place on its gated refresh and the
// line follows without invalidation plumbing. Rings are TSharedPtr so many widgets can share one
// series (Dashboard pattern).
//
// Promoted from SCkEcsDebugger_Sparkline / SCkSchedulerDebugger_Sparkline; migrating those two
// consumers onto this widget is a recorded follow-up, not done in passing.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_Sparkline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Sparkline)
        : _Color(FLinearColor::White)
        , _BandColor(FLinearColor::White)
        , _FillOpacity(0.0f)
        , _BandFillOpacity(0.18f)
        , _ShowEndDot(true)
        , _MarkerValue(TOptional<float>{})
        , _MarkerColor(FLinearColor::Yellow)
        , _DesiredSize(FVector2D(120.0f, 26.0f))
    {}
        SLATE_ARGUMENT(TSharedPtr<TArray<float>>, Samples)

        // Optional second series (same sample cadence/length as Samples). The area between the
        // primary line and this line is filled with BandColor at BandFillOpacity.
        SLATE_ARGUMENT(TSharedPtr<TArray<float>>, BandSamples)

        // Attribute-bound so a tone that depends on live data (over-budget, saturated) re-colors
        // the line without rebuilding the row.
        SLATE_ATTRIBUTE(FLinearColor, Color)
        SLATE_ARGUMENT(FLinearColor, BandColor)

        // 0 = line only; > 0 = gradient area fill under the primary line (this alpha at the line,
        // fading to 0 at the baseline)
        SLATE_ARGUMENT(float, FillOpacity)
        SLATE_ARGUMENT(float, BandFillOpacity)

        SLATE_ARGUMENT(bool, ShowEndDot)

        // Dashed horizontal reference line at this value (auto-included in the vertical scale)
        SLATE_ATTRIBUTE(TOptional<float>, MarkerValue)
        SLATE_ARGUMENT(FLinearColor, MarkerColor)

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
    TSharedPtr<TArray<float>> _Samples;
    TSharedPtr<TArray<float>> _BandSamples;

    TAttribute<FLinearColor> _Color = FLinearColor::White;
    FLinearColor _BandColor = FLinearColor::White;
    float _FillOpacity = 0.0f;
    float _BandFillOpacity = 0.18f;
    bool _ShowEndDot = true;

    TAttribute<TOptional<float>> _MarkerValue;
    FLinearColor _MarkerColor = FLinearColor::Yellow;

    FVector2D _DesiredSize = FVector2D(120.0f, 26.0f);
};

// ====================================================================================================================
