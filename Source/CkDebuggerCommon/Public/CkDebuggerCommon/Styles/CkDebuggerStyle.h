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

    /**
     * Shape variants for the CornerStyle axis. Rounded resolves through CkStyle's own rounded
     * family; only the square and pill ends live here, registered once (never allocated per frame).
     * Both are WHITE — tint at the use site like every other brush in this set.
     */
    static auto Get_SquareBrush() -> const FSlateBrush*;
    static auto Get_PillBrush()   -> const FSlateBrush*;

    /**
     * Transparent-fill, 1px-ring counterparts of the three corner shapes — the SurfaceElevation
     * axis' Outlined option and the EntityRef axis' OutlinePill. The ring is white, so the widget's
     * BorderBackgroundColor picks the ring color; the fill stays transparent whatever tint is
     * applied, which is what lets one Slate tint express both a fill and a ring.
     */
    static auto Get_SurfaceOutlineBrush() -> const FSlateBrush*;
    static auto Get_RoundedOutlineBrush() -> const FSlateBrush*;
    static auto Get_PillOutlineBrush()    -> const FSlateBrush*;

    /** The tinted backdrop and the ring behind an icon glyph (IconTreatment Well / Ring). */
    static auto Get_IconWellBrush() -> const FSlateBrush*;
    static auto Get_IconRingBrush() -> const FSlateBrush*;

    /** Alternating row fills for the RowBanding axis' Zebra option. */
    static auto Get_RowBandBrush(bool InIsOddRow) -> const FSlateBrush*;

    /** The 1px rule brush — the separator, and RowBanding's Hairline option. */
    static auto Get_SeparatorBrush() -> const FSlateBrush*;

    static constexpr float Padding_Small = 4.0f;
    static constexpr float Padding_Medium = 8.0f;
    static constexpr float Padding_Large = 16.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
    static TArray<FName> GeneralIconPool;

    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};

// ====================================================================================================================
