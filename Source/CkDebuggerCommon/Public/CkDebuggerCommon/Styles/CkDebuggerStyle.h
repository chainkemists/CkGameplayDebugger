#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

// ====================================================================================================================
// THE brush/text-style set for the CK debugger suite.
//
// Promoted out of CkEcsDebugger (2026-08-09, common-widget consolidation P5/U1) — the class name and
// the "CkDebuggerStyle" style-set name are deliberately unchanged so every existing call site only
// swaps its #include. Feature modules adopt this set instead of registering module-local equivalents.
//
// Division of labour:
//   CkStyle::                (CkEditorTools) colors + typography tokens — tunable, no Slate objects
//   FCkDebuggerStyle         registered BRUSHES / text styles / the SVG icon registry (this file)
//   FCkDebuggerCommonStyle   brushes only the common widgets need (glow halos, flat button, toggle)
//
// Everything here is SlateCore-only, so the set builds and registers in a non-editor target — which
// it must, CkDebuggerCommon being a Runtime module. Nothing in it is WITH_EDITOR gated.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkDebuggerStyle
{
public:
    static auto Initialize() -> void;
    static auto Shutdown() -> void;
    static auto Get() -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

    /**
     * Resolves "CkDebugger.Icon.<InIconId>" — the monochrome feature glyphs registered
     * from Resources/Icons/*.svg. They are white by design: tint per-feature at draw
     * time via SImage.ColorAndOpacity so one asset serves every color/state.
     * Unknown ids yield nullptr — pass registered ids only.
     */
    static auto Get_IconBrush(FName InIconId) -> const FSlateBrush*;

    /**
     * The general archetype glyph library (Resources/Icons/General/*.svg, sorted by
     * name). Archetypes without a bespoke or dominant-feature icon hash their key into
     * this pool for a stable, distinct glyph. Game archetype descriptors may also name
     * any of these directly via their IconSvgPath.
     */
    static auto Get_GeneralIconPool() -> const TArray<FName>&;

    static constexpr float Padding_Small = 4.0f;
    static constexpr float Padding_Medium = 8.0f;
    static constexpr float Padding_Large = 16.0f;

    static constexpr float GraphNode_Width = 180.0f;
    static constexpr float GraphNode_Height = 40.0f;
    static constexpr float GraphNode_AccentWidth = 4.0f;
    static constexpr float GraphNode_CornerRadius = 6.0f;
    static constexpr float GraphNode_BorderThickness = 2.0f;
    static constexpr float GraphNode_BorderThickness_Center = 3.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
    static TArray<FName> GeneralIconPool;

    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};

// ====================================================================================================================
