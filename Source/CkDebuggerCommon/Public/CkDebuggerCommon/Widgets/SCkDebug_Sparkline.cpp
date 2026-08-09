#include "SCkDebug_Sparkline.h"

#include "CkCore/Macros/CkMacros.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_sparkline
{
    // Fills the vertical strip between two point rows (Top above Bottom at each X) as a triangle
    // list, with per-vertex alpha so the fill can fade toward the baseline.
    static auto MakeFill(
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FGeometry& InGeometry,
        const TArray<FVector2D>& InTop,
        const TArray<FVector2D>& InBottom,
        const FLinearColor& InColor,
        float InTopAlpha,
        float InBottomAlpha) -> void
    {
        if (InTop.Num() < 2 || InTop.Num() != InBottom.Num())
        { return; }

        const auto* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
        const auto& Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
        const auto& Transform = InGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

        const auto TopColor    = (InColor * FLinearColor(1.0f, 1.0f, 1.0f, InTopAlpha)).ToFColor(true);
        const auto BottomColor = (InColor * FLinearColor(1.0f, 1.0f, 1.0f, InBottomAlpha)).ToFColor(true);

        auto Vertices = TArray<FSlateVertex>{};
        Vertices.Reserve(InTop.Num() * 2);
        for (auto Index = 0; Index < InTop.Num(); ++Index)
        {
            Vertices.Emplace(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                Transform, FVector2f(InTop[Index]), FVector2f(0.0f, 0.0f), TopColor));
            Vertices.Emplace(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                Transform, FVector2f(InBottom[Index]), FVector2f(0.0f, 0.0f), BottomColor));
        }

        auto Indices = TArray<SlateIndex>{};
        Indices.Reserve((InTop.Num() - 1) * 6);
        for (auto Index = 0; Index < InTop.Num() - 1; ++Index)
        {
            const auto Base = Index * 2;
            Indices.Append({
                static_cast<SlateIndex>(Base),     static_cast<SlateIndex>(Base + 1), static_cast<SlateIndex>(Base + 2),
                static_cast<SlateIndex>(Base + 2), static_cast<SlateIndex>(Base + 1), static_cast<SlateIndex>(Base + 3)});
        }

        FSlateDrawElement::MakeCustomVerts(
            OutDrawElements, InLayerId, Handle, Vertices, Indices, nullptr, 0, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_Sparkline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Samples = InArgs._Samples;
    _BandSamples = InArgs._BandSamples;
    _Color = InArgs._Color;
    _BandColor = InArgs._BandColor;
    _FillOpacity = InArgs._FillOpacity;
    _BandFillOpacity = InArgs._BandFillOpacity;
    _ShowEndDot = InArgs._ShowEndDot;
    _MarkerValue = InArgs._MarkerValue;
    _MarkerColor = InArgs._MarkerColor;
    _DesiredSize = InArgs._DesiredSize;

    // The sample ring mutates behind Slate's back — repaint every frame.
    ForceVolatile(true);
}

auto
    SCkDebug_Sparkline::
    ComputeDesiredSize(
        float)
    const
    -> FVector2D
{
    return _DesiredSize;
}

