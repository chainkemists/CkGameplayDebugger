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
    // ChipStyle: inspector badge boxes, feature chips, overlay field chips.
    CKDEBUGGERCOMMON_API auto Make_Chip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // BadgeStyle: count badges, fold badges.
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
    CKDEBUGGERCOMMON_API auto Make_SectionHeader(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone) -> TSharedRef<SWidget>;

    // EntityIdStyle: pure text composition for every SCkDebug_EntityRef site. InCleanName is
    // already run through ck::DebugNameClean; InIdText is the canonical "ID|Version(Raw)" string.
    CKDEBUGGERCOMMON_API auto Make_EntityIdText(
        const FCkDebuggerStyleSelection& InSelection,
        const FString& InCleanName,
        const FString& InIdText) -> FText;

    // MergeCountDisplay: the xN affordance on a merged row. Null when the axis is Hidden or when
    // there is nothing merged (InCount <= 1) — callers skip the slot entirely rather than pad it.
    CKDEBUGGERCOMMON_API auto Make_MergeCount(
        const FCkDebuggerStyleSelection& InSelection,
        int32 InCount) -> TSharedPtr<SWidget>;

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
