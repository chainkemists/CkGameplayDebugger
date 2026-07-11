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
// frame rate, and re-batch lines every tick. Here each gizmo is three
// persistent PMG Arrow entities (Duration=-1, pooled procmesh) that are only
// MOVED per tick — no geometry churn, no flicker at any gate setting.
//
// Not built on CkPmg's Pivot: Pivot is a Composite whose child arrows bake
// their world transforms once at setup (CkPmg_Processor_DirectionalShapes.cpp,
// FProcessor_Pmg_Pivot_Setup) — moving the pivot parent afterwards moves
// nothing. The three arrows are created directly and re-aimed on update.
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
        FCk_Handle_Transform Arrows[3];
        FTransform           LastTransform = FTransform::Identity;
    };

    auto DoDestroy(FGizmo& InGizmo) -> void;

    TMap<FCk_Handle, FGizmo> _Gizmos;
};

// --------------------------------------------------------------------------------------------------------------------
