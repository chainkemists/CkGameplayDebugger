#include "CkAudioDebugger/Window/SCkAudioDebugger_FalloffCurve.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_audio_debugger_falloff
{
    constexpr auto k_PadLeft   = 28.0f;
    constexpr auto k_PadRight  = 8.0f;
    constexpr auto k_PadTop    = 8.0f;
    constexpr auto k_PadBottom = 20.0f;

    auto
        Draw_DashedVertical(
            FSlateWindowElementList& OutDrawElements,
            int32 InLayerId,
            const FPaintGeometry& InGeometry,
            float InX,
            float InTop,
            float InBottom,
            const FLinearColor& InColor)
        -> void
    {
        constexpr auto DashLength = 4.0f;

        for (auto Y = InTop; Y < InBottom; Y += DashLength * 2.0f)
        {
            FSlateDrawElement::MakeLines(
                OutDrawElements, InLayerId, InGeometry,
                TArray<FVector2D>{{InX, Y}, {InX, FMath::Min(Y + DashLength, InBottom)}},
                ESlateDrawEffect::None, InColor, true, 1.0f);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_FalloffCurve::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _View = InArgs._View;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_FalloffCurve::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{330.0f, 110.0f};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_FalloffCurve::
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
    using namespace ck_audio_debugger_falloff;

    if (NOT _View.IsValid() || NOT _View->HasSpatialData || _View->FalloffCurve.Num() < 2)
    { return InLayerId; }

    const auto Geometry = InAllottedGeometry.ToPaintGeometry();
    const auto Size     = InAllottedGeometry.GetLocalSize();

    const auto PlotLeft   = k_PadLeft;
    const auto PlotRight  = Size.X - k_PadRight;
    const auto PlotTop    = k_PadTop;
    const auto PlotBottom = Size.Y - k_PadBottom;

    const auto PlotWidth  = PlotRight - PlotLeft;
    const auto PlotHeight = PlotBottom - PlotTop;

    if (PlotWidth <= 1.0f || PlotHeight <= 1.0f)
    { return InLayerId; }

    // The curve's own domain, which is what the collector swept — NOT the radar range. Reusing the radar's scale here
    // would silently stretch the curve against an axis it was never sampled over.
    const auto CurveRange = FMath::Max(_View->MaxFalloffCm * 1.15f, KINDA_SMALL_NUMBER);

    const auto ToX = [&](float InDistanceCm) -> float
    {
        return PlotLeft + PlotWidth * FMath::Clamp(InDistanceCm / CurveRange, 0.0f, 1.0f);
    };

    const auto ToY = [&](float InGain) -> float
    {
        return PlotBottom - PlotHeight * FMath::Clamp(InGain, 0.0f, 1.0f);
    };

    auto Layer = InLayerId;

    // ---- Axes ----
    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry,
        TArray<FVector2D>{{PlotLeft, PlotBottom}, {PlotRight, PlotBottom}},
        ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry,
        TArray<FVector2D>{{PlotLeft, PlotTop}, {PlotLeft, PlotBottom}},
        ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

    ++Layer;

    // ---- The curve ----
    auto CurvePoints = TArray<FVector2D>{};
    CurvePoints.Reserve(_View->FalloffCurve.Num());

    for (auto Index = 0; Index < _View->FalloffCurve.Num(); ++Index)
    {
        const auto Alpha = static_cast<float>(Index) / static_cast<float>(_View->FalloffCurve.Num() - 1);

        CurvePoints.Emplace(
            PlotLeft + PlotWidth * Alpha,
            ToY(_View->FalloffCurve[Index]));
    }

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry, CurvePoints,
        ESlateDrawEffect::None, CkStyle::Accent(), true, 2.0f);

    ++Layer;

    // ---- Inner radius and falloff markers, matching the radar's ring styling exactly ----
    if (_View->InnerRadiusCm > 0.0f && _View->InnerRadiusCm < CurveRange)
    {
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer, Geometry,
            TArray<FVector2D>{{ToX(_View->InnerRadiusCm), PlotTop}, {ToX(_View->InnerRadiusCm), PlotBottom}},
            ESlateDrawEffect::None, CkStyle::Ok(), true, 1.0f);
    }

    if (_View->MaxFalloffCm > 0.0f && _View->MaxFalloffCm < CurveRange)
    {
        Draw_DashedVertical(
            OutDrawElements, Layer, Geometry,
            ToX(_View->MaxFalloffCm), PlotTop, PlotBottom, CkStyle::Warn());
    }

    ++Layer;

    // ---- Where the listener actually is ----
    const auto Font = CkStyle::RegularFont(CkStyle::FontSizeMicro());
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto CurrentX = ToX(_View->DistanceCm);
    const auto CurrentY = ToY(_View->AttenuationGain);

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry,
        TArray<FVector2D>{{CurrentX, PlotTop}, {CurrentX, PlotBottom}},
        ESlateDrawEffect::None, CkStyle::Text(), true, 1.5f);

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        Layer + 1,
        InAllottedGeometry.ToPaintGeometry(
            FVector2f{8.0f, 8.0f},
            FSlateLayoutTransform{FVector2f{
                static_cast<float>(CurrentX) - 4.0f, static_cast<float>(CurrentY) - 4.0f}}),
        CkStyle::GetRoundedBrush_Pill(),
        ESlateDrawEffect::None,
        CkStyle::Text());

    Layer += 2;

    // ---- Labels ----
    const auto DrawLabel = [&](const FText& InText, float InX, float InY, const FLinearColor& InColor, bool InCentre)
    {
        const auto Measured = FontMeasure->Measure(InText, Font);

        FSlateDrawElement::MakeText(
            OutDrawElements,
            Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{static_cast<float>(Measured.X), static_cast<float>(Measured.Y)},
                FSlateLayoutTransform{FVector2f{
                    InCentre ? InX - static_cast<float>(Measured.X) * 0.5f : InX, InY}}),
            InText,
            Font,
            ESlateDrawEffect::None,
            InColor);
    };

    DrawLabel(FText::FromString(TEXT("1.0")), 2.0f, PlotTop - 2.0f, CkStyle::TextMute(), false);
    DrawLabel(FText::FromString(TEXT("0")), 2.0f, PlotBottom - 10.0f, CkStyle::TextMute(), false);

    DrawLabel(
        FText::FromString(FString::Printf(TEXT("%.1f m"), _View->DistanceCm / 100.0f)),
        CurrentX, PlotBottom + 4.0f, CkStyle::Text(), true);

    if (_View->MaxFalloffCm > 0.0f && _View->MaxFalloffCm < CurveRange)
    {
        DrawLabel(
            FText::FromString(FString::Printf(TEXT("%.0f"), _View->MaxFalloffCm / 100.0f)),
            ToX(_View->MaxFalloffCm), PlotBottom + 4.0f, CkStyle::TextMute(), true);
    }

    return Layer + 1;
}

// --------------------------------------------------------------------------------------------------------------------
