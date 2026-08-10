#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

#include "CkEditorTools/Style/CkStyle.h"

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
    // The padding ladder is an ALIAS onto the suite spacing ladder, not a parallel one. It used to
    // repeat 2/4/8 verbatim and then diverge at the top rung (14 where the suite's is 16), which is
    // exactly the drift the alias removes. The reconciliation moves one call site — the WS rail's
    // outer container padding (SCkGoapDebugger_WorldStateRail.cpp) — by 2px per edge; every other
    // rung is byte-identical.
    static constexpr float Padding_XSmall  = CkStyle::SpaceXS;
    static constexpr float Padding_Small   = CkStyle::SpaceS;
    static constexpr float Padding_Medium  = CkStyle::SpaceM;
    static constexpr float Padding_Large   = CkStyle::SpaceXL;

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
