#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// ====================================================================================================================

/** Stateless, screen-space selection visualization. The subsystem supplies one value snapshot;
 * this leaf retains no entity handles and never participates in input routing. */
class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_SelectionHud : public SLeafWidget
{
public:
    struct FMarker
    {
        FVector2D Position = FVector2D::ZeroVector;
        /** Relative screen-space selection ordinal (for example "0", "+1", "-1"). Entity names belong to focus cards. */
        FString RelativeLabel;
        bool Selected = false;
        bool Locked = false;
        bool OnScreen = true;
    };

    SLATE_BEGIN_ARGS(SCkDebugOverlay_SelectionHud) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto SetSnapshot(TArray<FMarker> InMarkers, TArray<FVector2D> InConePoints, float InDiamondScale) -> void;

    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override { return FVector2D::ZeroVector; }

private:
    TArray<FMarker> _Markers;
    TArray<FVector2D> _ConePoints;
    float _DiamondScale = 1.0f;
};

// ====================================================================================================================
