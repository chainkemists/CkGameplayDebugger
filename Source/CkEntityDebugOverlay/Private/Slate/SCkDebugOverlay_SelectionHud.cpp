#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionHud.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// ====================================================================================================================

namespace ck_debugoverlay_selection_hud
{
    constexpr float DiamondRadius = 6.0f;
    constexpr float SelectionHaloPadding = 4.5f;
    constexpr float SquareRootTwo = 1.41421356f;

    auto Make_Diamond(const FVector2D& InCenter, float InRadius) -> TArray<FVector2D>
    {
        return {
            FVector2D{ InCenter.X, InCenter.Y - InRadius },
            FVector2D{ InCenter.X + InRadius, InCenter.Y },
            FVector2D{ InCenter.X, InCenter.Y + InRadius },
            FVector2D{ InCenter.X - InRadius, InCenter.Y },
            FVector2D{ InCenter.X, InCenter.Y - InRadius },
        };
    }

    auto Make_Ring(const FVector2D& InCenter, float InRadius) -> TArray<FVector2D>
    {
        auto Result = TArray<FVector2D>{};
        constexpr int32 Segments = 12;
        Result.Reserve(Segments + 1);
        for (int32 Index = 0; Index <= Segments; ++Index)
        {
            const auto Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(Segments);
            Result.Add(InCenter + FVector2D{ FMath::Cos(Angle) * InRadius, FMath::Sin(Angle) * InRadius });
        }
        return Result;
    }

}

// ====================================================================================================================

auto SCkDebugOverlay_SelectionHud::Construct(const FArguments&) -> void
{ SetVisibility(EVisibility::HitTestInvisible); }

