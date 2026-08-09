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

UENUM(BlueprintType)
enum class ECkDebugAxis_EntityIdStyle : uint8
{
    NameAndId,
    CompactId,
    NameOnly,
    HashTintedChip,
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
        meta = (ToolTip = "How an entity reference composes its display text on every SCkDebug_EntityRef site."))
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
};

// ====================================================================================================================
