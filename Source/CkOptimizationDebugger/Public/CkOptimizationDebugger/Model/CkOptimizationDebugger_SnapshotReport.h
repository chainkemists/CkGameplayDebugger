#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** The shareable artifact: one self-contained HTML file per snapshot — the picture, the false-coloured ID map, the
 *  whole-view totals and the per-mesh table, with no external assets so it survives being attached to a ticket. */
namespace ck_optimization_debugger_snapshot_report
{
    /** Pure, and deterministic by construction: the timestamp is handed IN (the caller reads the clock, exactly as
     *  the model's scan times work) and everything else derives from the snapshot. Same snapshot + same timestamp
     *  is byte-identical output — `Ck.OptimizationDebugger.Snapshot.ReportDeterminism` pins it, which is the spec
     *  the module doctrine required the report export to arrive with. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_SnapshotReportHtml(
        const FCkOptimizationDebugger_Snapshot& InSnapshot,
        const FDateTime& InGeneratedAt) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
