#pragma once

#include "Widgets/SLeafWidget.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Minimal sparkline (Overview dashboard, spec §3.6): paints the shared sample ring as a
// polyline, auto-scaled to its min/max. The widget is volatile — the owner mutates the
// ring in place at 1 Hz and the line follows without invalidation plumbing.
// --------------------------------------------------------------------------------------------------------------------

class CKECSDEBUGGER_API SCkEcsDebugger_Sparkline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkEcsDebugger_Sparkline)
        : _Color(FLinearColor::White)
        , _DesiredSize(FVector2D(120.0f, 26.0f))
    {}
        SLATE_ARGUMENT(TSharedPtr<TArray<float>>, Samples)
        SLATE_ARGUMENT(FLinearColor, Color)
        SLATE_ARGUMENT(FVector2D, DesiredSize)
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
    TSharedPtr<TArray<float>> Samples;
    FLinearColor Color = FLinearColor::White;
    FVector2D DesiredSize = FVector2D(120.0f, 26.0f);
};
