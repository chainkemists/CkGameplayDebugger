#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// Focus-entity-in-PIE — the editor's "F" for ECS entities.
//
// One-shot: frames the entity's resolved world bounds in the level-editor
// viewport by gliding the camera back along its CURRENT view direction (no
// rotation — same animated feel as the editor's actor focus, via the view
// transform's TransitionToLocation). While POSSESSED, Focus_Entity queues the
// framing, requests the PIE↔SIE eject toggle (what F8 does), and completes
// once the editor processes it — so focus works from any state a debugger
// row can be clicked in. Distance: the exact FOV-fit distance times the
// `ck.Debug.Focus.DistanceScale` cvar (default 2 — fit-exact reads too close).
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
    // True when focusing can act: an editor viewport exists and is either
    // already ejected or in a PIE session it can eject from — gate focus
    // buttons/menu entries on this.
    CKDEBUGGERCOMMON_API auto Get_CanFocus() -> bool;

    // Frame the entity in the level-editor viewport (auto-ejects first while
    // possessed). Returns false when no viewport is available, the entity is
    // invalid, or no location could be resolved.
    CKDEBUGGERCOMMON_API auto Focus_Entity(const FCk_Handle& InEntity) -> bool;

    // The bounds Focus_Entity frames (resolution order in the header comment) —
    // also drives the picker's hover-bounds highlight. Unset when the entity has
    // neither transform nor any owning actor.
    CKDEBUGGERCOMMON_API auto Get_EntityWorldBounds(const FCk_Handle& InEntity) -> TOptional<FBoxSphereBounds>;
}

// --------------------------------------------------------------------------------------------------------------------
