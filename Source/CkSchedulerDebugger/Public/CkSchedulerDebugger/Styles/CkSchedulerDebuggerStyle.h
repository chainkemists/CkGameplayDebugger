#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

namespace FCkSchedulerDebuggerStyle
{
    // Background
    inline const FLinearColor Color_Background_Dark     = FLinearColor(0.071f, 0.071f, 0.102f);
    inline const FLinearColor Color_Background_Medium   = FLinearColor(0.118f, 0.118f, 0.165f);
    inline const FLinearColor Color_Background_Light    = FLinearColor(0.157f, 0.157f, 0.220f);
    inline const FLinearColor Color_Border              = FLinearColor(0.216f, 0.216f, 0.294f);

    // Text
    inline const FLinearColor Color_Text_Primary        = FLinearColor(0.878f, 0.878f, 0.878f);
    inline const FLinearColor Color_Text_Secondary      = FLinearColor(0.627f, 0.627f, 0.686f);
    inline const FLinearColor Color_Text_Muted          = FLinearColor(0.392f, 0.392f, 0.451f);
    inline const FLinearColor Color_Text_Highlight      = FLinearColor(1.0f, 1.0f, 1.0f);

    // Selection
    inline const FLinearColor Color_Selection           = FLinearColor(0.235f, 0.510f, 0.863f);

    // Status
    inline const FLinearColor Color_Active              = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Pumped              = FLinearColor(1.0f, 0.655f, 0.149f);
    inline const FLinearColor Color_Ghost               = FLinearColor(0.314f, 0.314f, 0.373f);
    inline const FLinearColor Color_Warning             = FLinearColor(0.937f, 0.325f, 0.314f);
    inline const FLinearColor Color_DirtyMarker         = FLinearColor(1.0f, 0.655f, 0.149f);
    inline const FLinearColor Color_Parallel            = FLinearColor(0.400f, 0.730f, 0.910f);

    // Group accent colors (matched to mockup)
    inline const FLinearColor Color_Group_PreDestruction        = FLinearColor(0.706f, 0.314f, 0.314f);
    inline const FLinearColor Color_Group_DestructionPipeline   = FLinearColor(0.706f, 0.392f, 0.275f);
    inline const FLinearColor Color_Group_Gameplay              = FLinearColor(0.235f, 0.588f, 0.784f);
    inline const FLinearColor Color_Group_Physics               = FLinearColor(0.784f, 0.627f, 0.235f);
    inline const FLinearColor Color_Group_Transform             = FLinearColor(0.510f, 0.706f, 0.314f);
    inline const FLinearColor Color_Group_PostTransform         = FLinearColor(0.392f, 0.627f, 0.510f);
    inline const FLinearColor Color_Group_Replication           = FLinearColor(0.627f, 0.392f, 0.784f);
    inline const FLinearColor Color_Group_EntityLifecycle       = FLinearColor(0.392f, 0.510f, 0.706f);
    inline const FLinearColor Color_Group_Overlap               = FLinearColor(0.784f, 0.471f, 0.627f);
    inline const FLinearColor Color_Group_Default               = FLinearColor(0.500f, 0.500f, 0.550f);

    // Timing heat colors
    inline const FLinearColor Color_Heat_Fast           = FLinearColor(0.263f, 0.627f, 0.278f);
    inline const FLinearColor Color_Heat_Medium         = FLinearColor(0.900f, 0.800f, 0.200f);
    inline const FLinearColor Color_Heat_Slow           = FLinearColor(0.900f, 0.500f, 0.150f);
    inline const FLinearColor Color_Heat_Hot            = FLinearColor(0.937f, 0.325f, 0.314f);

    // Graph
    inline const FLinearColor Color_Graph_Canvas        = FLinearColor(0.071f, 0.071f, 0.102f);
    inline const FLinearColor Color_Graph_Edge          = FLinearColor(0.275f, 0.275f, 0.353f);
    inline const FLinearColor Color_Graph_NodeBody      = FLinearColor(0.137f, 0.137f, 0.196f);
    inline const FLinearColor Color_Graph_NodeBorder    = FLinearColor(0.255f, 0.255f, 0.333f);

    // Layout constants
    inline constexpr float Padding_Small    = 4.0f;
    inline constexpr float Padding_Medium   = 8.0f;
    inline constexpr float Padding_Large    = 16.0f;

    inline constexpr float Node_Width       = 160.0f;
    inline constexpr float Node_Height      = 52.0f;
    inline constexpr float Node_AccentWidth = 4.0f;
    inline constexpr float Node_CornerRadius = 6.0f;
    inline constexpr float Node_BorderThickness = 2.0f;

    // Helpers

    inline auto
    Get_GroupColor(
        const FName& InGroupName)
        -> FLinearColor
    {
        if (InGroupName.ToString().Contains(TEXT("PreDestruction")))       { return Color_Group_PreDestruction; }
        if (InGroupName.ToString().Contains(TEXT("Destruction")))          { return Color_Group_DestructionPipeline; }
        if (InGroupName.ToString().Contains(TEXT("Gameplay")))             { return Color_Group_Gameplay; }
        if (InGroupName.ToString().Contains(TEXT("Physics")))              { return Color_Group_Physics; }
        if (InGroupName.ToString().Contains(TEXT("PostTransform")))        { return Color_Group_PostTransform; }
        if (InGroupName.ToString().Contains(TEXT("Transform")))            { return Color_Group_Transform; }
        if (InGroupName.ToString().Contains(TEXT("Replication")))          { return Color_Group_Replication; }
        if (InGroupName.ToString().Contains(TEXT("EntityLifecycle")))      { return Color_Group_EntityLifecycle; }
        if (InGroupName.ToString().Contains(TEXT("Overlap")))              { return Color_Group_Overlap; }
        return Color_Group_Default;
    }

    inline auto
    Get_TimingColor(
        double InTimeMs)
        -> FLinearColor
    {
        if (InTimeMs < 0.05)  { return Color_Heat_Fast; }
        if (InTimeMs < 0.15)  { return Color_Heat_Medium; }
        if (InTimeMs < 0.30)  { return Color_Heat_Slow; }
        return Color_Heat_Hot;
    }
}

// --------------------------------------------------------------------------------------------------------------------
