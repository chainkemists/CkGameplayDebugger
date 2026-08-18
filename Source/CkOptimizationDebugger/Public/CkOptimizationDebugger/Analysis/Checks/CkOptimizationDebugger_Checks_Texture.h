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

class UTexture;

/** Texture checks: the texture assets the level's materials sample — dimensions, mip coverage and compression/colour-space correctness. */
namespace ck_optimization_debugger_checks_texture
{
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Checks(
        const FCkOptimizationDebugger_ScanContext& InContext,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        TArray<FCkOptimizationDebugger_FindingRow>& OutFindings) -> void;

    /** A texture's authored size and the size the build will actually produce.
     *
     *  `Built*` is the top-mip size after PowerOfTwoMode / ResizeDuringBuild, MaxTextureSize and the
     *  LOD-group + LODBias mip drop — the number the texture editor prints as "Max In-Game", and the
     *  only number a budget can honestly judge. `Source*` is what the artist imported; the two differ
     *  exactly when some build-time cap is doing its job. */
    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_TextureDims
    {
        int32 SourceWidth = 0;
        int32 SourceHeight = 0;

        int32 BuiltWidth = 0;
        int32 BuiltHeight = 0;
    };

    /** Unset for a texture with neither a valid source nor platform data. Reads serialized header
     *  data only — never platform data on the source path, so it can never trigger a DDC build. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_TextureDims(
        UTexture* InTexture) -> TOptional<FCkOptimizationDebugger_TextureDims>;

    /** Whether this texture carries DATA rather than colour — packed masks, roughness, metallic, AO. Two signals,
     *  either of which is enough: the compression setting says so outright, or the name follows the packed-channel
     *  convention.
     *
     *  Exported because the `Texture.DataTextureSrgb` FIX has to re-ask the same question before it mutates, and a
     *  second copy of the rule in the fix would be a second place for it to drift. Re-validating only "is sRGB still
     *  on" was not enough: a texture re-authored as an albedo since the scan is no longer a data texture, and the
     *  fix would have stripped sRGB off a colour map and reported plain success. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Is_DataTexture(
        const UTexture* InTexture,
        const FString& InAssetName) -> bool;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
