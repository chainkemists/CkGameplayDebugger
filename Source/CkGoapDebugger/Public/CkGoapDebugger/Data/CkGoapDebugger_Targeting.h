#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// GOAP-debugger entity targeting — the ONE definition of "an entity this
// debugger lists". Shared by the module's FCkDebug_EntityTargetRoute (Open In /
// Sync from ECS), and the window's specialized viewport picker.
// ====================================================================================================================

namespace ck_goap_debugger
{
    // True for entities the GOAP debugger lists in its roster: an entity
    // carrying a RecordOfGoapPlanners with at least one registered Planner, or
    // an Add-stamped Planner owner that is neither an Action definition nor a
    // registered child Planner of a recorded owner.
    CKGOAPDEBUGGER_API auto IsGoapRosterEntity(const FCk_Handle& InCandidate) -> bool;

    // Closest exact / ancestor / descendant roster entity for an arbitrary
    // selection (lineage-aware). Invalid when the selection has no GOAP in its
    // lineage.
    CKGOAPDEBUGGER_API auto ResolveGoapTarget(const FCk_Handle& InSelected) -> FCk_Handle;
}

// ====================================================================================================================
