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

/** Mesh checks: the mesh assets the level places — triangle budgets, LOD coverage, Nanite fit and collision shape. */
namespace ck_optimization_debugger_checks_mesh
{
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Checks(
        const FCkOptimizationDebugger_ScanContext& InContext,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        TArray<FCkOptimizationDebugger_FindingRow>& OutFindings) -> void;

    /** Whether a material a Nanite mesh renders through fails to declare `Used With Nanite`.
     *
     *  Reads the authored `bUsedWithNanite` flag rather than `CheckMaterialUsage[_Concurrent]`: both usage-check
     *  calls can block on a shader compile, and an audit tool must never make the editor build shaders to describe
     *  an asset. The flag is the authored answer to the same question.
     *
     *  Exported for the same reason `Is_DataTexture` is — the `Mesh.NaniteMaterialIncompatible` FIX has to re-ask
     *  the question before it mutates, and a second copy of the rule in the fix would be a second place to drift. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Is_NaniteIncompatible(
        const UMaterialInterface* InMaterial) -> bool;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
