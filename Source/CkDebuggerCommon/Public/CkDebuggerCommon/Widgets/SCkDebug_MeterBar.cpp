#include "SCkDebug_MeterBar.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"

// ====================================================================================================================

auto
    SCkDebug_MeterBar::
    Construct(const FArguments& InArgs)
    -> void
{
    _Fraction    = InArgs._Fraction;
    _FillColor   = InArgs._FillColor;
    _TrackColor  = InArgs._TrackColor;
    _DesiredSize = InArgs._DesiredSize;
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

    return InLayerId + 2;
}

// ====================================================================================================================