auto SCkDebugOverlay_SelectionHud::SetSnapshot(
    TArray<FMarker> InMarkers,
    TArray<FVector2D> InConePoints,
    float InDiamondScale) -> void
{
    _Markers = MoveTemp(InMarkers);
    _ConePoints = MoveTemp(InConePoints);
    _DiamondScale = FMath::Clamp(InDiamondScale, 0.1f, 5.0f);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkDebugOverlay_SelectionHud::OnPaint(
    const FPaintArgs&,
    const FGeometry& InAllottedGeometry,
    const FSlateRect&,
    FSlateWindowElementList& OutDrawElements,
    int32 InLayerId,
    const FWidgetStyle&,
    bool) const -> int32
{
    auto Layer = InLayerId;
    const auto ConeColor = CkStyle::OverlayOf(CkStyle::Accent(), 0.78f);
    // Main supplies a sampled cone curve. Painting alternating source segments, rather than
    // restarting a micro-dash pattern per segment, guarantees visible gaps even for dense arcs.
    for (int32 Index = 1; Index < _ConePoints.Num(); Index += 2)
    {
        FSlateDrawElement::MakeLines(OutDrawElements, Layer, InAllottedGeometry.ToPaintGeometry(),
            TArray<FVector2D>{ _ConePoints[Index - 1], _ConePoints[Index] }, ESlateDrawEffect::None, ConeColor, true, 1.0f);
    }

    auto VisibleIndices = TArray<int32>{};
    for (int32 Index = 0; Index < _Markers.Num(); ++Index)
    {
        if (_Markers[Index].OnScreen)
        { VisibleIndices.Add(Index); }
    }
    VisibleIndices.Sort([this](int32 InLeft, int32 InRight)
    {
        const auto& Left = _Markers[InLeft];
        const auto& Right = _Markers[InRight];
        if (Left.Position.X != Right.Position.X) { return Left.Position.X < Right.Position.X; }
        if (Left.Position.Y != Right.Position.Y) { return Left.Position.Y < Right.Position.Y; }
        if (Left.RelativeLabel != Right.RelativeLabel) { return Left.RelativeLabel < Right.RelativeLabel; }
        return InLeft < InRight;
    });

    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto BadgeFont = ck::debug_axes::ScaledFont("Bold", FMath::Clamp(FMath::RoundToInt(10.0f * _DiamondScale), 8, 17));
    const auto BadgeInset = FMath::Max(1.0f, _DiamondScale);
    const auto BadgePadding = FVector2D{4.0f * _DiamondScale, 2.0f * _DiamondScale};
    const auto BadgeSpacing = FMath::Max(2.0f, 2.0f * _DiamondScale);
    const auto Get_BadgeSize = [&FontMeasure, &BadgeFont, BadgeInset, BadgePadding](const FString& InText) -> FVector2D
    {
        return FontMeasure->Measure(InText, BadgeFont) + (BadgePadding + FVector2D{BadgeInset, BadgeInset}) * 2.0f;
    };
    for (int32 VisibleIndex = 0; VisibleIndex < VisibleIndices.Num(); ++VisibleIndex)
    {
        const auto& Marker = _Markers[VisibleIndices[VisibleIndex]];
        const auto Radius = ck_debugoverlay_selection_hud::DiamondRadius * _DiamondScale * (Marker.Selected ? 1.30f : 1.0f);
        const auto Accent = Marker.Locked ? CkStyle::Warn() : CkStyle::Accent();
        const auto Halo = Marker.Locked ? CkStyle::Warn() : CkStyle::TextStrong();
        const auto Outline = Marker.Selected ? Accent : CkStyle::OverlayOf(Accent, 0.80f);
        const auto DiamondSide = Radius * ck_debugoverlay_selection_hud::SquareRootTwo;
        const auto DiamondOrigin = Marker.Position - FVector2D{DiamondSide * 0.5f, DiamondSide * 0.5f};
        FSlateDrawElement::MakeRotatedBox(OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(FVector2f{DiamondSide, DiamondSide}, FSlateLayoutTransform{FVector2f{DiamondOrigin}}),
            CkStyle::GetFilledBrush(), ESlateDrawEffect::None, PI * 0.25f, TOptional<FVector2D>{},
            FSlateDrawElement::RelativeToElement, CkStyle::OverlayOf(CkStyle::BgRoot(), 0.96f));
        FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            ck_debugoverlay_selection_hud::Make_Diamond(Marker.Position, Radius), ESlateDrawEffect::None, Outline, true, 1.5f);
        const auto InsetRadius = Radius * 0.63f;
        const auto InsetDiamond = ck_debugoverlay_selection_hud::Make_Diamond(Marker.Position, InsetRadius);
        FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            InsetDiamond, ESlateDrawEffect::None, CkStyle::OverlayOf(Accent, 0.72f), true, 1.0f);
        FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            TArray<FVector2D>{InsetDiamond[0], Marker.Position, InsetDiamond[3]}, ESlateDrawEffect::None,
            CkStyle::OverlayOf(CkStyle::TextStrong(), 0.58f), true, 1.0f);
        FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            TArray<FVector2D>{InsetDiamond[1], Marker.Position, InsetDiamond[2]}, ESlateDrawEffect::None,
            CkStyle::OverlayOf(Accent, 0.52f), true, 1.0f);
        if (Marker.Selected)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
                ck_debugoverlay_selection_hud::Make_Ring(Marker.Position, Radius + ck_debugoverlay_selection_hud::SelectionHaloPadding), ESlateDrawEffect::None, Halo, true, 1.5f);
        }

        if (Marker.RelativeLabel.IsEmpty())
        { continue; }

        auto GroupStart = VisibleIndex;
        while (GroupStart > 0 && _Markers[VisibleIndices[GroupStart - 1]].Position == Marker.Position) { --GroupStart; }
        const auto TextSize = FontMeasure->Measure(Marker.RelativeLabel, BadgeFont);
        const auto BadgeSize = Get_BadgeSize(Marker.RelativeLabel);
        auto BadgeY = Marker.Position.Y;
        for (auto GroupIndex = GroupStart; GroupIndex < VisibleIndex; ++GroupIndex)
        {
            const auto& Previous = _Markers[VisibleIndices[GroupIndex]];
            if (!Previous.RelativeLabel.IsEmpty())
            { BadgeY += Get_BadgeSize(Previous.RelativeLabel).Y + BadgeSpacing; }
        }

        // Plates are bottom-anchored above the marker. Stack number badges only downward so
        // a co-located group never paints over a world plate. The X fallback remains outside the
        // selected halo at every DiamondScale; an unusually narrow viewport clips rather than overlaps.
        const auto ClearRadius = Radius + (Marker.Selected ? ck_debugoverlay_selection_hud::SelectionHaloPadding : 0.0f);
        const auto BadgeGap = FMath::Max(2.0f, 2.0f * _DiamondScale);
        const auto RightX = Marker.Position.X + ClearRadius + BadgeGap;
        const auto LeftX = Marker.Position.X - ClearRadius - BadgeGap - BadgeSize.X;
        const auto bFitsRight = RightX + BadgeSize.X <= Size.X;
        const auto bFitsLeft = LeftX >= 0.0f;
        const auto BadgeX = bFitsRight || !bFitsLeft ? RightX : LeftX;
        const auto BadgePosition = FVector2D{ BadgeX, BadgeY };
        FSlateDrawElement::MakeLines(OutDrawElements, ++Layer, InAllottedGeometry.ToPaintGeometry(),
            TArray<FVector2D>{ Marker.Position, BadgePosition }, ESlateDrawEffect::None, CkStyle::OverlayOf(Accent, 0.62f), true, 1.0f);
        FSlateDrawElement::MakeBox(OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(BadgeSize, FSlateLayoutTransform{FVector2f{BadgePosition}}),
            CkStyle::GetRoundedBrush_Small(), ESlateDrawEffect::None, CkStyle::OverlayOf(CkStyle::BgRoot(), 0.96f));
        const auto InnerPosition = BadgePosition + FVector2D{BadgeInset, BadgeInset};
        const auto InnerSize = BadgeSize - FVector2D{BadgeInset * 2.0f, BadgeInset * 2.0f};
        FSlateDrawElement::MakeBox(OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(InnerSize, FSlateLayoutTransform{FVector2f{InnerPosition}}),
            CkStyle::GetRoundedBrush_Small(), ESlateDrawEffect::None, CkStyle::OverlayOf(Accent, 0.92f));
        const auto TextPosition = InnerPosition + BadgePadding;
        FSlateDrawElement::MakeText(OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform{FVector2f{TextPosition}}),
            Marker.RelativeLabel, BadgeFont, ESlateDrawEffect::None, CkStyle::TextStrong());
    }
    return Layer;
}

// ====================================================================================================================
