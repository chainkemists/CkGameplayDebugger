#pragma once

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "Misc/Attribute.h"

// ====================================================================================================================
// Mission Control's adapter onto the suite style axes. Every GOAP panel composes through these so a
// single axis flip in Editor Preferences -> Ck -> Debugger Style moves the whole window at once.
//
// RowDensity is applied as a DELTA on each surface's own base padding — the same rule the ECS
// inspector and entity tree use. The absolute metric belongs to the surface (Mission Control rows
// are deliberately denser than an editor tree, and Get_RowPadding's absolutes would blow that out);
// the OFFSET between options belongs to the axis. Comfortable => zero delta => today's rows.
//
// LIVE-NESS (R1). The `Get_*` / `Apply_*` entry points return VALUES — a caller that hands one
// straight to a Slate argument bakes the axis at construction and stops responding to a profile
// flip. Prefer the `Live_*` companions wherever the Slate argument is a TAttribute (which
// `.Padding` is on every slot, SBorder, and SButton::ContentPadding); reach for the value form only
// when composing something Slate cannot re-evaluate (arithmetic feeding a layout pass, a font
// measurement). `Make_Chip` / `Make_Badge` need no companion: the common helpers they forward to are
// live by construction, so the emitted widget re-reads the axis at paint time.
// ====================================================================================================================

namespace ck_goap_debugger_axes
{
    inline auto Get_Selection() -> const FCkDebuggerStyleSelection&
    {
        return UCkDebuggerStyleSettings::Get_Selection();
    }

    inline auto Apply_RowDensity(const FMargin& InBase) -> FMargin
    {
        const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_RowPadding(Get_Selection());

        const auto DeltaX = Current.Left - Baseline.Left;
        const auto DeltaY = Current.Top  - Baseline.Top;

        return FMargin
        {
            FMath::Max(0.0f, InBase.Left   + DeltaX),
            FMath::Max(0.0f, InBase.Top    + DeltaY),
            FMath::Max(0.0f, InBase.Right  + DeltaX),
            FMath::Max(0.0f, InBase.Bottom + DeltaY)
        };
    }

    /** The attribute form of Apply_RowDensity — bind this into `.Padding` / `.ContentPadding`. */
    inline auto Live_RowDensity(const FMargin& InBase) -> TAttribute<FMargin>
    {
        return TAttribute<FMargin>::CreateLambda([InBase]() -> FMargin
        {
            return Apply_RowDensity(InBase);
        });
    }

    inline auto Get_IconSize() -> float
    {
        return ck::debug_axes::Get_IconSize(Get_Selection());
    }

    /** Status dots / satisfaction squares — a fixed fraction of the icon metric so they track it. */
    inline auto Get_DotSize() -> float
    {
        return Get_IconSize() * 0.5f;
    }

    inline auto Get_SeparatorThickness() -> float
    {
        return ck::debug_axes::Get_SeparatorThickness(Get_Selection());
    }

    inline auto Make_Chip(const FText& InText, ECk_Tone InTone) -> TSharedRef<SWidget>
    {
        return ck::debug_axes::Make_Chip(Get_Selection(), InText, InTone);
    }

    inline auto Make_Badge(const FText& InText, ECk_Tone InTone) -> TSharedRef<SWidget>
    {
        return ck::debug_axes::Make_Badge(Get_Selection(), InText, InTone);
    }

    // ----- GraphNodeStyle ----------------------------------------------------
    // Expressed as SCALES on each surface's own baked geometry rather than absolutes, for the same
    // reason RowDensity is a delta: the GOAP action card, the SM state pill and the sub-SM bubble
    // each picked their border weight and inner padding against their own content, and the axis only
    // owns the OFFSET between options. Card divides the role by itself => 1.0 => today's cards
    // byte-identical (R6).

    /** Get_NodeBorderThickness() as a multiple of its own role value. Card 1.0. */
    inline auto Get_NodeBorderScale() -> float
    {
        const auto Role = CkStyle::NodeBorderThickness();
        if (Role <= KINDA_SMALL_NUMBER) { return 1.0f; }
        return ck::debug_axes::Get_NodeBorderThickness() / Role;
    }

    /** Get_NodeInactiveOpacity() as a multiple of its own role value — the dim/fade scale. Card 1.0. */
    inline auto Get_NodeDimScale() -> float
    {
        const auto Role = CkStyle::NodeInactiveOpacity();
        if (Role <= KINDA_SMALL_NUMBER) { return 1.0f; }
        return ck::debug_axes::Get_NodeInactiveOpacity() / Role;
    }

    /** Inner card padding scale: Dense tightens, Card / Minimal leave the surface's own value. */
    inline auto Get_NodePaddingScale() -> float
    {
        switch (Get_Selection().GraphNodeStyle)
        {
            case ECkDebugAxis_GraphNodeStyle::Card:    return 1.0f;
            case ECkDebugAxis_GraphNodeStyle::Minimal: return 1.0f;
            case ECkDebugAxis_GraphNodeStyle::Dense:   return 0.6f;
        }

        return 1.0f;
    }

    // No radius companion here on purpose: every GOAP graph surface draws its chrome with the flat
    // WhiteBrush, so there is no corner to scale. The SM adapter carries one because its sub-SM
    // bubble and state halo are rounded-box brushes.

    /**
     * False under Minimal — the node draws its border and its content only, no fill wash. Callers
     * express that by handing a transparent color to the fill layer they already bind, never by
     * building a different widget tree (the node widgets are not rebuilt on an axis flip).
     */
    inline auto Get_NodeDrawsFill() -> bool
    {
        return Get_Selection().GraphNodeStyle != ECkDebugAxis_GraphNodeStyle::Minimal;
    }
}

// ====================================================================================================================
