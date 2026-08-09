#pragma once

#include "Widgets/SLeafWidget.h"

#include <CoreMinimal.h>

// ====================================================================================================================
// Orientation cube: a unit cube seen through a fixed three-quarter camera, rotated by a Rotation
// attribute and stretched per-axis by a Scale attribute. Reads at a glance what three columns of
// Euler numbers do not — which way the entity is facing and how non-uniform its scale is.
//
// Contract:
//  - Rotation / Scale are live attributes; the widget is volatile and repaints every frame, so an
//    ECS-sourced lambda follows the entity without any invalidation plumbing from the owner.
//  - The three edges leaving the near-origin corner are the axis indicators (CkStyle::AxisX/Y/Z,
//    weighted). The other nine are neutral; back-facing edges are thinner and muted so the cube
//    reads as a solid even though nothing is filled.
//  - Fixed square footprint (Size). Nothing is allocated per paint: corners and depths are stack
//    arrays, one two-element point buffer is reused across all twelve edges, and no brushes are
//    involved (MakeLines only).
//
// Projection and normalization — both deliberately fixed so the cube never "breathes":
//  - View basis is a constant yaw/pitch offset (three faces visible). Corners are rotated, then
//    projected onto the screen right/up axes; the view-depth component only classifies front vs
//    back edges.
//  - Scale is taken component-wise absolute (a mirrored cube has the same silhouette), clamped to
//    [MinScale, MaxScale], then divided by max(1, largest component). So scale 1 fills the box
//    exactly, smaller scales genuinely render smaller, and larger scales are clamped with their
//    aspect ratio preserved — magnitude beyond 1 is the numeric row's job, not the cube's.
//  - The pixels-per-unit fit uses the worst-case half-diagonal (sqrt(3)/2) rather than the current
//    rotation's extent, so rotating an entity never pumps the cube's apparent size.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_OrientationCube : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_OrientationCube)
        : _Rotation(FQuat::Identity)
        , _Scale(FVector::OneVector)
        , _Size(64.0f)
    {}
        SLATE_ATTRIBUTE(FQuat, Rotation)
        SLATE_ATTRIBUTE(FVector, Scale)

        // Square footprint, in local Slate units.
        SLATE_ARGUMENT(float, Size)
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
    TAttribute<FQuat> _Rotation;
    TAttribute<FVector> _Scale;
    float _Size = 64.0f;
};

// ====================================================================================================================
