#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

// ====================================================================================================================

namespace ck::EcsSelectionGizmo
{
    struct FAxis
    {
        FVector2D Origin = FVector2D::ZeroVector;
        FVector2D End = FVector2D::ZeroVector;
        FLinearColor Color = FLinearColor::White;
        int32 SortOrder = 0;
    };

    struct FTriad
    {
        TArray<FAxis> Axes;
        FVector2D Origin = FVector2D::ZeroVector;
        float Opacity = 1.0f;
        bool IsSecondary = false;
    };

    /** Projects a transform while preserving its orientation and fixing the longest axis to InSizePixels. */
    CKECSDEBUGGER_API auto
    Project_Triad(
        const DebugViewportView::FProjection& InProjection,
        const FTransform& InTransform,
        float InSizePixels,
        bool InIsSecondary = false) -> TOptional<FTriad>;

    CKECSDEBUGGER_API auto Normalize_SizePixels(float InSizePixels) -> float;

    /** Converts render-target pixels into the local coordinates of the Slate viewport host. */
    CKECSDEBUGGER_API auto
    Convert_ToSlateLocal(
        const FTriad& InTriad,
        const FIntPoint& InProjectionSize,
        const FVector2D& InSlateLocalSize) -> TOptional<FTriad>;
}

// ====================================================================================================================

/** Screen-space, hit-test-invisible selection gizmo. It retains value snapshots only, never entity handles. */
class CKECSDEBUGGER_API SCkDebuggerSelectionGizmo : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerSelectionGizmo) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto SetSnapshot(TArray<ck::EcsSelectionGizmo::FTriad> InTriads) -> void;
    auto ClearSnapshot() -> void;

    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override
    { return FVector2D::ZeroVector; }

private:
    TArray<ck::EcsSelectionGizmo::FTriad> _Triads;
};

// ====================================================================================================================
