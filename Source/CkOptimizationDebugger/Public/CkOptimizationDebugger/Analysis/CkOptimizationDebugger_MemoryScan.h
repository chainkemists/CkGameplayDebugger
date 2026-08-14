#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** What one pass over the resident content found, and whether the streaming metrics inside it mean anything.
 *
 *  The two travel together because a reader cannot interpret an empty streaming column without the reason for it —
 *  and the reason is a property of the SESSION the scan ran in, not of any one row. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_MemoryScanResult
{
    TArray<FCkOptimizationDebugger_MemoryRow> Rows;

    ECkOptimizationDebugger_StreamingAvailability StreamingAvailability =
        ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_memory
{
    /**
     * Everything currently RESIDENT, in three tables: textures, render targets and static meshes.
     *
     * Deliberately independent of the level scan in both directions. "What does this level cost" and "what is loaded
     * right now" are different questions — the editor holds thumbnails, the content browser's previews and whatever
     * the last PIE session dragged in, none of which is in the persistent level, and all of which is really costing
     * memory. So this walks the live object graph (`TObjectIterator`) rather than the world, and never re-runs
     * itself: it is an explicit user action, exactly like Scan.
     *
     * **Nothing is loaded and nothing is built.** Only already-resident objects are visited, and every accessor used
     * reads state the object already has — see the module CLAUDE.md for the two texture accessors that were chosen
     * specifically because they do not stall on an in-flight texture build.
     *
     * Unlike the level scan this is NOT editor-only: `TObjectIterator` and the streaming manager are both runtime
     * facilities, so the memory page answers in a packaged Development/DebugGame build too.
     */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_MemoryScan() -> FCkOptimizationDebugger_MemoryScanResult;

    /**
     * Whether per-texture streaming metrics can be measured in this session, and if not, which of the two reasons
     * applies. Asked through `IStreamingManager::Get_Concurrent()` — the accessor that can return null — never
     * `Get()`, which lazily CONSTRUCTS a streaming manager collection on first call. An analysis tool that brought a
     * subsystem into existence in order to describe it would be reporting on itself.
     */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_StreamingAvailability() -> ECkOptimizationDebugger_StreamingAvailability;
}

// --------------------------------------------------------------------------------------------------------------------
