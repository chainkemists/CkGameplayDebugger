#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

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
// The SM debugger's adapter onto the suite style axes. Mirrors ck_goap_debugger_axes:: — the two
// graph debuggers cannot share one adapter without putting it in CkDebuggerCommon, and the axes lib
// deliberately exposes the ROLE-shaped entry points (Get_NodeBorderThickness / Get_NodeInactiveOpacity)
// rather than per-surface scales.
//
// GraphNodeStyle lands as a SCALE on each surface's own baked geometry, never as an absolute: the
// state pill, the sub-SM bubble and the transition badge each chose their border weight and fade
// against their own content, so the axis owns only the OFFSET between options. Card divides the role
// by itself => 1.0 => today's graph byte-identical (R6).
//
// Every one of these reads the LIVE selection, so they belong inside attribute lambdas / OnPaint —
// a construction-time call bakes the axis and the node stops responding to a profile flip.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_sm_debugger_axes
{
    inline auto Get_Selection() -> const FCkDebuggerStyleSelection&
    {
        return UCkDebuggerStyleSettings::Get_Selection();
    }

    /** Get_NodeBorderThickness() as a multiple of its own role value. Card 1.0. */
    inline auto Get_NodeBorderScale() -> float
    {
        const auto Role = CkStyle::NodeBorderThickness();
        if (Role <= KINDA_SMALL_NUMBER) { return 1.0f; }
        return ck::debug_axes::Get_NodeBorderThickness() / Role;
    }

    /** Get_NodeInactiveOpacity() as a multiple of its own role value — the dim/fade scale. Card 1.0. */
    inline auto Get_NodeDimScale() -> float
    {
        const auto Role = CkStyle::NodeInactiveOpacity();
        if (Role <= KINDA_SMALL_NUMBER) { return 1.0f; }
        return ck::debug_axes::Get_NodeInactiveOpacity() / Role;
    }

    /** Corner-radius scale — Dense squares the node chrome off a little; Card / Minimal leave it. */
    inline auto Get_NodeRadiusScale() -> float
    {
        switch (Get_Selection().GraphNodeStyle)
        {
            case ECkDebugAxis_GraphNodeStyle::Card:    return 1.0f;
            case ECkDebugAxis_GraphNodeStyle::Minimal: return 1.0f;
            case ECkDebugAxis_GraphNodeStyle::Dense:   return 0.5f;
        }

        return 1.0f;
    }

    /**
     * False under Minimal — the node draws its border and its content only, no fill wash. Callers
     * express that through the fill color / alpha they already bind, never by building a different
     * widget tree (graph node widgets are not recreated on an axis flip).
     */
    inline auto Get_NodeDrawsFill() -> bool
    {
        return Get_Selection().GraphNodeStyle != ECkDebugAxis_GraphNodeStyle::Minimal;
    }

    /** Inline node indicators (breakpoint dots, state squares) read as a glyph at half an icon box. */
    inline auto Get_IndicatorSize() -> float
    {
        return ck::debug_axes::Get_IconSize(Get_Selection()) * 0.5f;
    }
}

// --------------------------------------------------------------------------------------------------------------------
