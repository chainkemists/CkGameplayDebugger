#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerStyleSelection.generated.h"

// ====================================================================================================================
// Layer B of the debugger theme model: COLORLESS per-axis style choices.
//
// An axis says WHICH shape/density/treatment a surface uses — never which color. Colors always
// resolve through CkStyle:: palette roles at render time, so palette (Layer A) and selection
// (Layer B) stay orthogonal.
//
// Option order == the axis catalog order in `_Design/2026-08-09-style-lab.md`; the value 0 entry
// is the axis DEFAULT. Ini stores enum entries by NAME, so an unknown/removed name falls back to
// the property default — clamp-and-default on load comes for free. Any add/remove/reorder here
// bumps UCkDebuggerStyleSettings::SchemaVersion.
// ====================================================================================================================

UENUM(BlueprintType)
enum class ECkDebugAxis_ChipStyle : uint8
{
    Tint,
    Solid,
    Outline,
    TextOnly,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_RowDensity : uint8
{
    Comfortable,
    Compact,
    Airy,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * What TEXT an entity reference composes — composition only, never treatment. The box it is drawn
 * in belongs to EntityRefStyle; a config written before the two were separated names an option
 * that no longer exists here and falls back to NameAndId.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_EntityIdStyle : uint8
{
    NameAndId,
    CompactId,
    NameOnly,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_BadgeStyle : uint8
{
    Solid,
    Hollow,
    CountOnly,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_SeparatorWeight : uint8
{
    Hairline,
    None,
    Standard,
    Heavy,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_FoldChipStyle : uint8
{
    Chip,
    Text,
    Minimal,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_ValueAlignment : uint8
{
    Left,
    Right,
    AlignedColumns,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_ProviderChipStyle : uint8
{
    Tint,
    Solid,
    AbbrevOnly,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_LegendMode : uint8
{
    Deduped,
    Off,
    PerSection,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_MergeCountDisplay : uint8
{
    SuffixText,
    CountBadge,
    Hidden,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_FlashOnChange : uint8
{
    Off,
    Value,
    Row,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_SectionHeaderStyle : uint8
{
    Uppercase,
    Mixed,
    Minimal,
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECkDebugAxis_IconSize : uint8
{
    Medium,
    Small,
    Large,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * How an inspector row presents its EDIT affordances (numeric fields, switches, action buttons).
 * Hidden restores fully read-only inspectors — the builder emits the plain value row instead of the
 * control, so nothing interactive is composed at all.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_EditControlStyle : uint8
{
    Inline,
    OnHover,
    Hidden,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * How much structure the ECS entity tree presents at once.
 *
 * The axis MODULATES the tree's own project settings (fold toggle, sibling-group threshold,
 * badge cap) — it never replaces them, so the project's values remain the base at every level
 * and Normal is a byte-identical no-op on all of them. Minimal tightens the thresholds and
 * coalesces technical rows harder; Full turns structural folding off so every row is shown
 * plainly.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_TreeComplexity : uint8
{
    Normal,
    Minimal,
    Full,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * The backdrop a glyph sits on. Plain is a bare glyph in its box — the shape every SCkDebug_Icon
 * site ships with. Well and Ring both grow the widget by their own padding, which is why the
 * default has to stay Plain for the layout to be unchanged.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_IconTreatment : uint8
{
    Plain,
    Well,
    Ring,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Global type-size multiplier applied on top of the CkStyle FontSize* roles: Normal x1.0,
 * Small x0.875, Large x1.125. It composes with the palette's own font sizes rather than replacing
 * them, so an Editor Preferences typography edit still moves everything.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_TextScale : uint8
{
    Normal,
    Small,
    Large,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Visual TREATMENT of SCkDebug_EntityRef — the box the reference is drawn in. Orthogonal to
 * EntityIdStyle, which owns only what TEXT the reference composes.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_EntityRefStyle : uint8
{
    Flat,
    Pill,
    OutlinePill,
    Monochrome,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Corner shape for every chip / badge / card surface, resolved through the registered brush
 * selectors so nothing allocates per frame.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_CornerStyle : uint8
{
    Rounded,
    Sharp,
    Pill,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * How much tonal separation nested surfaces get. Layered walks the Bg ladder per depth (today).
 * Flat collapses every nested tier onto one fill. Outlined drops the fills entirely and draws a
 * 1px ring instead.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_SurfaceElevation : uint8
{
    Layered,
    Flat,
    Outlined,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * How a debug graph node presents itself: Card is today's bordered fill, Minimal thins the border
 * and fades inactive nodes harder, Dense thickens the border and keeps inactive nodes readable so
 * a crowded graph still parses.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_GraphNodeStyle : uint8
{
    Card,
    Minimal,
    Dense,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Row-to-row separation in list and tree surfaces. Zebra alternates a fill; Hairline draws a rule
 * under each row at the SeparatorWeight thickness instead.
 */
UENUM(BlueprintType)
enum class ECkDebugAxis_RowBanding : uint8
{
    Off,
    Zebra,
    Hairline,
};

// ====================================================================================================================

/**
 * The complete Layer-B style selection: one option per axis. Default-constructed == the "Classic"
 * profile (spec-enforced). Config serialization is owned by the property that HOLDS this struct
 * (UCkDebuggerStyleSettings::Selection), which is why the members carry no `config` specifier.
 */
USTRUCT(BlueprintType)
struct CKDEBUGGERCOMMON_API FCkDebuggerStyleSelection
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Treatment for inspector badge boxes, feature chips, and overlay field chips."))
    ECkDebugAxis_ChipStyle ChipStyle = ECkDebugAxis_ChipStyle::Tint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Row padding for tree rows, inspector rows, and overlay card rows."))
    ECkDebugAxis_RowDensity RowDensity = ECkDebugAxis_RowDensity::Comfortable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "How an entity reference composes its display TEXT on every SCkDebug_EntityRef site. How it is drawn stays with Entity Ref Style."))
    ECkDebugAxis_EntityIdStyle EntityIdStyle = ECkDebugAxis_EntityIdStyle::NameAndId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Treatment for count badges and fold badges."))
    ECkDebugAxis_BadgeStyle BadgeStyle = ECkDebugAxis_BadgeStyle::Solid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Thickness of section separators in the inspector and the overlay card."))
    ECkDebugAxis_SeparatorWeight SeparatorWeight = ECkDebugAxis_SeparatorWeight::Hairline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Treatment for tree fold / group chips."))
    ECkDebugAxis_FoldChipStyle FoldChipStyle = ECkDebugAxis_FoldChipStyle::Chip;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Alignment of the inspector value column."))
    ECkDebugAxis_ValueAlignment ValueAlignment = ECkDebugAxis_ValueAlignment::Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Treatment for overlay section provider chips."))
    ECkDebugAxis_ProviderChipStyle ProviderChipStyle = ECkDebugAxis_ProviderChipStyle::Tint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Overlay legend strip: deduped across sections, off, or repeated per section."))
    ECkDebugAxis_LegendMode LegendMode = ECkDebugAxis_LegendMode::Deduped;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "How a merged row's xN count renders. WHETHER merging happens stays a project setting."))
    ECkDebugAxis_MergeCountDisplay MergeCountDisplay = ECkDebugAxis_MergeCountDisplay::SuffixText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Inspector value-change flash scope."))
    ECkDebugAxis_FlashOnChange FlashOnChange = ECkDebugAxis_FlashOnChange::Off;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Treatment for inspector and overlay section headers."))
    ECkDebugAxis_SectionHeaderStyle SectionHeaderStyle = ECkDebugAxis_SectionHeaderStyle::Uppercase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Glyph size for every SCkDebug_Icon site: Small 12, Medium 16, Large 20."))
    ECkDebugAxis_IconSize IconSize = ECkDebugAxis_IconSize::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Inspector edit affordances: always inline, revealed on row hover, or hidden (fully read-only inspectors)."))
    ECkDebugAxis_EditControlStyle EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "ECS entity tree declutter dial. Minimal folds technical rows harder and tightens the sibling-group threshold; Full turns folding and grouping off. Modulates the project's own tree settings — it never replaces them."))
    ECkDebugAxis_TreeComplexity TreeComplexity = ECkDebugAxis_TreeComplexity::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Backdrop behind every SCkDebug_Icon glyph: bare, a tinted well, or a thin ring."))
    ECkDebugAxis_IconTreatment IconTreatment = ECkDebugAxis_IconTreatment::Plain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Type-size multiplier applied on top of the palette's font-size roles: Normal x1.0, Small x0.875, Large x1.125."))
    ECkDebugAxis_TextScale TextScale = ECkDebugAxis_TextScale::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Visual treatment of the entity reference pill. What TEXT it composes stays with Entity Id Style."))
    ECkDebugAxis_EntityRefStyle EntityRefStyle = ECkDebugAxis_EntityRefStyle::Flat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Corner shape for chips, badges, and cards."))
    ECkDebugAxis_CornerStyle CornerStyle = ECkDebugAxis_CornerStyle::Rounded;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Tonal separation between nested surfaces: a background ladder per depth, one flat fill, or transparent fills with 1px rings."))
    ECkDebugAxis_SurfaceElevation SurfaceElevation = ECkDebugAxis_SurfaceElevation::Layered;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Border weight and inactive fade for debug graph nodes (state machine, GOAP)."))
    ECkDebugAxis_GraphNodeStyle GraphNodeStyle = ECkDebugAxis_GraphNodeStyle::Card;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style Axes",
        meta = (ToolTip = "Row-to-row separation in list and tree surfaces: none, alternating fills, or a per-row rule."))
    ECkDebugAxis_RowBanding RowBanding = ECkDebugAxis_RowBanding::Off;
};

// ====================================================================================================================
