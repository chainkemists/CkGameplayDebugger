#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

class FCkDebuggerStyle
{
public:
    static auto Initialize() -> void;
    static auto Shutdown() -> void;
    static auto Get() -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

    static constexpr float Padding_Small = 4.0f;
    static constexpr float Padding_Medium = 8.0f;
    static constexpr float Padding_Large = 16.0f;

    static const FLinearColor Color_Background_Dark;
    static const FLinearColor Color_Background_Medium;
    static const FLinearColor Color_Background_Light;
    static const FLinearColor Color_Border;
    static const FLinearColor Color_Selection;
    static const FLinearColor Color_SelectionInactive;
    static const FLinearColor Color_Hover;
    
    static const FLinearColor Color_Text_Primary;
    static const FLinearColor Color_Text_Secondary;
    static const FLinearColor Color_Text_Muted;
    static const FLinearColor Color_Text_Highlight;
    
    static const FLinearColor Color_Entity_ID;
    static const FLinearColor Color_Transform;
    static const FLinearColor Color_Network;
    static const FLinearColor Color_Relationship;
    static const FLinearColor Color_Attribute;
    static const FLinearColor Color_Reference;
    static const FLinearColor Color_None;
    static const FLinearColor Color_Error;
    static const FLinearColor Color_Warning;
    static const FLinearColor Color_Success;

    // ---- Value-type colors (for displaying typed data uniformly across inspectors)

    static const FLinearColor Color_Value_Bool_True;
    static const FLinearColor Color_Value_Bool_False;
    static const FLinearColor Color_Value_Numeric;
    static const FLinearColor Color_Value_String;
    static const FLinearColor Color_Value_Math;
    static const FLinearColor Color_Value_Tag;
    static const FLinearColor Color_Value_Enum;
    static const FLinearColor Color_Value_Object;
    static const FLinearColor Color_Value_Handle;

    // ---- State colors (for enabled/disabled/overlapping status)

    static const FLinearColor Color_State_Enabled;
    static const FLinearColor Color_State_Disabled;
    static const FLinearColor Color_State_Overlapping;
    static const FLinearColor Color_State_Config;

    // ---- Status colors (for objective/task progress)

    static const FLinearColor Color_Status_NotStarted;
    static const FLinearColor Color_Status_Active;
    static const FLinearColor Color_Status_Completed;
    static const FLinearColor Color_Status_Failed;

    // ---- Graph colors

    static const FLinearColor Color_Graph_Background;
    static const FLinearColor Color_Graph_Edge;
    static const FLinearColor Color_Graph_Node_Center;
    static const FLinearColor Color_Graph_Node_Default;

    static const FLinearColor Color_Graph_Node_Border_Default;
    static const FLinearColor Color_Graph_Node_Border_Center;

    static constexpr float GraphNode_Width = 180.0f;
    static constexpr float GraphNode_Height = 40.0f;
    static constexpr float GraphNode_AccentWidth = 4.0f;
    static constexpr float GraphNode_CornerRadius = 6.0f;
    static constexpr float GraphNode_BorderThickness = 2.0f;
    static constexpr float GraphNode_BorderThickness_Center = 3.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
    
    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};