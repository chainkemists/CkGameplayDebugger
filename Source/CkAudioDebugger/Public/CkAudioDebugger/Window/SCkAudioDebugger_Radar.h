#pragma once

#include "CkAudioDebugger/Data/CkAudioDebugger_Types.h"

#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Top-down radar: the listener at the centre facing up, every live track plotted by bearing and distance, and the
 * selected track's inner and falloff radii drawn as rings.
 *
 * The reason this exists rather than two more numbers in a table: "is it inside the falloff" is instant off a picture
 * and is arithmetic off a pair of figures. A track sitting just outside its own dashed ring explains its own silence.
 *
 * The view struct is shared and mutated in place by the window on its refresh gate, so the widget follows live
 * without being rebuilt — the same contract `SCkDebug_Sparkline` uses for its sample ring.
 *
 * Deliberately NOT promoted to CkDebuggerCommon. A generic observer-relative radar would plausibly serve perception,
 * aggro and spatial-query too, but it has exactly one consumer today; promote it when a second one appears rather
 * than guessing now at what the shared shape should be.
 */
class SCkAudioDebugger_Radar : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAudioDebugger_Radar) {}
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
