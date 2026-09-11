#include "SCkDebuggerSelectionGizmo.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// ====================================================================================================================

namespace ck::EcsSelectionGizmo
{
namespace
{
    constexpr float MinSizePixels = 24.0f;
    constexpr float MaxSizePixels = 192.0f;
    constexpr float ProjectionSampleUnits = 100.0f;
    constexpr float SecondaryScale = 0.64f;

    auto Is_Finite(const FVector2D& InValue) -> bool
    { return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y); }
}

auto Normalize_SizePixels(const float InSizePixels) -> float
{
    return FMath::Clamp(FMath::IsFinite(InSizePixels) ? InSizePixels : 72.0f, MinSizePixels, MaxSizePixels);
}

auto Convert_ToSlateLocal(
    const FTriad& InTriad,
    const FIntPoint& InProjectionSize,
    const FVector2D& InSlateLocalSize) -> TOptional<FTriad>
{
    const auto ValidSizes = InProjectionSize.X > 0 && InProjectionSize.Y > 0 &&
        Is_Finite(InSlateLocalSize) && InSlateLocalSize.X > 0.0f && InSlateLocalSize.Y > 0.0f;
    if (NOT ValidSizes)
    { return {}; }

    const auto PixelToLocal = FVector2D{
        InSlateLocalSize.X / static_cast<double>(InProjectionSize.X),
        InSlateLocalSize.Y / static_cast<double>(InProjectionSize.Y)};
    auto Result = InTriad;
    Result.Origin *= PixelToLocal;
    for (auto& Axis : Result.Axes)
    {
        Axis.Origin *= PixelToLocal;
        Axis.End *= PixelToLocal;
    }
    return Result;
}

auto Project_Triad(
    const DebugViewportView::FProjection& InProjection,
    const FTransform& InTransform,
    const float InSizePixels,
    const bool InIsSecondary) -> TOptional<FTriad>
{
    if (InTransform.ContainsNaN())
    { return {}; }

    const auto OriginWorld = InTransform.GetLocation();
    const auto Clip = InProjection.ViewProjection.TransformFVector4(FVector4{OriginWorld, 1.0f});
    if (NOT FMath::IsFinite(Clip.W) || Clip.W <= KINDA_SMALL_NUMBER)
    { return {}; }

    auto Origin = FVector2D::ZeroVector;
    auto OriginInside = false;
    if (NOT InProjection.Project(OriginWorld, Origin, OriginInside) || NOT OriginInside)
    { return {}; }

    const auto Rotation = InTransform.GetRotation();
    const FVector Directions[] = {
        Rotation.GetAxisX(),
        Rotation.GetAxisY(),
        Rotation.GetAxisZ(),
    };
    const FLinearColor Colors[] = {
        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FF4A57"))),
        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("43D17A"))),
        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4E7DFF"))),
    };

    auto ProjectedDeltas = TArray<FVector2D>{};
    ProjectedDeltas.Reserve(UE_ARRAY_COUNT(Directions));
    auto LongestDelta = 0.0f;
    for (const auto& Direction : Directions)
    {
        auto Endpoint = FVector2D::ZeroVector;
        auto EndpointInside = false;
        if (NOT InProjection.Project(OriginWorld + Direction * ProjectionSampleUnits, Endpoint, EndpointInside) ||
            NOT Is_Finite(Endpoint))
        { return {}; }

        const auto Delta = Endpoint - Origin;
        ProjectedDeltas.Add(Delta);
        LongestDelta = FMath::Max(LongestDelta, static_cast<float>(Delta.Size()));
    }

    if (LongestDelta <= KINDA_SMALL_NUMBER)
    { return {}; }

    const auto RequestedSize = Normalize_SizePixels(InSizePixels) * (InIsSecondary ? SecondaryScale : 1.0f);
    const auto PixelScale = RequestedSize / LongestDelta;
    auto Result = FTriad{};
    Result.Origin = Origin;
    Result.Opacity = InIsSecondary ? 0.46f : 1.0f;
    Result.IsSecondary = InIsSecondary;
    Result.Axes.Reserve(UE_ARRAY_COUNT(Directions));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
    {
        Result.Axes.Add(FAxis{Origin, Origin + ProjectedDeltas[Index] * PixelScale, Colors[Index], Index});
    }
    return Result;
}
}

// ====================================================================================================================

