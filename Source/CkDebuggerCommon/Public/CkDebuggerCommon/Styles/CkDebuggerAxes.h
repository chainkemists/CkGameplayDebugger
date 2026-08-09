#pragma once

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"

#include "CkEditorTools/Style/CkStyle.h"

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

    // ----- Predicates --------------------------------------------------------
    CKDEBUGGERCOMMON_API auto Legend_IsVisible(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Legend_IsDeduped(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Values_UseAlignedColumns(const FCkDebuggerStyleSelection& InSelection) -> bool;
    CKDEBUGGERCOMMON_API auto Values_AlignRight(const FCkDebuggerStyleSelection& InSelection) -> bool;
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
