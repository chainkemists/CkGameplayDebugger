#pragma once

#include "CkAudioDebugger/Data/CkAudioDebugger_Types.h"

#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * The selected track's attenuation curve with the current listener distance marked ON it.
 *
 * The point is that the gain is shown as a POSITION on a curve rather than asserted as a number: "0.42" says nothing
 * about whether the track is just past the knee or falling off a cliff, and the difference decides whether the fix is
 * to move the sound or to change the falloff.
 *
 * The curve is sampled by the collector through the engine's own `FBaseAttenuationSettings::Evaluate`, so the plotted
 * line and the marked point are the same function — this widget only projects, it never evaluates.
 */
class SCkAudioDebugger_FalloffCurve : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAudioDebugger_FalloffCurve) {}
        SLATE_ARGUMENT(TSharedPtr<FCkAudioDebugger_SpatialView>, View)
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    auto
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    auto
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const -> FVector2D override;

private:
    TSharedPtr<FCkAudioDebugger_SpatialView> _View;
};

// --------------------------------------------------------------------------------------------------------------------
