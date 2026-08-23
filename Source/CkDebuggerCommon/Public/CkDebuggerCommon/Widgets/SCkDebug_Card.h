#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Rounded panel card with an optional left accent stripe and an optional soft
// glow — the mockup's decision cards, plan-step cards, and panel surfaces.
//
//   StripeColor  (attr)  left 3px semantic stripe; transparent = none.
//                        (in-plan accent, blocked red, fallback amber)
//   GlowColor    (attr)  soft outer halo; transparent = none.
//   Selected     (attr)  accent border treatment.
//
// The card body is caller content; compose rows/chips/steppers inside.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_Card : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Card)
        : _StripeColor(FLinearColor::Transparent)
        , _GlowColor(FLinearColor::Transparent)
        , _Selected(false)
        , _BodyPadding(FMargin(11.0f, 8.0f))
    {}
        SLATE_ATTRIBUTE(FLinearColor, StripeColor)
        SLATE_ATTRIBUTE(FLinearColor, GlowColor)
        SLATE_ATTRIBUTE(bool, Selected)
        // Unset follows Style Lab's Surface Elevation live; callers may bind/override for bespoke glows.
        SLATE_ATTRIBUTE(float, GlowExtent)
        SLATE_ARGUMENT(FMargin, BodyPadding)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
