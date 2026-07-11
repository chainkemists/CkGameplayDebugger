#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// Focus-entity-in-PIE — the editor's "F" for ECS entities.
//
// One-shot: frames the entity's resolved world bounds in the level-editor
// viewport by pulling the camera back along its CURRENT view direction (no
// rotation — same feel as the editor's actor focus). Deliberately
// ejected/simulate-only: while possessed the camera belongs to gameplay and
// is not fought over; Focus_Entity is then a no-op returning false.
//
// Bounds resolution order (tightest first, so focusing a feature sub-entity
// never frames its whole owner complex):
//   1. Own ISM-proxy mesh bounds at the entity transform.
//   2. Direct owning actor's bounds.
//   3. 1 m box at the entity's own transform (pawn-less NPCs, logic markers).
//   4. Recursive owning-actor bounds (entity carries no transform at all).
// ====================================================================================================================

namespace ck::DebugFocus
{
    // True while an ejected/simulate editor camera drives the viewport —
    // gate focus buttons/menu entries on this.
    CKDEBUGGERCOMMON_API auto Get_CanFocus() -> bool;

    // Frame the entity in the level-editor viewport. Returns false when not
    // ejected, the entity is invalid, or no location could be resolved.
    CKDEBUGGERCOMMON_API auto Focus_Entity(const FCk_Handle& InEntity) -> bool;

    // The bounds Focus_Entity frames (resolution order in the header comment) —
    // also drives the picker's hover-bounds highlight. Unset when the entity has
    // neither transform nor any owning actor.
    CKDEBUGGERCOMMON_API auto Get_EntityWorldBounds(const FCk_Handle& InEntity) -> TOptional<FBoxSphereBounds>;
}

// --------------------------------------------------------------------------------------------------------------------
