#include "SCkDebug_MeterBar.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"

// ====================================================================================================================

auto
    SCkDebug_MeterBar::
    Construct(const FArguments& InArgs)
    -> void
{
    _Fraction       = InArgs._Fraction;
    _FillColor      = InArgs._FillColor;
    _TrackColor     = InArgs._TrackColor;
    _TargetFraction = InArgs._TargetFraction;
    _TargetColor    = InArgs._TargetColor;
    _DesiredSize    = InArgs._DesiredSize;
}

auto
    SCkDebug_MeterBar::
    ComputeDesiredSize(float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return _DesiredSize;
}

auto
    SCkDebug_MeterBar::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const
    -> int32
{
    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto* Brush = CkStyle::GetRoundedBrush_Small();

    const auto TrackColor = _TrackColor.A > 0.0f ? _TrackColor : CkStyle::BgRoot();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId,
        InAllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform{}),
        Brush,
        ESlateDrawEffect::None,
        TrackColor);

    const auto Fraction = FMath::Clamp(_Fraction.Get(0.0f), 0.0f, 1.0f);
    if (Fraction > KINDA_SMALL_NUMBER)
    {
        const auto FillWidth = FMath::Max(Size.Y, Size.X * Fraction);   // never thinner than the rounding
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            InLayerId + 1,
            InAllottedGeometry.ToPaintGeometry(FVector2f(FillWidth, Size.Y), FSlateLayoutTransform{}),
            Brush,
            ESlateDrawEffect::None,
            _FillColor.Get(FLinearColor::White));
    }

    if (const auto Target = _TargetFraction.Get(TOptional<float>{});
        Target.IsSet())
    {
        // Square, not the rounded brush: a rounded 2px rule renders as a smudge at this height, and the marker has to
        // read as a hard "here" against a fill whose leading edge is itself rounded.
        constexpr auto MarkerWidth = 2.0f;

        const auto Clamped  = FMath::Clamp(Target.GetValue(), 0.0f, 1.0f);
        const auto MarkerX  = FMath::Clamp(Size.X * Clamped - MarkerWidth * 0.5f, 0.0f, Size.X - MarkerWidth);

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            InLayerId + 2,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(MarkerWidth, Size.Y),
                FSlateLayoutTransform{FVector2f(MarkerX, 0.0f)}),
            CkStyle::GetFilledBrush(),
            ESlateDrawEffect::None,
            _TargetColor.Get(FLinearColor::White));
    }

    return InLayerId + 3;
}

// ====================================================================================================================
