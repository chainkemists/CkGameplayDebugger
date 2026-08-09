#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The direction rosette of the key/state view: eight spokes, the recorded octant lit, and the conditioned axis pair
// drawn as a dot where the stick actually is.
//
// The lit wedge is the row's `_Octant` verbatim. It is NOT computed from the axis pair — the sampler's reading is
// hysteresis-damped against state that only exists between rows, so a dial that re-derived it would disagree with
// the matcher on exactly the frames a reader is trying to explain. Seeing the dot sit inside one wedge while a
// neighbour is lit IS the hysteresis, rendered.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_OctantDial : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_OctantDial)
        : _DesiredSize(132.0f)
    {}
        SLATE_ARGUMENT(float, DesiredSize)
        SLATE_ATTRIBUTE(ECk_Intent_Octant, Octant)
        SLATE_ATTRIBUTE(FVector2D, AxisValue)
        SLATE_ATTRIBUTE(float, NeutralRadius)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

protected:
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
    float _DesiredSize = 132.0f;
    TAttribute<ECk_Intent_Octant> _Octant;
    TAttribute<FVector2D> _AxisValue;
    TAttribute<float> _NeutralRadius;
};

// --------------------------------------------------------------------------------------------------------------------
