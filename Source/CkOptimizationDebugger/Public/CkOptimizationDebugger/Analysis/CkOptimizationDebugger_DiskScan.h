#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_disk
{
    /**
     * What `/Game` costs on disk, bucketed by asset family.
     *
     * Asset-registry driven and **package**-granular, because that is the granularity the number exists at:
     * `FAssetPackageData::DiskSize` is the size of a package on disk, not of one object inside it. A package is
     * therefore attributed to exactly one family — the family of its PRIMARY asset, the one whose name matches the
     * package's own — so no byte is counted twice and the buckets add up to the total printed beside them.
     *
     * Reads nothing but the registry's serialized headers: no package is loaded, no DDC build is triggered, and no
     * texture or mesh is realized to answer the question. Classification goes through `FAssetData::IsInstanceOf`
     * with `EResolveClass::No`, so a class nobody has loaded lands in `Other` rather than loading it to find out.
     *
     * Outside the editor this returns an `IsAvailable == false` breakdown — the module ships in packaged
     * Development/DebugGame builds, where the asset registry describes a cooked set that answers a different
     * question. Zero bytes and "nobody asked" are different statements and the flag is what separates them.
     */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ProjectDiskBreakdown() -> FCkOptimizationDebugger_DiskBreakdown;
}

// --------------------------------------------------------------------------------------------------------------------
