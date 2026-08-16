#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** What one cleanup pass over `/Game` found, and what it could not say.
 *
 *  `RequiresEditor` and `WasStillIndexing` are on the record for the same reason the disk breakdown's flags are:
 *  "nothing to clean up", "nobody asked" and "the registry was still indexing so this is a floor" are three different
 *  statements, and an empty row list would be indistinguishable from all three. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupScanResult
{
    TArray<FCkOptimizationDebugger_CleanupRow> Rows;

    // No editor session, or no asset registry — the module ships in packaged Development/DebugGame builds, where the
    // registry describes a cooked set that answers a different question.
    bool RequiresEditor = false;

    // The reader pressed Cancel on the progress dialog. What was gathered before that point is kept, exactly as the
    // level scan keeps a partial gather.
    bool WasCancelled = false;

    // The asset registry was still indexing, so every count here is a floor rather than a total.
    bool WasStillIndexing = false;

    // Packages the on-disk graph says nothing references, but a registered external-reference provider does — an
    // AngelScript generated accessor, today. They are NOT unreferenced and are deliberately absent from `Rows`.
    //
    // The COUNT is on the record rather than the rows being silently dropped: a row that vanished would make this tab
    // disagree with the Reference Viewer with nothing on screen explaining why, which is the same reason an excluded
    // level is greyed rather than removed. This is how the page says the scan looked at them.
    int32 ExternallyReferencedCount = 0;

    // Which providers claimed at least one of those, sorted, so the status strip can name them rather than saying
    // "something" referenced them.
    TArray<FName> ExternalReferenceSourceIds;

    // Whether ANY provider was registered to ask. False means this pass could not consider script or config
    // references AT ALL — not that it considered them and found none. Reporting an unreferenced count without saying
    // which of the two happened is the defect `RequiresEditor` and `IsAvailable` exist to prevent elsewhere.
    bool HasExternalReferenceProvider = false;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup
{
    /**
     * Everything under `/Game` that is worth a second look before a milestone: assets nothing references, possible
     * duplicates, redirectors, and packages with unsaved changes.
     *
     * **Nothing here is an instruction.** Every row is presented for review, and the actions that can remove an asset
     * route through the engine's own confirmation dialog — see the module CLAUDE.md's action-safety contract.
     *
     * Asset-registry driven and independent of the level scan in both directions: a level scan does not refresh these
     * rows and this pass does not touch the findings. Three of the four categories read serialized registry data only
     * and load nothing; the dirty-package category is the exception and reads live editor state, which is why that
     * one is only ever as current as the last press of the button.
     *
     * Outside the editor this returns `RequiresEditor == true` and no rows.
     */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_CleanupScan() -> FCkOptimizationDebugger_CleanupScanResult;

    /**
     * The always-rooted class names the unreferenced pass never reports, in a stable order.
     *
     * Deliberately conservative and deliberately short. A level is referenced by the project's map settings rather
     * than by a package, a data asset can be named from config or from code, and a primary asset label exists to be
     * discovered rather than referenced — none of which the on-disk reference graph can see. Reporting those as
     * "nothing references this" would be the tool asking a reader to delete the thing the project boots from.
     *
     * Matching is on the asset's own class NAME (`FTopLevelAssetPath::GetAssetName`) and on nothing else: no class is
     * loaded to answer, exactly as the disk breakdown's classification refuses to.
     */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AlwaysRootedClassNames() -> TArray<FString>;

    /** Whether a class name is in that list. Pure, so the exclusion rule is testable without an asset registry. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Is_AlwaysRootedClassName(
        const FString& InClassName) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
