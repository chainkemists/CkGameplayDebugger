#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** One snapshot as one shareable `.cksnap` file: QA captures on their machine, attaches the file to a ticket, and
 *  tech art loads it with the picture, the per-pixel identification AND the selection intact — "look at these three
 *  meshes" travels with the file. */
namespace ck_optimization_debugger_snapshot_codec
{
    /** Bumped when the layout changes. A file from a NEWER version loads to nothing rather than to a guess. */
    inline constexpr uint32 k_SnapshotFileVersion = 1;

    CKOPTIMIZATIONDEBUGGER_API auto
    Encode_SnapshotFile(
        const FCkOptimizationDebugger_Snapshot& InSnapshot) -> TArray<uint8>;

    /** Unset for anything that is not a whole, well-formed snapshot file of a known version — wrong magic, future
     *  version, truncation, or counts the remaining bytes cannot back. Never a partial snapshot: half a prim table
     *  under a real picture would be read as the truth about that picture (the RLE decoder's rule, restated for
     *  files). Pure over bytes, so the round-trip spec runs with no disk and no editor. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Decode_SnapshotFile(
        const TArray<uint8>& InBytes) -> TOptional<FCkOptimizationDebugger_Snapshot>;
}

// --------------------------------------------------------------------------------------------------------------------
