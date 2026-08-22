#pragma once

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Analysis/CkPerfLab_SessionCompare.h"
#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Turning a session into a file somebody else can read.
//
// Every builder here is PURE and deterministic by construction: the timestamp is handed in rather
// than read from the clock, every list is explicitly sorted with a final tie-break, and every number
// is formatted through fmt's locale-independent path. Same session + same budget + same timestamp is
// a byte-identical file, which is what makes two exports diffable — and what
// `Ck.PerfLab.Export.Determinism` pins.
//
// Nothing here reads the world, the filesystem, or any global. Writing the file is the caller's job.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab::exporter
{
    /** Which CSV table a caller wants. Six fixed tables, fixed header rows, sorted rows. */
    enum class ECk_CsvTable : uint8
    {
        SessionInfo,
        Positions,
        Directions,
        Findings,
        Contributors,
        ScoreComponents,
    };

    /** Every table, in the order a sectioned single-file export writes them. */
    CKPERFLAB_API auto
    Get_AllCsvTables() -> TArray<ECk_CsvTable>;

    CKPERFLAB_API auto
    Get_CsvTableName(
        ECk_CsvTable InTable) -> FString;

    /**
     * One self-contained HTML file: no external stylesheet, no script, no images, so it survives
     * being attached to a ticket or opened from a network share.
     *
     * Carries the limitation paragraph verbatim. The report is the copy of this tool most likely to
     * be read by somebody who never ran it, and therefore the copy least able to afford implying
     * that a "likely contributor" was measured to be the cause.
     */
    CKPERFLAB_API auto
    Build_SessionHtml(
        const FCk_PerfLab_Session& InSession,
        const FCk_PerfLab_Analysis& InAnalysis,
        const FDateTime& InGeneratedAt) -> FString;

    /** One CSV table. Header row fixed, rows sorted, cells RFC-4180 quoted. */
    CKPERFLAB_API auto
    Build_SessionCsv(
        const FCk_PerfLab_Session& InSession,
        const FCk_PerfLab_Analysis& InAnalysis,
        ECk_CsvTable InTable) -> FString;

    /** All six tables in one file, each under a `# <name>` section line and separated by a blank line. */
    CKPERFLAB_API auto
    Build_SessionCsvBundle(
        const FCk_PerfLab_Session& InSession,
        const FCk_PerfLab_Analysis& InAnalysis) -> FString;

    /**
     * The session as JSON, with the analysis attached.
     *
     * Deliberately delegates the session half to the codec rather than re-serialising it: one schema
     * with two writers drifts, and the drift shows up as an export that no longer round-trips
     * through the reader that consumes it.
     */
    CKPERFLAB_API auto
    Build_SessionJson(
        const FCk_PerfLab_Session& InSession,
        const FCk_PerfLab_Analysis& InAnalysis,
        const FDateTime& InGeneratedAt) -> FString;

    /** A comparison as HTML, carrying its refusal or its warning banners. */
    CKPERFLAB_API auto
    Build_CompareHtml(
        const FCk_PerfLab_Compare& InCompare,
        const FDateTime& InGeneratedAt) -> FString;

    /** The limitation paragraph every export carries. Pinned by spec — the wording IS the mitigation. */
    CKPERFLAB_API extern const TCHAR* const k_LimitationParagraph;
}

// --------------------------------------------------------------------------------------------------------------------
