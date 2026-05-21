#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

// ====================================================================================================================
// CkGoap Debugger style — brushes, text styles, layout constants, and palette
// matching mockup F v2. Owned by the CkGoapDebugger module; registered at
// StartupModule, unregistered at ShutdownModule.
//
// Palette mirrors the mockup tokens exactly (hex → linear). The Color_*
// statics double as a quick lookup for code that needs the underlying
// FLinearColor (e.g. node tinting in custom paint paths).
// ====================================================================================================================

class FCkGoapDebuggerStyle
{
public:
    static auto Initialize()      -> void;
    static auto Shutdown()        -> void;
    static auto Get()             -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

    // -- Palette (mockup F v2) -----------------------------------------------
    // Backgrounds
    static const FLinearColor Color_Bg_Root;            // #0a0e17  app background
    static const FLinearColor Color_Bg_Panel;           // #0c1018  sidebar / panels
    static const FLinearColor Color_Bg_Surface;         // #0f1520  card surfaces
    static const FLinearColor Color_Bg_Black;           // #000000  mode bar / graph head

    // Borders
    static const FLinearColor Color_Border_Subtle;      // #1a2332  inner dividers
    static const FLinearColor Color_Border_Strong;      // #2a3342  outer dividers

    // Text
    static const FLinearColor Color_Text_Primary;       // #e0e0e0
    static const FLinearColor Color_Text_Secondary;     // #aaaaaa
    static const FLinearColor Color_Text_Muted;         // #888888
    static const FLinearColor Color_Text_Dim;           // #666666
    static const FLinearColor Color_Text_Faint;         // #555555
    static const FLinearColor Color_Text_Ghost;         // #444444

    // Status accents
    static const FLinearColor Color_Status_PlanFound;   // #22c55e  green
    static const FLinearColor Color_Status_Planning;    // #60a5fa  blue
    static const FLinearColor Color_Status_PlanningBdr; // #3b82f6  blue (border)
    static const FLinearColor Color_Status_Failed;      // #ef4444  red
    static const FLinearColor Color_Status_Selected;    // #f59e0b  amber
    static const FLinearColor Color_Status_Composite;   // #a855f7  purple

    // -- Layout constants -----------------------------------------------------
    static constexpr float Padding_XSmall  = 2.0f;
    static constexpr float Padding_Small   = 4.0f;
    static constexpr float Padding_Medium  = 8.0f;
    static constexpr float Padding_Large   = 14.0f;

    static constexpr float SidebarWidth          = 280.0f;
    static constexpr float SidebarBottomHeight   = 230.0f;
    static constexpr float ModeBarHeight         = 36.0f;
    static constexpr float ToolbarHeight         = 38.0f;
    static constexpr float CornerRadius_Small    = 3.0f;
    static constexpr float CornerRadius_Medium   = 4.0f;
    static constexpr float CornerRadius_Large    = 6.0f;
    static constexpr float CornerRadius_Badge    = 8.0f;
    static constexpr float Border_Thin           = 1.0f;
    static constexpr float Border_Standard       = 1.5f;
    static constexpr float Border_Strong         = 2.0f;

    static constexpr float GraphNode_Width       = 180.0f;
    static constexpr float GraphNode_MinHeight   = 60.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;

    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle)    -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};

// ====================================================================================================================
