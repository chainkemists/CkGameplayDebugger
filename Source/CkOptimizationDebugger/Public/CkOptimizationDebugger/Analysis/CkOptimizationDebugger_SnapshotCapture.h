#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CoreMinimal.h"

#include "Camera/CameraTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotCaptureParams
{
    int32 CaptureWidth = 1280;

    /** Past this many visible primitives the capture keeps the nearest and says how many it dropped. The cap exists
     *  because Phase 5 renders one stencil pass per 255 candidates, and an uncapped scene is an unbounded number of
     *  passes inside one press. */
    int32 MaxPrims = 4096;

    // Handed in for the same reason the model's timestamps are: capture code that read the clock could not be
    // driven by a spec, and a label is display state either way.
    FDateTime CapturedAt;

    FString Label;
};

// --------------------------------------------------------------------------------------------------------------------

/** Taking one picture and identifying what is in it. Deliberately NOT behind `#if WITH_EDITOR` — this is what
 *  "works in a Development build" rests on, and only the editor-viewport point-of-view branch is editor-only. */
namespace ck_optimization_debugger_snapshot_capture
{
    /** A game or PIE world if one is running, else the editor world. The order matters: while PIE is up, the picture
     *  the reader wants is the one the PLAYER is looking at. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_CaptureWorld() -> UWorld*;

    /** Where the picture is taken from. Game/PIE: the first local player's camera-manager cache. Editor: the level
     *  viewport client's own location, rotation and FOV. Unset when neither yields a view, which is a refusal to
     *  capture rather than a capture from the origin. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_CaptureView(
        UWorld* InWorld) -> TOptional<FMinimalViewInfo>;

    /** One explicit, synchronous capture. Unset plus a filled `OutFailureReason` on every failure path — a
     *  half-snapshot that looked like a real one would be a picture the reader draws conclusions from. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Capture(
        UWorld* InWorld,
        const FCkOptimizationDebugger_SnapshotCaptureParams& InParams,
        FString& OutFailureReason) -> TOptional<FCkOptimizationDebugger_Snapshot>;

    /** Writes the snapshot to `<Project>/Saved/CkOptimizationDebugger/` as two PNGs: the colour image and a
     *  false-coloured ID map. This pair IS how identification is proven — the ID map has to be a silhouette-exact
     *  copy of the colour image, and the ways it can be wrong are all things a human sees instantly and no
     *  automated check can: soft edges mean the two passes disagree about the point of view, a far mesh's colour
     *  bleeding through a near one means the render-every-pass rule was broken, speckle means the identity target
     *  was not linear. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Dump_DebugImages(
        const FCkOptimizationDebugger_Snapshot& InSnapshot,
        FString& OutFailureReason) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
