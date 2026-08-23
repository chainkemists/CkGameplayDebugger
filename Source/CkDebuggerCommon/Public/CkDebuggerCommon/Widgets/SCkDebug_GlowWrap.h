#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Wraps any content with a soft tinted halo — the Slate equivalent of a CSS
// box-shadow glow. The glow is an alpha-falloff 9-slice drawn UNDER the
// content, extending `Extent` px past it on every side (the wrap therefore
// adds 2×Extent to the desired size; layouts should treat that as breathing
// room, exactly like a shadow).
//
// GlowColor is attribute-bound: alpha 0 hides the halo entirely (collapsed
// image, zero paint cost), so flag-driven glows (active-chain leaf, trace
// highlight) flip live without a rebuild.
//
//   SNew(SCkDebug_GlowWrap)
//       .GlowColor_Lambda([this] { return _IsActive ? CkStyle::Accent() : FLinearColor::Transparent; })
//       [ SNew(SCkDebug_NodePill) ... ]
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_GlowWrap : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_GlowWrap)
        : _GlowColor(FLinearColor::Transparent)
        , _Extent(7.0f)
        , _Tight(false)
        , _GlowOpacity(0.55f)
    {}
        // Tint of the halo. Alpha 0 → hidden.
        SLATE_ATTRIBUTE(FLinearColor, GlowColor)
        // How far (px) the halo extends past the content on each side.
        SLATE_ATTRIBUTE(float, Extent)
        // true → crisp small-element halo (Glow_Tight), false → wide soft halo.
        SLATE_ARGUMENT(bool, Tight)
        // Multiplied into GlowColor's alpha — one knob for overall strength.
        SLATE_ARGUMENT(float, GlowOpacity)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
