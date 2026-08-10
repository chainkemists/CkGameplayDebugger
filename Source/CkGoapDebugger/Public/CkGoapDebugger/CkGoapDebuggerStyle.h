#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

// ====================================================================================================================
// CkGoap Debugger style — brushes, text styles, and layout constants. Owned by the CkGoapDebugger
// module; registered at StartupModule, unregistered at ShutdownModule.
//
// Colors are NOT stored here. Every brush and text style resolves its ink from a CkStyle:: role at
// registration time (StartupModule, well past static init), so an Editor Preferences → Ck → Style
// edit moves this style set with the rest of the suite.
// ====================================================================================================================

class FCkGoapDebuggerStyle
{
public:
    static auto Initialize()      -> void;
    static auto Shutdown()        -> void;
    static auto Get()             -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

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

    static constexpr float GraphNode_Width       = 180.0f;   // minimum card width — cards self-size to content
    static constexpr float GraphNode_MaxWidth    = 420.0f;   // cap; beyond this ellipsis + tooltip take over
    static constexpr float GraphNode_MinHeight   = 60.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;

    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle)    -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};

// ====================================================================================================================
