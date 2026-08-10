#pragma once

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"

// Pure pre-render budget for the non-interactive focus card.  This is deliberately
// applied before Slate so important diagnostics cannot be hidden solely by clipping.
struct FCk_DebugOverlay_FocusCardBudget
{
    int32 MaxRowsPerSection = 4;
    int32 MaxRowsTotal      = 18;
};

namespace ck_debugoverlay
{
    // THE focus-card section ordering: protected AI / navigation providers first, generic
    // attribute floods last, then SortPriority, then SourceOrder (a stable tie-break — subtree
    // aggregation emits several equal-priority sections per provider and an unstable sort over
    // them reshuffles the card every tick). Shared so every pre-render trim — the budget and
    // the distance LOD — rations against the same protected ordering.
    auto Sort_SectionsForBudget(
        TArray<FCk_DebugOverlay_Section>& InOutSections) -> void;

    // Rations rows against the budget. OmittedRowCount ACCUMULATES: a section may already
    // carry omissions from an earlier trim (distance LOD), and the card renders the sum as
    // that section's "+N more".
    auto Apply_FocusCardBudget(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_FocusCardBudget& InBudget) -> FCk_DebugOverlay_EntityModel;
}
