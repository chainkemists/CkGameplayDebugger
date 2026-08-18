#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// THE canonical icon widget: a glyph that always explains itself on hover.
//
// Every debugger glyph should render through this instead of a bare SImage —
// a bare SImage in a layout slot silently ships without a tooltip, which is
// how half the suite's icons became un-hoverable. The Meaning argument is the
// point of the widget: what the glyph STANDS FOR, shown as the tooltip.
//
// The widget is passive (no button, consumes no clicks) — safe inside
// SListView/STreeView rows and inside interactive wrappers. When the icon
// sits inside a control that already carries a richer tooltip (a filter
// chip's SCheckBox, a launcher tool button), keep that wrapper tooltip and
// skip this widget — one surface, one tooltip.
//
// Brushes stay owned by each debugger's own style registry (e.g.
// FCkIconStyle::Get_Brush) — pass the pointer in.
//
// The IconTreatment axis draws a live backdrop behind the glyph (bare / tinted
// well / thin ring). Plain costs zero padding and paints nothing, so the
// default is byte-identical to the bare glyph this widget shipped with.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_Icon : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Icon)
        : _Brush(nullptr)
        , _ColorAndOpacity(FSlateColor::UseForeground())
        , _Accent(FLinearColor::Transparent)
    {}
        // The glyph. Nullptr renders nothing (matches GetOptionalBrush misses).
        SLATE_ARGUMENT(const FSlateBrush*, Brush)
        // What the glyph stands for — becomes the hover tooltip. Required in
        // spirit: an icon without a Meaning is exactly the defect this widget
        // exists to prevent.
        SLATE_ATTRIBUTE(FText, Meaning)
        SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
        // The feature color the IconTreatment backdrop tints itself from. A zero-alpha
        // accent (the default) means "unset" and the backdrop falls back to the muted
        // text role — Plain draws no backdrop at all, so under the default treatment
        // this argument changes nothing.
        SLATE_ATTRIBUTE(FLinearColor, Accent)
        // Optional square size override (e.g. 14 for rail chips, 16 default). Whatever is
        // set here is the BASE size: the IconSize axis scales it (Medium 16 = x1.0, Small
        // 12 = x0.75, Large 20 = x1.25) live, via attribute, so a Style Lab flip resizes
        // every existing glyph without any call site changing.
        SLATE_ARGUMENT(TOptional<FVector2D>, Size)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
