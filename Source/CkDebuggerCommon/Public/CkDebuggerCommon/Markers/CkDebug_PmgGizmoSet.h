#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CoreMinimal.h"

class UWorld;

// ====================================================================================================================
// Persistent PMG transform gizmos (RGB axis triad) keyed by target entity.
//
// Replaces the per-tick DrawDebugTransformGizmo pattern: one-frame debug lines
// re-issued from a refresh-GATED Slate tick blink whenever the gate caps below
// frame rate, and re-batch lines every tick. Here each gizmo is six persistent
// PMG solids (Duration=-1, pooled procmesh) — a cylinder shaft + cone tip per
// axis — that are only MOVED per tick: no geometry churn, no flicker at any
// gate setting. Solids rather than flat arrows: flat shapes read edge-on-
// invisible in perspective. Each part bakes a full-alpha wireframe outline as
// a second mesh section (FProcessor_Pmg_DebugShape_BakeLines), so the axes
// stay legible against same-hue geometry.
//
// Not built on CkPmg's Pivot: Pivot is a Composite whose child arrows bake
// their world transforms once at setup (CkPmg_Processor_DirectionalShapes.cpp,
// FProcessor_Pmg_Pivot_Setup) — moving the pivot parent afterwards moves
// nothing. The six solids are created directly and re-aimed on update.
//
// Lifecycle contract (mirrors inspector rebuild semantics):
//   - UpdateGizmo(World, Key, Transform) — create-on-first-sight, then move.
//     Call from your per-tick hook. No-ops on redundant transforms.
//   - Remove(Key)                        — target vanished mid-selection.
//   - Reset()                            — destroy everything; call from
//     OnDeactivated / EndPIE (BEFORE the PIE registry dies — handle contract).
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkDebug_PmgGizmoSet
{
public:
    ~FCkDebug_PmgGizmoSet();

    // Create or move the gizmo for InKey so it matches InTransform (location +
    // rotation; scale is deliberately ignored — the gizmo is fixed-size, like
    // the DrawDebugTransformGizmo it replaces).
    auto UpdateGizmo(UWorld* InWorld, const FCk_Handle& InKey, const FTransform& InTransform) -> void;

    // Destroy the gizmo for InKey (no-op when absent).
    auto Remove(const FCk_Handle& InKey) -> void;

    // Destroy all gizmos. Safe to call repeatedly.
    auto Reset() -> void;

private:
    struct FGizmo
    {
        FCk_Handle           Root;
        FCk_Handle_Transform Parts[6];   // [0..2] axis shafts (cylinders), [3..5] axis tips (cones)
        FTransform           LastTransform = FTransform::Identity;
    };

    auto DoDestroy(FGizmo& InGizmo) -> void;

    TMap<FCk_Handle, FGizmo> _Gizmos;
};

// --------------------------------------------------------------------------------------------------------------------
