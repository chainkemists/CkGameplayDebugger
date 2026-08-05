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
    auto Apply_FocusCardBudget(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_FocusCardBudget& InBudget) -> FCk_DebugOverlay_EntityModel;
}
