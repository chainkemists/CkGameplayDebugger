#pragma once

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** What one scan produced, and what it could not reach. Returned as one value rather than an out-parameter pair
 *  because the window needs both to say anything honest on the status strip: "12 findings" over an editor world
 *  that had three unloaded sub-levels is a different sentence from "12 findings" over a fully loaded one. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanResult
{
    TArray<FCkOptimizationDebugger_FindingRow> Findings;

    // Persistent level first, then every loaded sub-level, in world order.
    TArray<FString> ScannedLevelNames;

    // Sub-levels the world knows about but has not loaded. Named rather than skipped silently: saying nothing about
    // a level nobody opened is indistinguishable from finding it clean.
    TArray<FString> SkippedUnloadedLevelNames;

    // The user pressed Cancel in the progress dialog. The findings gathered so far are still returned — a partial
    // answer the reader was told is partial beats throwing away work they waited for.
    bool WasCancelled = false;

    // No editor world was reachable. Outside the editor this is the only outcome; the window says so rather than
    // reporting a clean level.
    bool RequiresEditor = false;

    // The dashboard's whole data source, produced from the SAME gather the checks ran against. A second walk to
    // count actors could disagree with the walk that produced the findings, and a dashboard that contradicts its own
    // list is worse than a dashboard with fewer numbers on it.
    FCkOptimizationDebugger_ScanSummary Summary;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_scan
{
    /** Walks the persistent level and every LOADED streaming sub-level of `InEditorWorld` the user has not excluded,
     *  gathers what the checks need once, runs every check family against it, and folds the same gather into the
     *  dashboard's census.
     *
     *  Synchronous, game-thread, behind a cancelable `FScopedSlowTask`. A scan is a question the user asked and is
     *  waiting on the answer to; a background pass that lands three seconds after they moved on would be answering
     *  a question about a level they have since changed.
     *
     *  `InExcludedLevelNames` holds level package short names the scan must SKIP WHOLE — neither gathered nor
     *  checked. Filtering an excluded level out of the findings afterwards would still pay for its walk and would
     *  still fold its actors into the census, which is the opposite of what excluding it means. Excluded levels are
     *  still LISTED in `Summary.Levels`, greyed, so a narrowed scope never looks like a smaller project.
     *
     *  Outside the editor this returns `RequiresEditor` with no findings — the module ships in packaged
     *  Development/DebugGame builds, where there is no editor world to walk. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Scan(
        UWorld* InEditorWorld,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        const TSet<FName>& InExcludedLevelNames) -> FCkOptimizationDebugger_ScanResult;

    /** The editor world the Scan command runs against, or null outside the editor. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_EditorWorld() -> UWorld*;
}

// --------------------------------------------------------------------------------------------------------------------
