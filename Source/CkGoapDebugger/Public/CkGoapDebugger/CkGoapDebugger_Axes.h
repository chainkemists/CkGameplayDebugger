#pragma once

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

// ====================================================================================================================
// Mission Control's adapter onto the suite style axes. Every GOAP panel composes through these so a
// single axis flip in Editor Preferences -> Ck -> Debugger Style moves the whole window at once.
//
// RowDensity is applied as a DELTA on each surface's own base padding — the same rule the ECS
// inspector and entity tree use. The absolute metric belongs to the surface (Mission Control rows
// are deliberately denser than an editor tree, and Get_RowPadding's absolutes would blow that out);
// the OFFSET between options belongs to the axis. Comfortable => zero delta => today's rows.
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
}

// ====================================================================================================================
