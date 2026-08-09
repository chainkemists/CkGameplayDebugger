#include "SCkDebug_OrientationCube.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_orientation_cube
{
    // Fixed three-quarter camera: yaw around Z, then pitch above the horizon. Chosen so the three
    // faces meeting the near-origin corner all read at identity rotation.
    constexpr auto ViewYawDegrees   = 35.0f;
    constexpr auto ViewPitchDegrees = 25.0f;

    // A zero-scaled axis still draws a visible sliver instead of collapsing the cube into a quad;
    // anything past MaxScale is indistinguishable once normalized anyway.
    constexpr auto MinScale = 0.05;
    constexpr auto MaxScale = 8.0;

    // Inset so the weighted axis edges never touch the row's neighbours.
    constexpr auto EdgePadding = 3.0;

    constexpr auto Thickness_Axis_Front    = 2.0f;
    constexpr auto Thickness_Axis_Back     = 1.25f;
    constexpr auto Thickness_Neutral_Front = 1.25f;
    constexpr auto Thickness_Neutral_Back  = 0.75f;

    constexpr auto Opacity_Axis_Back    = 0.45f;
    constexpr auto Opacity_Neutral_Back = 0.35f;

    // Corner index bits: 1 = +X, 2 = +Y, 4 = +Z. Corner 0 is the near-origin corner, so the first
    // three edges are the ones leaving it along +X / +Y / +Z — the axis indicators.
    struct FEdge
    {
        int32 A;
        int32 B;
        int32 Axis;   // 0/1/2 for the three origin edges, INDEX_NONE for the other nine
    };

    constexpr FEdge Edges[] =
    {
        {0, 1, 0}, {0, 2, 1}, {0, 4, 2},
        {1, 3, INDEX_NONE}, {1, 5, INDEX_NONE},
        {2, 3, INDEX_NONE}, {2, 6, INDEX_NONE},
        {3, 7, INDEX_NONE},
        {4, 5, INDEX_NONE}, {4, 6, INDEX_NONE},
        {5, 7, INDEX_NONE},
        {6, 7, INDEX_NONE},
    };

    static auto Get_AxisColor(
        int32 InAxis)
        -> FLinearColor
    {
        switch (InAxis)
        {
            case 0:  return CkStyle::AxisX();
            case 1:  return CkStyle::AxisY();
            default: return CkStyle::AxisZ();
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_OrientationCube::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Rotation = InArgs._Rotation;
    _Scale    = InArgs._Scale;
    _Size     = InArgs._Size;

    // The attributes read live ECS state that mutates behind Slate's back — repaint every frame.
    ForceVolatile(true);
}

auto
    SCkDebug_OrientationCube::
    ComputeDesiredSize(
        float)
    const
    -> FVector2D
{
    return FVector2D{_Size, _Size};
}

auto
    SCkDebug_OrientationCube::
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
    const auto LocalSize = InAllottedGeometry.GetLocalSize();
    const auto BoxExtent = FMath::Min(LocalSize.X, LocalSize.Y);

    if (BoxExtent <= KINDA_SMALL_NUMBER)
    { return InLayerId; }

    const auto Rotation = _Rotation.Get(FQuat::Identity);
    const auto RawScale = _Scale.Get(FVector::OneVector);

    // Absolute (a mirrored cube has the same silhouette), clamped, then divided by the largest
    // component only when it exceeds 1 — scale 1 fills the box, smaller scales stay smaller,
    // larger scales clamp with their aspect ratio intact.
    auto NormalizedScale = FVector
    {
        FMath::Clamp(FMath::Abs(RawScale.X), ck_debug_orientation_cube::MinScale, ck_debug_orientation_cube::MaxScale),
        FMath::Clamp(FMath::Abs(RawScale.Y), ck_debug_orientation_cube::MinScale, ck_debug_orientation_cube::MaxScale),
        FMath::Clamp(FMath::Abs(RawScale.Z), ck_debug_orientation_cube::MinScale, ck_debug_orientation_cube::MaxScale)
    };
    NormalizedScale /= FMath::Max(1.0, NormalizedScale.GetMax());

    // Worst-case half-diagonal of the unit cube — a rotation-independent fit, so spinning an
    // entity never pumps the cube's apparent size.
    const auto WorstCaseHalfDiagonal = 0.5 * FMath::Sqrt(3.0);
    const auto PixelsPerUnit = FMath::Max(
        1.0,
        (BoxExtent * 0.5 - ck_debug_orientation_cube::EdgePadding) / WorstCaseHalfDiagonal);

    const auto YawRadians   = FMath::DegreesToRadians(static_cast<double>(ck_debug_orientation_cube::ViewYawDegrees));
    const auto PitchRadians = FMath::DegreesToRadians(static_cast<double>(ck_debug_orientation_cube::ViewPitchDegrees));

    const auto ScreenRight = FVector{-FMath::Sin(YawRadians), FMath::Cos(YawRadians), 0.0};
    const auto ViewDirection = FVector
    {
        FMath::Cos(YawRadians) * FMath::Cos(PitchRadians),
        FMath::Sin(YawRadians) * FMath::Cos(PitchRadians),
        FMath::Sin(PitchRadians)
    };
    const auto ScreenUp = FVector::CrossProduct(ViewDirection, ScreenRight);

    const auto Center = FVector2D{LocalSize.X * 0.5, LocalSize.Y * 0.5};

    // Stack arrays — nothing heap-allocated per corner.
    FVector2D Projected[8];
    double Depth[8];

    for (auto Index = 0; Index < 8; ++Index)
    {
        const auto Corner = FVector
        {
            ((Index & 1) != 0 ? 0.5 : -0.5) * NormalizedScale.X,
            ((Index & 2) != 0 ? 0.5 : -0.5) * NormalizedScale.Y,
            ((Index & 4) != 0 ? 0.5 : -0.5) * NormalizedScale.Z
        };

        const auto Rotated = Rotation.RotateVector(Corner);

        Projected[Index] = Center + FVector2D
        {
            FVector::DotProduct(Rotated, ScreenRight),
            -FVector::DotProduct(Rotated, ScreenUp)
        } * PixelsPerUnit;

        // The cube is centred on the origin, so a corner's sign IS its front/back classification.
        Depth[Index] = FVector::DotProduct(Rotated, ViewDirection);
    }

    const auto PaintGeometry = InAllottedGeometry.ToPaintGeometry(FVector2f{LocalSize}, FSlateLayoutTransform{});

    // One two-element buffer reused by all twelve edges.
    auto Points = TArray<FVector2D>{FVector2D::ZeroVector, FVector2D::ZeroVector};

    const auto DoDrawEdge = [&](const ck_debug_orientation_cube::FEdge& InEdge, int32 InEdgeLayer) -> void
    {
        Points[0] = Projected[InEdge.A];
        Points[1] = Projected[InEdge.B];

        const auto IsBackFacing = ((Depth[InEdge.A] + Depth[InEdge.B]) * 0.5) < 0.0;
        const auto IsAxisEdge = InEdge.Axis != INDEX_NONE;

        const auto Color = [&]() -> FLinearColor
        {
            if (IsAxisEdge)
            {
                const auto AxisColor = ck_debug_orientation_cube::Get_AxisColor(InEdge.Axis);
                return IsBackFacing
                    ? AxisColor.CopyWithNewOpacity(ck_debug_orientation_cube::Opacity_Axis_Back)
                    : AxisColor;
            }

            return IsBackFacing
                ? CkStyle::TextMute().CopyWithNewOpacity(ck_debug_orientation_cube::Opacity_Neutral_Back)
                : CkStyle::BorderStrong();
        }();

        const auto Thickness = IsAxisEdge
            ? (IsBackFacing ? ck_debug_orientation_cube::Thickness_Axis_Back : ck_debug_orientation_cube::Thickness_Axis_Front)
            : (IsBackFacing ? ck_debug_orientation_cube::Thickness_Neutral_Back : ck_debug_orientation_cube::Thickness_Neutral_Front);

        constexpr auto Antialias = true;
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            InEdgeLayer,
            PaintGeometry,
            Points,
            ESlateDrawEffect::None,
            Color,
            Antialias,
            Thickness);
    };

    // Back neutral edges, then front neutral edges, then the axis triad on top — the axis edges
    // are the reason the widget exists, so they are never occluded by a neutral edge.
    for (const auto& Edge : ck_debug_orientation_cube::Edges)
    {
        if (Edge.Axis != INDEX_NONE || (Depth[Edge.A] + Depth[Edge.B]) * 0.5 >= 0.0)
        { continue; }

        DoDrawEdge(Edge, InLayerId);
    }

    for (const auto& Edge : ck_debug_orientation_cube::Edges)
    {
        if (Edge.Axis != INDEX_NONE || (Depth[Edge.A] + Depth[Edge.B]) * 0.5 < 0.0)
        { continue; }

        DoDrawEdge(Edge, InLayerId + 1);
    }

    for (const auto& Edge : ck_debug_orientation_cube::Edges)
    {
        if (Edge.Axis == INDEX_NONE)
        { continue; }

        DoDrawEdge(Edge, InLayerId + 2);
    }

    return InLayerId + 3;
}

// --------------------------------------------------------------------------------------------------------------------
