#pragma once

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

// Editor-only by construction: every check below reads editor-side asset state over a walked editor world. The
// module ships in packaged Development/DebugGame builds, where there is no such world — so the declarations are
// absent there rather than present and empty, and the one caller (`Run_Scan`) is behind the same guard.
#if WITH_EDITOR

/** Lighting checks: what the level costs to light — dynamic light population and lightmap resolution overrides. */
namespace ck_optimization_debugger_checks_lighting
{
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Checks(
        const FCkOptimizationDebugger_ScanContext& InContext,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        TArray<FCkOptimizationDebugger_FindingRow>& OutFindings) -> void;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
