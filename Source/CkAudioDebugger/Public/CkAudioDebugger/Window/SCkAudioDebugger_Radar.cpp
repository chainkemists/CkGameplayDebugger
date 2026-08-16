#include "CkAudioDebugger/Window/SCkAudioDebugger_Radar.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_audio_debugger_radar
{
    constexpr auto k_Margin = 18.0f;
    constexpr auto k_CircleSegments = 72;
    constexpr auto k_DashSegments = 48;

    auto
        Build_Circle(
            const FVector2D& InCentre,
            float InRadius)
        -> TArray<FVector2D>
    {
        auto Points = TArray<FVector2D>{};
        Points.Reserve(k_CircleSegments + 1);

        for (auto Index = 0; Index <= k_CircleSegments; ++Index)
        {
            const auto Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(k_CircleSegments);

            Points.Emplace(
                InCentre.X + InRadius * FMath::Cos(Angle),
                InCentre.Y + InRadius * FMath::Sin(Angle));
        }

        return Points;
    }

    /** Slate has no dashed stroke, so a dashed ring is emitted as alternating short arcs. Dashed vs solid is the only
     *  thing separating the falloff ring from the inner one at a glance, so it is worth the extra draw calls. */
    auto
        Draw_DashedCircle(
            FSlateWindowElementList& OutDrawElements,
            int32 InLayerId,
            const FPaintGeometry& InGeometry,
            const FVector2D& InCentre,
            float InRadius,
            const FLinearColor& InColor)
        -> void
    {
        for (auto Index = 0; Index < k_DashSegments; Index += 2)
        {
            const auto StartAngle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(k_DashSegments);
            const auto EndAngle   = 2.0f * PI * static_cast<float>(Index + 1) / static_cast<float>(k_DashSegments);

            const auto Dash = TArray<FVector2D>{
                FVector2D{InCentre.X + InRadius * FMath::Cos(StartAngle),
                          InCentre.Y + InRadius * FMath::Sin(StartAngle)},
                FVector2D{InCentre.X + InRadius * FMath::Cos(EndAngle),
                          InCentre.Y + InRadius * FMath::Sin(EndAngle)}};

            FSlateDrawElement::MakeLines(
                OutDrawElements, InLayerId, InGeometry, Dash, ESlateDrawEffect::None, InColor, true, 1.0f);
        }
    }

    /** Screen-space position of a blip. Bearing is measured off the listener's forward, and the radar draws forward
     *  as UP — so a track the listener is facing appears above the centre, which is the only orientation a reader
     *  will guess correctly without a legend. */
    auto
        Project_Blip(
            const FVector2D& InCentre,
            float InPixelsPerCm,
            float InBearingDegrees,
            float InDistanceCm)
        -> FVector2D
    {
        const auto Radians = FMath::DegreesToRadians(InBearingDegrees);
        const auto Radius  = InDistanceCm * InPixelsPerCm;

        return FVector2D{
            InCentre.X + Radius * FMath::Sin(Radians),
            InCentre.Y - Radius * FMath::Cos(Radians)};
    }

    auto
        Get_BlipColor(
            const FCkAudioDebugger_SpatialBlip& InBlip)
        -> FLinearColor
    {
        if (InBlip.IsVirtualized)
        { return CkStyle::Err(); }

        if (InBlip.IsOutOfRange)
        { return CkStyle::Warn(); }

        return InBlip.IsAudible ? CkStyle::Ok() : CkStyle::TextMute();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_Radar::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _View = InArgs._View;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_Radar::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{240.0f, 240.0f};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkAudioDebugger_Radar::
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
    using namespace ck_audio_debugger_radar;

    if (NOT _View.IsValid() || NOT _View->HasSpatialData)
    { return InLayerId; }

    const auto Geometry = InAllottedGeometry.ToPaintGeometry();
    const auto Size     = InAllottedGeometry.GetLocalSize();

    // Square and centred, so the rings stay circles in a slot of any aspect.
    const auto Extent = FMath::Min(Size.X, Size.Y);
    const auto Centre = FVector2D{Size.X * 0.5, Size.Y * 0.5};
    const auto OuterRadius = Extent * 0.5f - k_Margin;

    if (OuterRadius <= 1.0f)
    { return InLayerId; }

    const auto Range = FMath::Max(_View->RadarRangeCm, KINDA_SMALL_NUMBER);
    const auto PixelsPerCm = OuterRadius / Range;

    auto Layer = InLayerId;

    // ---- Range boundary and crosshair ----
    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry, Build_Circle(Centre, OuterRadius),
        ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry,
        TArray<FVector2D>{{Centre.X, Centre.Y - OuterRadius}, {Centre.X, Centre.Y + OuterRadius}},
        ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry,
        TArray<FVector2D>{{Centre.X - OuterRadius, Centre.Y}, {Centre.X + OuterRadius, Centre.Y}},
        ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

    ++Layer;

    // ---- Attenuation rings ----
    if (const auto FalloffRadius = _View->MaxFalloffCm * PixelsPerCm;
        _View->IsAttenuated && FalloffRadius > 1.0f && FalloffRadius <= OuterRadius)
    {
        Draw_DashedCircle(OutDrawElements, Layer, Geometry, Centre, FalloffRadius, CkStyle::Warn());
    }

    if (const auto InnerRadius = _View->InnerRadiusCm * PixelsPerCm;
        _View->IsAttenuated && InnerRadius > 1.0f && InnerRadius <= OuterRadius)
    {
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer, Geometry, Build_Circle(Centre, InnerRadius),
            ESlateDrawEffect::None, CkStyle::Ok(), true, 1.0f);
    }

    ++Layer;

    // ---- Listener, drawn as an arrow so "facing up" is asserted rather than assumed ----
    const auto Arrow = TArray<FVector2D>{
        {Centre.X, Centre.Y - 14.0},
        {Centre.X + 8.0, Centre.Y + 6.0},
        {Centre.X, Centre.Y + 1.0},
        {Centre.X - 8.0, Centre.Y + 6.0},
        {Centre.X, Centre.Y - 14.0}};

    FSlateDrawElement::MakeLines(
        OutDrawElements, Layer, Geometry, Arrow, ESlateDrawEffect::None, CkStyle::Text(), true, 1.5f);

    ++Layer;

    // ---- Blips ----
    const auto Font = CkStyle::RegularFont(CkStyle::FontSizeMicro());
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    for (const auto& Blip : _View->Blips)
    {
        // Clamped to the rim rather than dropped: a track too far away to plot is the single most interesting thing
        // on this page, and culling it would hide exactly the case the reader came here for. The rim ring plus the
        // out-of-range colour say "at least this far", which is the honest claim.
        const auto ClampedDistance = FMath::Min(Blip.DistanceCm, Range);
        const auto Position = Project_Blip(Centre, PixelsPerCm, Blip.BearingDegrees, ClampedDistance);
        const auto Color = Get_BlipColor(Blip);

        if (Blip.IsSelected)
        {
            FSlateDrawElement::MakeLines(
                OutDrawElements, Layer, Geometry, TArray<FVector2D>{Centre, Position},
                ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.6f), true, 1.0f);

            FSlateDrawElement::MakeLines(
                OutDrawElements, Layer + 1, Geometry, Build_Circle(Position, 11.0f),
                ESlateDrawEffect::None, Color.CopyWithNewOpacity(0.45f), true, 1.0f);
        }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            Layer + 2,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{Blip.IsSelected ? 12.0f : 8.0f, Blip.IsSelected ? 12.0f : 8.0f},
                FSlateLayoutTransform{FVector2f{
                    static_cast<float>(Position.X) - (Blip.IsSelected ? 6.0f : 4.0f),
                    static_cast<float>(Position.Y) - (Blip.IsSelected ? 6.0f : 4.0f)}}),
            CkStyle::GetRoundedBrush_Pill(),
            ESlateDrawEffect::None,
            Color);

        if (NOT Blip.IsSelected)
        { continue; }

        const auto Label = FText::FromString(FString::Printf(
            TEXT("%.1f m"), Blip.DistanceCm / 100.0f));

        const auto LabelSize = FontMeasure->Measure(Label, Font);

        FSlateDrawElement::MakeText(
            OutDrawElements,
            Layer + 3,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{static_cast<float>(LabelSize.X), static_cast<float>(LabelSize.Y)},
                FSlateLayoutTransform{FVector2f{
                    static_cast<float>(Position.X - LabelSize.X * 0.5),
                    static_cast<float>(Position.Y) - 26.0f}}),
            Label,
            Font,
            ESlateDrawEffect::None,
            CkStyle::Text());
    }

    Layer += 4;

    // ---- Range label, so every radius on the picture has a scale ----
    const auto RangeLabel = FText::FromString(FString::Printf(TEXT("%.0f m"), Range / 100.0f));
    const auto RangeSize = FontMeasure->Measure(RangeLabel, Font);

    FSlateDrawElement::MakeText(
        OutDrawElements,
        Layer,
        InAllottedGeometry.ToPaintGeometry(
            FVector2f{static_cast<float>(RangeSize.X), static_cast<float>(RangeSize.Y)},
            FSlateLayoutTransform{FVector2f{
                static_cast<float>(Centre.X + OuterRadius - RangeSize.X),
                static_cast<float>(Centre.Y + OuterRadius - RangeSize.Y)}}),
        RangeLabel,
        Font,
        ESlateDrawEffect::None,
        CkStyle::TextMute());

    return Layer + 1;
}

// --------------------------------------------------------------------------------------------------------------------