namespace ck_ecs_selection_gizmo_widget
{
    auto Add_Quad(
        TArray<FSlateVertex>& OutVertices,
        TArray<SlateIndex>& OutIndices,
        const FSlateRenderTransform& InTransform,
        const FVector2D& InA,
        const FVector2D& InB,
        const FVector2D& InC,
        const FVector2D& InD,
        const FLinearColor& InColor) -> void
    {
        const auto First = static_cast<SlateIndex>(OutVertices.Num());
        const auto Color = InColor.ToFColor(true);
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InA}, FVector2f::ZeroVector, Color));
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InB}, FVector2f::ZeroVector, Color));
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InC}, FVector2f::ZeroVector, Color));
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InD}, FVector2f::ZeroVector, Color));
        OutIndices.Append({First, static_cast<SlateIndex>(First + 1), static_cast<SlateIndex>(First + 2),
                           First, static_cast<SlateIndex>(First + 2), static_cast<SlateIndex>(First + 3)});
    }

    auto Add_Triangle(
        TArray<FSlateVertex>& OutVertices,
        TArray<SlateIndex>& OutIndices,
        const FSlateRenderTransform& InTransform,
        const FVector2D& InA,
        const FVector2D& InB,
        const FVector2D& InC,
        const FLinearColor& InColor) -> void
    {
        const auto First = static_cast<SlateIndex>(OutVertices.Num());
        const auto Color = InColor.ToFColor(true);
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InA}, FVector2f::ZeroVector, Color));
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InB}, FVector2f::ZeroVector, Color));
        OutVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(InTransform, FVector2f{InC}, FVector2f::ZeroVector, Color));
        OutIndices.Append({First, static_cast<SlateIndex>(First + 1), static_cast<SlateIndex>(First + 2)});
    }

    auto Add_Arrow(
        TArray<FSlateVertex>& OutVertices,
        TArray<SlateIndex>& OutIndices,
        const FSlateRenderTransform& InTransform,
        const ck::EcsSelectionGizmo::FAxis& InAxis,
        const float InOpacity,
        const float InWidthScale,
        const bool InShadow) -> void
    {
        const auto Delta = InAxis.End - InAxis.Origin;
        const auto Length = static_cast<float>(Delta.Size());
        if (Length <= KINDA_SMALL_NUMBER)
        { return; }

        const auto Direction = Delta / Length;
        const auto Perpendicular = FVector2D{-Direction.Y, Direction.X};
        const auto ShaftHalfWidth = (InShadow ? 4.2f : 2.8f) * InWidthScale;
        const auto TipHalfWidth = (InShadow ? 9.0f : 7.0f) * InWidthScale;
        const auto TipLength = FMath::Min(20.0f * InWidthScale, Length * 0.42f);
        const auto ShaftEnd = InAxis.End - Direction * TipLength;
        const auto Color = InShadow
            ? CkStyle::OverlayOf(CkStyle::BgRoot(), 0.92f * InOpacity)
            : CkStyle::OverlayOf(InAxis.Color, 0.96f * InOpacity);

        Add_Quad(OutVertices, OutIndices, InTransform,
            InAxis.Origin + Perpendicular * ShaftHalfWidth,
            ShaftEnd + Perpendicular * ShaftHalfWidth,
            ShaftEnd - Perpendicular * ShaftHalfWidth,
            InAxis.Origin - Perpendicular * ShaftHalfWidth,
            Color);
        Add_Triangle(OutVertices, OutIndices, InTransform,
            InAxis.End,
            ShaftEnd + Perpendicular * TipHalfWidth,
            ShaftEnd - Perpendicular * TipHalfWidth,
            Color);
    }
}

// ====================================================================================================================

auto SCkDebuggerSelectionGizmo::Construct(const FArguments&) -> void
{ SetVisibility(EVisibility::HitTestInvisible); }

auto SCkDebuggerSelectionGizmo::SetSnapshot(TArray<ck::EcsSelectionGizmo::FTriad> InTriads) -> void
{
    _Triads = MoveTemp(InTriads);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkDebuggerSelectionGizmo::ClearSnapshot() -> void
{
    if (_Triads.IsEmpty())
    { return; }
    _Triads.Reset();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkDebuggerSelectionGizmo::OnPaint(
    const FPaintArgs&,
    const FGeometry& InAllottedGeometry,
    const FSlateRect&,
    FSlateWindowElementList& OutDrawElements,
    const int32 InLayerId,
    const FWidgetStyle&,
    bool) const -> int32
{
    auto Layer = InLayerId;
    const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*CkStyle::GetFilledBrush());
    const auto RenderTransform = InAllottedGeometry.GetAccumulatedRenderTransform();

    for (const auto& Triad : _Triads)
    {
        auto SortedAxes = Triad.Axes;
        SortedAxes.Sort([](const auto& InLeft, const auto& InRight)
        { return InLeft.SortOrder < InRight.SortOrder; });

        auto Vertices = TArray<FSlateVertex>{};
        auto Indices = TArray<SlateIndex>{};
        for (const auto& Axis : SortedAxes)
        {
            ck_ecs_selection_gizmo_widget::Add_Arrow(
                Vertices, Indices, RenderTransform, Axis, Triad.Opacity, Triad.IsSecondary ? 0.78f : 1.0f, true);
        }
        for (const auto& Axis : SortedAxes)
        {
            ck_ecs_selection_gizmo_widget::Add_Arrow(
                Vertices, Indices, RenderTransform, Axis, Triad.Opacity, Triad.IsSecondary ? 0.78f : 1.0f, false);
        }
        FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++Layer, Resource, Vertices, Indices, nullptr, 0, 0);

        const auto HubRadius = Triad.IsSecondary ? 4.0f : 6.0f;
        const auto HubSide = HubRadius * 1.41421356f;
        const auto HubOrigin = Triad.Origin - FVector2D{HubSide * 0.5f, HubSide * 0.5f};
        FSlateDrawElement::MakeRotatedBox(OutDrawElements, ++Layer,
            InAllottedGeometry.ToPaintGeometry(FVector2f{HubSide, HubSide}, FSlateLayoutTransform{FVector2f{HubOrigin}}),
            CkStyle::GetFilledBrush(), ESlateDrawEffect::None, PI * 0.25f, TOptional<FVector2D>{},
            FSlateDrawElement::RelativeToElement,
            CkStyle::OverlayOf(Triad.IsSecondary ? CkStyle::TextMute() : CkStyle::TextStrong(), Triad.Opacity));
    }
    return Layer;
}

// ====================================================================================================================
