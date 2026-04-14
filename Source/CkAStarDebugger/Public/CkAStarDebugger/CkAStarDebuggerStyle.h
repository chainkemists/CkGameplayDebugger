#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Inline style constants for the A* Debugger module.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkAStarDebuggerStyle
{
    // Grid cell colors (matching mockup hex values)
    inline const FLinearColor Color_Cell_Empty          = FLinearColor(0.102f, 0.102f, 0.180f);  // #1A1A2E
    inline const FLinearColor Color_Cell_Blocked        = FLinearColor(0.239f, 0.067f, 0.067f);  // #3D1111
    inline const FLinearColor Color_Cell_OpenSet        = FLinearColor(0.118f, 0.302f, 0.549f);  // #1E4D8C
    inline const FLinearColor Color_Cell_ClosedSet      = FLinearColor(0.176f, 0.216f, 0.282f);  // #2D3748
    inline const FLinearColor Color_Cell_Path           = FLinearColor(0.133f, 0.773f, 0.369f);  // #22C55E
    inline const FLinearColor Color_Cell_Start          = FLinearColor(0.024f, 0.714f, 0.831f);  // #06B6D4
    inline const FLinearColor Color_Cell_Goal           = FLinearColor(0.961f, 0.620f, 0.043f);  // #F59E0B
    inline const FLinearColor Color_Cell_CurrentExpand  = FLinearColor(0.984f, 0.749f, 0.141f);  // #FBBF24

    // Grid rendering
    inline const FLinearColor Color_Grid_Lines          = FLinearColor(0.165f, 0.165f, 0.243f);  // #2A2A3E
    inline const FLinearColor Color_Grid_Border         = FLinearColor(0.290f, 0.333f, 0.408f);  // #4A5568
    inline const FLinearColor Color_Grid_Background     = FLinearColor(0.051f, 0.051f, 0.102f);  // #0D0D1A

    inline constexpr float Grid_CellSize    = 16.0f;
    inline constexpr float Grid_CellGap     = 1.0f;
    inline constexpr float Grid_MinCellSize = 4.0f;
    inline constexpr float Grid_MaxCellSize = 48.0f;

    // Status colors
    inline const FLinearColor Color_Status_Idle               = FLinearColor(0.565f, 0.565f, 0.565f);
    inline const FLinearColor Color_Status_InProgress         = FLinearColor(0.231f, 0.510f, 0.965f);  // #3B82F6
    inline const FLinearColor Color_Status_Complete           = FLinearColor(0.133f, 0.773f, 0.369f);  // #22C55E
    inline const FLinearColor Color_Status_Failed             = FLinearColor(0.937f, 0.267f, 0.267f);  // #EF4444
    inline const FLinearColor Color_Status_CostThreshold      = FLinearColor(0.961f, 0.620f, 0.043f);  // #F59E0B

    // Budget bar
    inline const FLinearColor Color_Budget_Normal  = FLinearColor(0.231f, 0.510f, 0.965f);  // blue
    inline const FLinearColor Color_Budget_Warning = FLinearColor(0.961f, 0.620f, 0.043f);  // amber
    inline const FLinearColor Color_Budget_Over    = FLinearColor(0.937f, 0.267f, 0.267f);  // red

    // Exploration meter
    inline const FLinearColor Color_Exploration    = FLinearColor(0.545f, 0.361f, 0.965f);  // #8B5CF6

    // Path overlay
    inline const FLinearColor Color_Path_Line      = FLinearColor(0.133f, 0.773f, 0.369f);  // green
    inline constexpr float Path_LineThickness      = 2.0f;

    // General text (matching SM debugger)
    inline const FLinearColor Color_Text_Primary   = FLinearColor(0.878f, 0.878f, 0.878f);
    inline const FLinearColor Color_Text_Secondary = FLinearColor(0.533f, 0.573f, 0.627f);  // #8892A0
    inline const FLinearColor Color_Text_Muted     = FLinearColor(0.290f, 0.333f, 0.408f);  // #4A5568

    // Panel backgrounds
    inline const FLinearColor Color_Panel_Background      = FLinearColor(0.086f, 0.129f, 0.243f);  // #16213E
    inline const FLinearColor Color_Panel_DarkBackground   = FLinearColor(0.102f, 0.102f, 0.180f);  // #1A1A2E
    inline const FLinearColor Color_Panel_Border           = FLinearColor(0.165f, 0.165f, 0.306f);  // #2A2A4E

    // Layout
    inline constexpr float Panel_Padding       = 12.0f;
    inline constexpr float Section_Spacing     = 16.0f;
    inline constexpr float Row_Height          = 18.0f;
    inline constexpr float MiniStat_FontSize   = 18.0f;
    inline constexpr float Label_FontSize      = 10.0f;
    inline constexpr float HistoryDot_Size     = 8.0f;
}

// --------------------------------------------------------------------------------------------------------------------
