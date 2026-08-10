#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Grid-canvas constants for the A* Debugger.
//
// KEPT LOCAL ON PURPOSE: these hues are the search algorithm's own semantics (start / goal / open set /
// closed set / path / blocked), not UI status — a reader decodes the grid by them and nothing else in the
// suite means the same thing, so they are the "semantic canvas color" exception to the CkStyle:: role rule
// (CkDebuggerCommon/CLAUDE.md → "Domain ramps"). Every OTHER color this module used (text, panels, status,
// budget, exploration) now resolves from CkStyle:: roles / ck::debug_axes ramps at the use site.
// --------------------------------------------------------------------------------------------------------------------

namespace FCkAStarDebuggerStyle
{
    // Cell states (matching mockup hex values)
    inline const FLinearColor Color_Cell_Empty          = FLinearColor(0.102f, 0.102f, 0.180f);  // #1A1A2E
    inline const FLinearColor Color_Cell_Blocked        = FLinearColor(0.239f, 0.067f, 0.067f);  // #3D1111
    inline const FLinearColor Color_Cell_OpenSet        = FLinearColor(0.118f, 0.302f, 0.549f);  // #1E4D8C
    inline const FLinearColor Color_Cell_ClosedSet      = FLinearColor(0.176f, 0.216f, 0.282f);  // #2D3748
    inline const FLinearColor Color_Cell_Path           = FLinearColor(0.133f, 0.773f, 0.369f);  // #22C55E
    inline const FLinearColor Color_Cell_Start          = FLinearColor(0.024f, 0.714f, 0.831f);  // #06B6D4
    inline const FLinearColor Color_Cell_Goal           = FLinearColor(0.961f, 0.620f, 0.043f);  // #F59E0B

    // Path overlay — the polyline drawn through the path cells, so it is the path cell state's own hue.
    inline const FLinearColor Color_Path_Line           = FLinearColor(0.133f, 0.773f, 0.369f);  // #22C55E
    inline constexpr float Path_LineThickness           = 2.0f;

    // Grid geometry
    inline constexpr float Grid_CellSize    = 16.0f;
    inline constexpr float Grid_CellGap     = 1.0f;
    inline constexpr float Grid_MinCellSize = 4.0f;
    inline constexpr float Grid_MaxCellSize = 48.0f;
}

// --------------------------------------------------------------------------------------------------------------------
