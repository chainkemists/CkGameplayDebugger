#pragma once

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Layout/Margin.h"
#include "Widgets/SWidget.h"

// ====================================================================================================================
// The option -> widget mapping for Layer B. Every debugger surface composes through these functions
// so a single axis flip moves every surface at once.
//
// Three families, and they must move in lock-step: RENDER builds the widget, METRIC answers the
// size/spacing question a caller needs BEFORE composing, PREDICATE answers the "does this element
// exist at all" question. A render change that shifts geometry without its metric companion is the
// bug this split exists to prevent.
//
// Colors are never arguments — they resolve from CkStyle:: roles at call time, which is what keeps
// palette and selection orthogonal. Muting is a composition choice (pick the dim role), never an
// alpha overlay wrapped around already-styled children.
//
// Switch convention: NO `default:` label anywhere in this file. Every enumerator is listed, and a
// fallback `return` for the axis default follows the switch — so adding an axis option surfaces as
// a -Wswitch warning at the exact sites that must be updated.
// ====================================================================================================================

namespace ck::debug_axes
{
    // ----- Render ------------------------------------------------------------
    // LIVE BY CONSTRUCTION (R1). Every widget these emit binds its axis-driven visuals through
    // TAttribute lambdas that re-read the LIVE selection at paint time, so flipping an axis in the
    // Style Lab moves widgets that were built long before the flip — no rebuild, no invalidation,
    // no revision watch. Options that differ STRUCTURALLY (a ring the other options don't draw, a
    // box TextOnly doesn't want) are expressed by making that layer transparent and zero-padded,
    // never by building a different widget tree. Brushes are the ones registered once in CkStyle;
    // nothing here allocates (R7).
    //
    // Consequence for `InSelection` on the tone overloads: it is NOT read. It stays for call-site
    // symmetry with the metric / predicate families below, and because every existing caller
    // already hands over `UCkDebuggerStyleSettings::Get_Selection()`. Passing a hypothetical
    // selection will not be honored — the emitted widget always follows the user's live setting.

    // ChipStyle: inspector badge boxes, feature chips, overlay field chips.
    // `InInk` is the accent: the label ink for Tint / Outline / TextOnly, the ring for Outline, and
    // the BODY for Solid (whose label flips to TextStrong). `InFill` is the tint wash and is read
    // by Tint only. `InFontSize` defaults to CkStyle::FontSizeSmall().
    CKDEBUGGERCOMMON_API auto Make_Chip(
        const FText& InText,
        FLinearColor InInk,
        FLinearColor InFill,
        TOptional<int32> InFontSize = {}) -> TSharedRef<SWidget>;

    CKDEBUGGERCOMMON_API auto Make_Chip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // BadgeStyle: count badges, fold badges.
    // `InInk` is the accent: the ring and label ink for Hollow / CountOnly. `InFill` is the Solid
    // body (whose label flips to TextStrong). Note the deliberate asymmetry with Make_Chip — a
    // badge has no tint wash, so its solid body is the caller's Fill rather than its Ink.
    CKDEBUGGERCOMMON_API auto Make_Badge(
        const FText& InText,
        FLinearColor InInk,
        FLinearColor InFill,
        TOptional<int32> InFontSize = {}) -> TSharedRef<SWidget>;

