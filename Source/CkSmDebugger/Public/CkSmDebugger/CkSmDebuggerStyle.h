#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Inline style constants for the SM Debugger module.
// These are self-contained — no dependency on CkEcsDebugger.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkSmDebuggerStyle
{
    // Colors — state machine nodes
    inline const FLinearColor Color_Sm_CurrentStateBorder     = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Sm_InactiveStateBorder    = FLinearColor(0.329f, 0.431f, 0.478f);
    inline const FLinearColor Color_Sm_TransitionQueuedBorder = FLinearColor(1.0f, 0.718f, 0.302f);
    inline const FLinearColor Color_Sm_NodeBackground         = FLinearColor(0.125f, 0.125f, 0.188f);
    inline const FLinearColor Color_Sm_NodeHeader             = FLinearColor(0.165f, 0.165f, 0.235f);
    inline const FLinearColor Color_Sm_CanvasBackground       = FLinearColor(0.071f, 0.071f, 0.102f);
    inline const FLinearColor Color_Sm_CanvasGridLines        = FLinearColor(0.118f, 0.118f, 0.173f, 0.314f);
    inline const FLinearColor Color_Sm_TransitionSatisfied    = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Sm_TransitionUnsatisfied  = FLinearColor(0.290f, 0.353f, 0.408f);
    inline const FLinearColor Color_Sm_TransitionHovered      = FLinearColor(0.565f, 0.792f, 0.976f);
    inline const FLinearColor Color_Sm_TextPrimary            = FLinearColor(0.878f, 0.878f, 0.878f);
    inline const FLinearColor Color_Sm_TextSecondary          = FLinearColor(0.565f, 0.565f, 0.565f);
    inline const FLinearColor Color_Sm_TaskRunning            = FLinearColor(1.0f, 0.757f, 0.027f);
    inline const FLinearColor Color_Sm_TaskSucceeded          = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Sm_TaskFailed             = FLinearColor(0.937f, 0.325f, 0.314f);
    inline const FLinearColor Color_Sm_ConditionSatisfied     = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Sm_ConditionUnsatisfied   = FLinearColor(0.937f, 0.325f, 0.314f);
    inline const FLinearColor Color_Sm_ConditionUnknown       = FLinearColor(0.565f, 0.565f, 0.565f);
    inline const FLinearColor Color_Sm_Breakpoint             = FLinearColor(0.937f, 0.325f, 0.314f);
    inline const FLinearColor Color_Sm_BreakpointOutline      = FLinearColor(0.937f, 0.325f, 0.314f, 0.753f);
    inline const FLinearColor Color_Sm_NodeShadow             = FLinearColor(0.0f, 0.0f, 0.0f, 0.188f);
    inline const FLinearColor Color_Sm_HeaderSeparator        = FLinearColor(0.251f, 0.251f, 0.333f, 0.502f);
    inline const FLinearColor Color_Sm_TransitionBadgeBg      = FLinearColor(0.102f, 0.102f, 0.165f, 0.878f);
    inline const FLinearColor Color_Sm_SubSmNodeBackground    = FLinearColor(0.102f, 0.118f, 0.180f);
    inline const FLinearColor Color_Sm_SubSmNodeHeader        = FLinearColor(0.165f, 0.176f, 0.267f);
    inline const FLinearColor Color_Sm_SubSmInactiveBorder    = FLinearColor(0.361f, 0.420f, 0.753f);
    inline const FLinearColor Color_Sm_SubSmBadge             = FLinearColor(0.259f, 0.647f, 0.961f);
    inline const FLinearColor Color_Sm_SubSmConnector         = FLinearColor(0.259f, 0.647f, 0.961f, 0.502f);
    inline const FLinearColor Color_Sm_SubSmLabel             = FLinearColor(0.259f, 0.647f, 0.961f, 0.753f);

    // Active state — orange/amber matching LogicDriverPro active highlight
    inline const FLinearColor Color_Sm_ActiveStateBorder      = FLinearColor(0.85f, 0.55f, 0.25f);
    inline const FLinearColor Color_Sm_InactiveStateBody      = FLinearColor(0.125f, 0.125f, 0.188f);

    // Title shadow (LogicDriverPro uses 0.6 gray)
    inline const FLinearColor Color_Sm_TitleShadow            = FLinearColor(0.6f, 0.6f, 0.6f);

    // Transition badge / wire
    inline const FLinearColor Color_Sm_TransitionBadge        = FLinearColor(0.9f, 0.9f, 0.9f);
    inline const FLinearColor Color_Sm_TransitionWire         = FLinearColor(0.290f, 0.353f, 0.408f);

    // Scrub / live highlight
    inline const FLinearColor Color_Sm_ScrubActiveOutline     = FLinearColor(0.263f, 0.627f, 0.278f, 0.45f);
    inline const FLinearColor Color_Sm_PreviousStateOutline   = FLinearColor(0.55f, 0.55f, 0.6f, 0.40f);
    inline const FLinearColor Color_Sm_ScrubTransitionWire    = FLinearColor(0.259f, 0.647f, 0.961f, 1.0f);
    inline const FLinearColor Color_Sm_LiveFlashWire          = FLinearColor(0.85f, 0.55f, 0.25f, 1.0f);

    // Active-state highlight: the current state's node border + halo are bright
    // green (the only green in the graph). Swap this if green clashes with the
    // satisfied-transition green elsewhere.
    inline const FLinearColor Color_Sm_ActiveOutline          = FLinearColor(0.353f, 0.820f, 0.498f);

    // Entry node
    inline const FLinearColor Color_Sm_EntryText              = FLinearColor(0.7f, 0.7f, 0.7f);

    // Overridden state — inline "OVERRIDE" label under the state name
    inline const FLinearColor Color_Sm_Override               = FLinearColor(0.72f, 0.42f, 0.95f);

    // Event-driven vs polled / ticking — inline "EVENT-DRIVEN" / "TICK" labels and detail-panel pills
    inline const FLinearColor Color_Sm_EventDriven            = FLinearColor(0.95f, 0.75f, 0.25f);
    inline const FLinearColor Color_Sm_Polled                 = FLinearColor(0.60f, 0.60f, 0.65f);
    inline const FLinearColor Color_Sm_TaskTick               = FLinearColor(0.85f, 0.55f, 0.25f);

    // General text
    inline const FLinearColor Color_Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    inline const FLinearColor Color_Text_Secondary  = FLinearColor(0.5f, 0.5f, 0.55f);
    inline const FLinearColor Color_Text_Muted      = FLinearColor(0.35f, 0.35f, 0.4f);
    inline const FLinearColor Color_Text_Highlight  = FLinearColor(1.0f, 1.0f, 1.0f);
    inline const FLinearColor Color_Success         = FLinearColor(0.2f, 0.8f, 0.2f);
    inline const FLinearColor Color_Warning         = FLinearColor(0.9f, 0.7f, 0.1f);
    inline const FLinearColor Color_Background_Dark = FLinearColor(0.01f, 0.01f, 0.01f);

    // Layout
    inline constexpr float Sm_NodePadding             = 6.0f;
    inline constexpr float Sm_PinPadding              = 4.0f;
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
