#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Layout + timing constants for the SM Debugger graph.
//
// COLOURS DO NOT LIVE HERE. They resolve from `CkStyle::` roles (CkEditorTools/Style/CkStyle.h,
// tunable under Editor Preferences -> Ck -> Style) at the use site. This header used to carry a
// ~35-token local palette on the rationale "self-contained, no dependency on CkEcsDebugger"; that
// rationale is obsolete — the suite's style set lives in CkDebuggerCommon, which this module has
// depended on for a long time. The single colour that survived is below, with its reason.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkSmDebuggerStyle
{
    // KEPT LOCAL (SM-semantic): the "this state's script was overridden at runtime" marker. The
    // palette has no override/mutation role, and its only purples (Value_Tag, CategoryAge) already
    // carry unrelated value-type / action-category meaning.
    inline const FLinearColor Color_Sm_Override = FLinearColor(0.72f, 0.42f, 0.95f);

    // Layout
    inline constexpr float Sm_NodePadding             = 6.0f;
    inline constexpr float Sm_HeaderHeight            = 24.0f;
    inline constexpr float Sm_TaskRowHeight           = 18.0f;
    inline constexpr float Sm_CornerRadius            = 6.0f;
    inline constexpr float Sm_BorderThickness         = 2.0f;
    inline constexpr float Sm_ActiveBorderThickness   = 2.0f;
    inline constexpr float Sm_GridSpacing             = 64.0f;
    inline constexpr float Sm_ArrowSize               = 6.0f;
    inline constexpr float Sm_BiDirectionalOffset     = 8.0f;
    inline constexpr float Sm_AccentBarWidth          = 3.0f;
    inline constexpr float Sm_StateIconSize           = 5.0f;
    inline constexpr float Sm_StateIconGap            = 6.0f;
    inline constexpr float Sm_TransitionBadgeRadius   = 10.0f;
    inline constexpr float Sm_TransitionBadgeFontSize = 10.0f;

    // Seconds for live highlights to fade. Fast = becoming current / becoming
    // previous (snappy); the slow duration is the final fade of the grey
    // previous-halo out to nothing (lingers so you can track where it came from).
    inline constexpr float Sm_HighlightFadeFast       = 0.18f;
    inline constexpr float Sm_HighlightFadeDuration   = 0.6f;

    // Seconds for the one-shot "just became current" entry overshoot (a brief
    // brightening of the border colour) to play out. Shorter than the fade —
    // it's a transient attention grab, not a steady-state.
    inline constexpr float Sm_EntryPulseDuration      = 0.5f;

}

// --------------------------------------------------------------------------------------------------------------------