    CKDEBUGGERCOMMON_API auto Make_Badge(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // FoldChipStyle: the tree's fold / group affordance. Presentation only — the clickable
    // wrapper stays at the call site, so the result is safe to drop inside an STableRow.
    CKDEBUGGERCOMMON_API auto Make_FoldChip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // ProviderChipStyle: overlay section provider chips.
    CKDEBUGGERCOMMON_API auto Make_ProviderChip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // SectionHeaderStyle: inspector + overlay section headers.
    // `InToolTip` is optional — empty (the default) attaches no tooltip at all, so an existing
    // three-argument call renders exactly as it did.
    CKDEBUGGERCOMMON_API auto Make_SectionHeader(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone,
        const FText& InToolTip = FText::GetEmpty()) -> TSharedRef<SWidget>;

    // EntityIdStyle: pure text composition for every SCkDebug_EntityRef site. InCleanName is
    // already run through ck::DebugNameClean; InIdText is the canonical "ID|Version(Raw)" string.
    CKDEBUGGERCOMMON_API auto Make_EntityIdText(
        const FCkDebuggerStyleSelection& InSelection,
        const FString& InCleanName,
        const FString& InIdText) -> FText;

    // MergeCountDisplay: the xN affordance on a merged row. Null only when there is nothing merged
    // (InCount <= 1) — that is DATA, not an axis, so callers still skip the slot entirely.
    // The Hidden option is now carried by a collapsed visibility on the returned widget instead of
    // a null return: a collapsed child contributes neither size nor slot padding, so the layout is
    // the same as omitting it, and the option stays live (R1).
    CKDEBUGGERCOMMON_API auto Make_MergeCount(
        const FCkDebuggerStyleSelection& InSelection,
        int32 InCount) -> TSharedPtr<SWidget>;

    // ----- Typography --------------------------------------------------------
    // TextScale. Both read the LIVE selection, like the metric deltas below — a scale is only
    // meaningful against the user's current setting. Bind them through an attribute to stay live;
    // a construct-time call bakes the size, which is exactly why the five registered
    // CkDebugger.Text.* styles cannot follow this axis (an FTextBlockStyle bakes its font at
    // style-set creation). Text that must follow TextScale composes through ScaledFont instead.

    /** The multiplier TextScale applies on top of every CkStyle FontSize* role: 1.0 / 0.875 / 1.125. */
    CKDEBUGGERCOMMON_API auto Get_TextScale() -> float;

    /** A CkStyle role size with TextScale applied, rounded to whole points and clamped at 1. */
    CKDEBUGGERCOMMON_API auto Get_ScaledFontSize(int32 InRoleSize) -> int32;

    /**
     * The scaled counterpart of CkStyle::RegularFont / BoldFont / MonoFont — same face names
     * ("Regular" / "Bold" / "Mono"), same role sizes, plus the axis.
     */
    CKDEBUGGERCOMMON_API auto ScaledFont(const ANSICHAR* InFace, int32 InRoleSize) -> FSlateFontInfo;

    // ----- Brushes -----------------------------------------------------------
    // CornerStyle. Every brush here is registered ONCE in FCkDebuggerStyle or CkStyle (R7) — a call
    // site must never allocate one. Bind through .BorderImage_Lambda to keep the axis live.
    CKDEBUGGERCOMMON_API auto Get_ChipBrush()  -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_BadgeBrush() -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_CardBrush()  -> const FSlateBrush*;

    // SurfaceElevation, for the COMMON surface widgets (card body, window chrome strips, inspector
    // header, labeled group, expandable column). Depth 0 is the window ground and stays an opaque
    // BgRoot fill under every option — a transparent root would show editor chrome through the
    // debugger. Depth 1 is a header/strip, 2 a body, 3+ an inset.
    //
    // The tint is whatever the BRUSH paints: the fill under Layered / Flat, the ring under Outlined
    // (whose fill is transparent at any tint). A Slate border carries one tint, so the two options
    // are expressed by swapping the brush, not by adding a second layer.
    //
    // Flat collapses the nested fills onto one tier rather than growing per-surface hairlines —
    // boundary weight stays the SeparatorWeight axis' business.
    CKDEBUGGERCOMMON_API auto Get_SurfaceBrush(int32 InDepth) -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_SurfaceTint(int32 InDepth)  -> FLinearColor;

    // RowBanding for list / tree surfaces. Under Zebra the brush is the row's full-bleed fill and
    // the rule thickness is zero; under Hairline the brush is the 1px rule the caller draws along
    // the row's bottom edge at Get_RowBandingRuleThickness() (which honours SeparatorWeight, so
    // SeparatorWeight None silences the banding too). Off draws neither.
    CKDEBUGGERCOMMON_API auto Get_RowBandingBrush(int32 InRowIndex) -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_RowBandingRuleThickness() -> float;

    // GraphNodeStyle, expressed as a modulation of the two node roles the SM / GOAP graph-node
    // widgets already read. Card is a byte-identical pass-through of the role values.
    CKDEBUGGERCOMMON_API auto Get_NodeBorderThickness() -> float;
    CKDEBUGGERCOMMON_API auto Get_NodeInactiveOpacity() -> float;

    // EntityRefStyle — the box SCkDebug_EntityRef draws around its label. Flat is the treatment the
    // widget ships today (no box); Pill fills, OutlinePill rings, Monochrome drops the accent color
    // for the dim text role. Which accent the widget hands over is ITS business (a hash-derived hue
    // under the tinted treatments, the EntityId role otherwise) — these only shape it.
    CKDEBUGGERCOMMON_API auto Get_EntityRefBrush()   -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_EntityRefPadding() -> FMargin;
    CKDEBUGGERCOMMON_API auto Get_EntityRefFill(const FLinearColor& InAccent) -> FLinearColor;
    CKDEBUGGERCOMMON_API auto Get_EntityRefInk(const FLinearColor& InAccent)  -> FLinearColor;

    /** True for the treatments whose accent is the entity's own hash hue rather than the EntityId role. */
    CKDEBUGGERCOMMON_API auto EntityRef_UsesHashTint() -> bool;

    /**
     * The hover ink of a CLICKABLE entity reference: the resting ink lifted toward white by the
     * palette's hover strength, alpha preserved. Pure and axis-independent on purpose — it is the
     * seam the spec asserts against, since a Slate hover cannot be faked in an automation test.
     */
    CKDEBUGGERCOMMON_API auto Get_EntityRefHoverInk(const FLinearColor& InRestInk) -> FLinearColor;

    // IconTreatment — the backdrop SCkDebug_Icon composes behind its glyph. Plain draws nothing and
    // costs no padding, which is what keeps the default glyph geometry unchanged. InAccent is the
    // caller's tint; a zero-alpha accent means "unset" and falls back to the muted text role.
    CKDEBUGGERCOMMON_API auto Get_IconBackdropBrush()   -> const FSlateBrush*;
    CKDEBUGGERCOMMON_API auto Get_IconBackdropPadding() -> FMargin;
    CKDEBUGGERCOMMON_API auto Get_IconBackdropTint(const FLinearColor& InAccent) -> FLinearColor;

    // ----- Metrics -----------------------------------------------------------
    // RowDensity: padding for tree rows, inspector rows, overlay card rows.
    CKDEBUGGERCOMMON_API auto Get_RowPadding(const FCkDebuggerStyleSelection& InSelection) -> FMargin;

    // IconSize: Small 12, Medium 16, Large 20.
    CKDEBUGGERCOMMON_API auto Get_IconSize(const FCkDebuggerStyleSelection& InSelection) -> float;

    // SeparatorWeight: None 0, Hairline 1, Standard 2, Heavy 3. Zero means "draw nothing".
    CKDEBUGGERCOMMON_API auto Get_SeparatorThickness(const FCkDebuggerStyleSelection& InSelection) -> float;

    // ----- Metric deltas -----------------------------------------------------
    // The metric axes apply as a DELTA on a surface's own base geometry, never as an absolute.
    // Surfaces deliberately disagree on absolute spacing and glyph size (the on-screen overlay
    // card is far denser than the ECS tree, the launcher rail's glyphs are larger than the axis'
    // own 12/16/20 scale) — only the OFFSET between options is the axis' business. The default
    // option yields a zero delta, so under Classic every surface renders exactly as it shipped.
    // Row padding is clamped at zero so Compact can never produce negative margins; icon size is
    // clamped at 1 so Small can never collapse a glyph box.
    //
    // Unlike the render/metric/predicate families above, these three read the LIVE selection
    // instead of taking one: a delta is only meaningful against the user's current setting, and
    // every call site was already passing `UCkDebuggerStyleSettings::Get_Selection()`.
    CKDEBUGGERCOMMON_API auto Apply_RowDensity(const FMargin& InBase) -> FMargin;
    CKDEBUGGERCOMMON_API auto Apply_IconSize(float InBase) -> float;

    // SSeparator::Thickness is a construction-time argument with no setter, so SeparatorWeight is
    // carried by an SBox height override wrapped around a 1px rule. The None option collapses the
    // box so its slot padding goes with it, rather than reserving space for a zero-height rule.
    CKDEBUGGERCOMMON_API auto Make_AxisSeparator() -> TSharedRef<SWidget>;

    // ----- Predicates --------------------------------------------------------
    // EditControlStyle: the two questions a row builder asks before composing an edit affordance.
    // False from EditControls_AreVisible means the row must fall back to its READ-ONLY form — the
    // control is not composed and then hidden, it is never composed.
    CKDEBUGGERCOMMON_API auto EditControls_AreVisible(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto EditControls_RevealOnHover(const FCkDebuggerStyleSelection& InSelection) -> bool;

    // ----- TreeComplexity ----------------------------------------------------
    // The ECS entity tree's structural declutter dial. Like the metric deltas above, these
    // MODULATE the surface's own base values (the project's fold threshold, its fold toggle,
    // its badge cap) instead of dictating absolutes — the tree owns what "5 siblings" or
    // "6 badges" means, the axis only owns the offset between levels. Normal is a no-op on
    // every one of them, which is the regression bar: an untouched axis renders the tree
    // exactly as it shipped.

    // Scale applied to the tree's sibling-group threshold. Below 1 the tree coalesces sooner.
    // Always > 0; the caller still clamps the product to its own minimum group size.
    CKDEBUGGERCOMMON_API auto Tree_FoldThresholdMultiplier(const FCkDebuggerStyleSelection& InSelection) -> float;

    // True when internal / technical entities must be presented as plain rows — the fold pass
    // is skipped entirely rather than folding and then revealing. Full only.
    CKDEBUGGERCOMMON_API auto Tree_ShowsInternalRows(const FCkDebuggerStyleSelection& InSelection) -> bool;

    // False when sibling runs must never coalesce into a synthetic group row, whatever the
    // project setting says. Full only.
    CKDEBUGGERCOMMON_API auto Tree_GroupsSiblings(const FCkDebuggerStyleSelection& InSelection) -> bool;

    // True when rows that carry a DERIVED name (no debug name of their own) should coalesce by
    // that derived name instead of by their archetype — the "N nameless probes" declutter.
    // Minimal only; implies Tree_GroupsSiblings.
    CKDEBUGGERCOMMON_API auto Tree_GroupsUnnamedRows(const FCkDebuggerStyleSelection& InSelection) -> bool;

    // The per-row badge budget, modulated from the caller's own base cap. Always >= 1.
    CKDEBUGGERCOMMON_API auto Tree_BadgeCap(const FCkDebuggerStyleSelection& InSelection, int32 InBaseCap) -> int32;

    CKDEBUGGERCOMMON_API auto Legend_IsVisible(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Legend_IsDeduped(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Values_UseAlignedColumns(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Values_AlignRight(const FCkDebuggerStyleSelection& InSelection) -> bool;
}

// ====================================================================================================================
// DOMAIN RAMPS — the three color scales that previously lived as hardcoded per-module blocks
// (Scheduler's timing heat, Eqs's candidate score gradient, the ECS inspector-filter categorical
// palette). Every stop is derived from a CkStyle:: role, so a palette edit moves all of them and
// no consumer ever writes a hex literal for "how hot / how good / which bucket".
//
// Semantic canvas colors that only mean something inside ONE visualization (AStar grid cell states,
// Crowd navmesh paint, Map fog) deliberately stay local to their module.
//
// Every entry point is TOTAL: ramps clamp (and treat NaN as 0), the categorical index wraps, so a
// caller can never produce an undefined color.
// ====================================================================================================================

namespace ck::debug_axes
{
    /**
     * Cold → hot ramp over a normalized [0,1] cost/time/pressure value.
     * Stops: Ok (0.0) → Warn (0.5) → Err (1.0); the 0.75 midpoint is the amber-orange the
     * Scheduler's four-band Get_TimingColor used to hardcode.
     * Values outside [0,1] clamp; NaN reads as 0 (coldest).
     */
    CKDEBUGGERCOMMON_API auto Get_HeatColor(float InNormalized) -> FLinearColor;

    /**
     * Low → high quality ramp over a normalized [0,1] score.
     * Stops: Info (0.0, blue) → Ok (1.0, green) — the Eqs candidate gradient.
     * Values outside [0,1] clamp; NaN reads as 0 (worst).
     */
    CKDEBUGGERCOMMON_API auto Get_ScoreColor(float InNormalized) -> FLinearColor;

    /**
     * Deterministic bucket color for "these things are different, not better or worse"
     * (inspector badges, group accents, lane tints). Drawn from the palette's six Category
     * roles plus Accent + Relationship — eight hues chosen to stay distinguishable side by side.
     * The index wraps (negatives included), so any int is a valid argument.
     */
    CKDEBUGGERCOMMON_API auto Get_CategoricalColor(int32 InIndex) -> FLinearColor;

    /** Same palette, keyed by a stable name instead of a caller-managed index. */
    CKDEBUGGERCOMMON_API auto Get_CategoricalColor(FName InKey) -> FLinearColor;

    /** Number of distinct entries before Get_CategoricalColor wraps. */
    CKDEBUGGERCOMMON_API auto Get_CategoricalPaletteSize() -> int32;
}

// ====================================================================================================================

/**
 * A named, curated point in axis space. Applying a profile copies Selection into the settings and
 * saves; any manual axis edit afterwards flips the displayed name to "Custom".
 */
struct CKDEBUGGERCOMMON_API FCkDebuggerStyleProfile
{
    FString Name;
    FString Blurb;
    FCkDebuggerStyleSelection Selection;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_axes
{
    // Registry order is UI order. Entry 0 is always "Classic" and always equals a default-constructed
    // selection — that identity is what makes "reset" and "defaults" the same operation.
    CKDEBUGGERCOMMON_API auto Get_StyleProfiles() -> const TArray<FCkDebuggerStyleProfile>&;
}

// ====================================================================================================================
