#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Inline style constants for the CkNavDebugger module. Mirrors CkAStarDebuggerStyle layout.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkNavDebuggerStyle
{
    // Status colors (mirror CkAStarDebugger to keep visual continuity)
    inline const FLinearColor Color_Status_None     = FLinearColor(0.565f, 0.565f, 0.565f);
    inline const FLinearColor Color_Status_Pending  = FLinearColor(0.961f, 0.620f, 0.043f);  // amber
    inline const FLinearColor Color_Status_Ready    = FLinearColor(0.133f, 0.773f, 0.369f);  // green
    inline const FLinearColor Color_Status_Partial  = FLinearColor(0.961f, 0.620f, 0.043f);  // amber
    inline const FLinearColor Color_Status_Failed   = FLinearColor(0.937f, 0.267f, 0.267f);  // red

    // Health-check indicator colors
    inline const FLinearColor Color_Health_Pass     = FLinearColor(0.133f, 0.773f, 0.369f);
    inline const FLinearColor Color_Health_Warn     = FLinearColor(0.961f, 0.620f, 0.043f);
    inline const FLinearColor Color_Health_Fail     = FLinearColor(0.937f, 0.267f, 0.267f);

    // In-world debug-draw colors (used by FCkNavDebugger_WorldDraw)
    inline const FLinearColor Color_Draw_PathReady     = FLinearColor(0.133f, 0.773f, 0.369f);
    inline const FLinearColor Color_Draw_PathPartial   = FLinearColor(0.961f, 0.620f, 0.043f);
    inline const FLinearColor Color_Draw_PathFailed    = FLinearColor(0.937f, 0.267f, 0.267f);
    inline const FLinearColor Color_Draw_AgentCapsule  = FLinearColor(0.231f, 0.510f, 0.965f);
    inline const FLinearColor Color_Draw_ProjectionOK  = FLinearColor(0.024f, 0.714f, 0.831f);
    inline const FLinearColor Color_Draw_ProjectionBad = FLinearColor(0.937f, 0.267f, 0.267f);

    // General text
    inline const FLinearColor Color_Text_Primary   = FLinearColor(0.878f, 0.878f, 0.878f);
    inline const FLinearColor Color_Text_Secondary = FLinearColor(0.533f, 0.573f, 0.627f);
    inline const FLinearColor Color_Text_Muted     = FLinearColor(0.290f, 0.333f, 0.408f);

    // Panel backgrounds
    inline const FLinearColor Color_Panel_Background      = FLinearColor(0.086f, 0.129f, 0.243f);
    inline const FLinearColor Color_Panel_DarkBackground  = FLinearColor(0.102f, 0.102f, 0.180f);
    inline const FLinearColor Color_Panel_Border          = FLinearColor(0.165f, 0.165f, 0.306f);

    // Layout
    inline constexpr float Panel_Padding   = 12.0f;
    inline constexpr float Section_Spacing = 16.0f;
    inline constexpr float Row_Height      = 18.0f;
    inline constexpr float Label_FontSize  = 10.0f;
}

// --------------------------------------------------------------------------------------------------------------------
