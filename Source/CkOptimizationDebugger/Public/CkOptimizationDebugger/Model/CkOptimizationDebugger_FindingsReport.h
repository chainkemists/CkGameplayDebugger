#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** What a findings export carries besides the findings — the things a reader of the file, days later and on another
 *  machine, needs in order to know what they are looking at. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ReportContext
{
    FString ProjectName;

    /** Which levels the level scan covered, already sorted by the scan. */
    TArray<FString> ScannedLevelNames;

    /** How many assets the project scan read, and how many of those it had to open. Zero and zero when no project
     *  scan has run — which the report SAYS rather than leaving the reader to assume it covered everything. */
    int32 ProjectAssetCount = 0;
    int32 ProjectDeepLoadedCount = 0;

    /** Suppressed findings are NOT in the export's list, so the count of them is, and the report says so. A report
     *  that silently omitted them would be a report a teammate could not reconcile with their own. */
    int32 SuppressedCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** The findings export — the module's long-standing known gap, landing WITH its determinism spec as the doctrine
 *  required of it.
 *
 *  Both builders are PURE and deterministic by construction: the timestamp is handed in (the caller reads the
 *  clock, exactly as the snapshot report does), every list is explicitly sorted with a final tie-break, and nothing
 *  in the document is pointer- or environment-derived. Same findings + same context + same timestamp is a
 *  byte-identical file, which is what makes two exports diffable — and what
 *  `Ck.OptimizationDebugger.Report.Determinism` pins. */
namespace ck_optimization_debugger_findings_report
{
    /** One self-contained HTML file: no external stylesheet, no script, no images, so it survives being attached to
     *  a ticket or opened from a network share. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FindingsReportHtml(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
        const FCkOptimizationDebugger_ReportContext& InContext,
        const FDateTime& InGeneratedAt) -> FString;

    /** The same content as Markdown, for a pull request description or a wiki page. Not a lossy summary of the
     *  HTML — the two are built from the same sorted projection, so a claim in one is a claim in the other. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FindingsReportMarkdown(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
        const FCkOptimizationDebugger_ReportContext& InContext,
        const FDateTime& InGeneratedAt) -> FString;

    /** The findings in report order: worst severity first, then furthest over budget, then check id, then stable
     *  key as the final tie-break. Exposed because both builders read it and a spec pins it directly — an order
     *  that depended on the incoming array's order would make two exports of one scan disagree. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ReportOrder(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> TArray<FCkOptimizationDebugger_FindingRow>;
}

// --------------------------------------------------------------------------------------------------------------------
