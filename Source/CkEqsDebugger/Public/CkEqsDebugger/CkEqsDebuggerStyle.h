#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Inline style constants for the EQS Debugger module. Mirrors CkAStarDebuggerStyle's palette where possible so the
// debugger family looks consistent.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkEqsDebuggerStyle
{
    // Status colors — query lifecycle (Pending / InProgress / Complete / Failed / Cancelled)
    inline const FLinearColor Color_Status_Pending     = FLinearColor(0.565f, 0.565f, 0.565f);
    inline const FLinearColor Color_Status_InProgress  = FLinearColor(0.231f, 0.510f, 0.965f);  // #3B82F6
    inline const FLinearColor Color_Status_Complete    = FLinearColor(0.133f, 0.773f, 0.369f);  // #22C55E
    inline const FLinearColor Color_Status_Failed      = FLinearColor(0.937f, 0.267f, 0.267f);  // #EF4444
    inline const FLinearColor Color_Status_Cancelled   = FLinearColor(0.961f, 0.620f, 0.043f);  // #F59E0B

    // Candidate score gradient — blue (low) → green (high), interpolate by normalized score in [0, 1].
    inline const FLinearColor Color_Score_Low          = FLinearColor(0.231f, 0.510f, 0.965f);  // blue
    inline const FLinearColor Color_Score_High         = FLinearColor(0.133f, 0.773f, 0.369f);  // green
    inline const FLinearColor Color_Score_Best         = FLinearColor(0.984f, 0.749f, 0.141f);  // amber-yellow highlight
    inline const FLinearColor Color_Score_Failed       = FLinearColor(0.290f, 0.333f, 0.408f);  // muted gray for filter-failed candidates

    // General text (matching family palette)
    inline const FLinearColor Color_Text_Primary       = FLinearColor(0.878f, 0.878f, 0.878f);
    inline const FLinearColor Color_Text_Secondary     = FLinearColor(0.533f, 0.573f, 0.627f);  // #8892A0
    inline const FLinearColor Color_Text_Muted         = FLinearColor(0.290f, 0.333f, 0.408f);  // #4A5568

    // Panel backgrounds
    inline const FLinearColor Color_Panel_Background       = FLinearColor(0.086f, 0.129f, 0.243f);  // #16213E
    inline const FLinearColor Color_Panel_DarkBackground   = FLinearColor(0.102f, 0.102f, 0.180f);  // #1A1A2E
    inline const FLinearColor Color_Panel_Border           = FLinearColor(0.165f, 0.165f, 0.306f);  // #2A2A4E

    // Layout
    inline constexpr float Panel_Padding      = 12.0f;
    inline constexpr float Section_Spacing    = 16.0f;
    inline constexpr float Row_Height         = 18.0f;
    inline constexpr float MiniStat_FontSize  = 18.0f;
    inline constexpr float Label_FontSize     = 10.0f;

    // World-overlay (CkPmg sphere markers placed at candidate locations)
    inline constexpr float Overlay_SphereRadius_Default = 16.0f;
    inline constexpr float Overlay_SphereRadius_Best    = 24.0f;
}

// --------------------------------------------------------------------------------------------------------------------