auto
    SCkDebug_Sparkline::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled)
    const
    -> int32
{
    if (NOT _Samples.IsValid() || _Samples->Num() < 2)
    { return InLayerId; }

    const auto& Values = *_Samples;
    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto Marker = _MarkerValue.Get(TOptional<float>{});
    const auto HasBand = _BandSamples.IsValid() && _BandSamples->Num() == Values.Num();

    // One resolve per paint — the line, the area fill and the end dot must never disagree.
    const auto LineColor = _Color.Get();

    auto MinValue = Values[0];
    auto MaxValue = Values[0];
    for (const auto& Value : Values)
    {
        MinValue = FMath::Min(MinValue, Value);
        MaxValue = FMath::Max(MaxValue, Value);
    }
    if (HasBand)
    {
        for (const auto& Value : *_BandSamples)
        {
            MinValue = FMath::Min(MinValue, Value);
            MaxValue = FMath::Max(MaxValue, Value);
        }
    }
    if (Marker.IsSet())
    {
        MinValue = FMath::Min(MinValue, *Marker);
        MaxValue = FMath::Max(MaxValue, *Marker);
    }
    const auto Range = FMath::Max(MaxValue - MinValue, KINDA_SMALL_NUMBER);

    const auto ToPoint = [&](int32 InIndex, float InValue) -> FVector2D
    {
        const auto X = (Values.Num() > 1 ? static_cast<float>(InIndex) / (Values.Num() - 1) : 0.0f) * Size.X;
        const auto Normalized = (InValue - MinValue) / Range;
        const auto Y = (1.0f - Normalized) * (Size.Y - 2.0f) + 1.0f;
        return FVector2D{X, Y};
    };

    auto Points = TArray<FVector2D>{};
    Points.Reserve(Values.Num());
    for (auto Index = 0; Index < Values.Num(); ++Index)
    { Points.Emplace(ToPoint(Index, Values[Index])); }

    auto Layer = InLayerId;

    // band fill (between primary and band series), painted below everything else
    if (HasBand && _BandFillOpacity > 0.0f)
    {
        auto BandPoints = TArray<FVector2D>{};
        BandPoints.Reserve(Values.Num());
        for (auto Index = 0; Index < Values.Num(); ++Index)
        { BandPoints.Emplace(ToPoint(Index, (*_BandSamples)[Index])); }

        ck_debug_sparkline::MakeFill(OutDrawElements, Layer, InAllottedGeometry,
            BandPoints, Points, _BandColor, _BandFillOpacity, _BandFillOpacity);

        constexpr auto Antialias = true;
        FSlateDrawElement::MakeLines(
            OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            BandPoints, ESlateDrawEffect::None, _BandColor.CopyWithNewOpacity(0.6f), Antialias, 1.0f);
    }

    // gradient area under the primary line
    if (_FillOpacity > 0.0f)
    {
        auto Baseline = TArray<FVector2D>{};
        Baseline.Reserve(Points.Num());
        for (const auto& Point : Points)
        { Baseline.Emplace(Point.X, Size.Y); }

        ck_debug_sparkline::MakeFill(OutDrawElements, ++Layer, InAllottedGeometry,
            Points, Baseline, LineColor, _FillOpacity, 0.0f);
    }

    constexpr auto Antialias = true;
    FSlateDrawElement::MakeLines(
        OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
        Points, ESlateDrawEffect::None, LineColor, Antialias, 1.5f);

    // dashed marker line (e.g. high-water mark)
    if (Marker.IsSet())
    {
        const auto MarkerY = ToPoint(0, *Marker).Y;
        constexpr auto DashLength = 4.0f;
        constexpr auto GapLength = 3.0f;
        ++Layer;
        for (auto X = 0.0f; X < Size.X; X += DashLength + GapLength)
        {
            const auto Dash = TArray<FVector2D>{
                FVector2D{X, MarkerY},
                FVector2D{FMath::Min(X + DashLength, static_cast<float>(Size.X)), MarkerY}};
            FSlateDrawElement::MakeLines(
                OutDrawElements, Layer, InAllottedGeometry.ToPaintGeometry(),
                Dash, ESlateDrawEffect::None, _MarkerColor, false, 1.0f);
        }
    }

    // emphasized endpoint
    if (_ShowEndDot)
    {
        const auto& End = Points.Last();
        constexpr auto DotRadius = 2.0f;
        FSlateDrawElement::MakeBox(
            OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(DotRadius * 2.0f, DotRadius * 2.0f),
                FSlateLayoutTransform(FVector2f(End - FVector2D(DotRadius, DotRadius)))),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            LineColor);
    }

    return Layer + 1;
}

// --------------------------------------------------------------------------------------------------------------------
