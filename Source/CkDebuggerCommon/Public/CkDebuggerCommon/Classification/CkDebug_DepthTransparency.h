#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// Depth-transparent owners — the ONE authority both hierarchy surfaces call.
//
// First-class entities sometimes live under an ActorRelay. The relay is plumbing,
// not a conceptual parent, so depth/nesting logic should look THROUGH it: the ECS
// tree hoists relay children to top level, and the overlay's depth walk does not
// count a relay hop. v1 has exactly one built-in rule (owning actor is an
// ACk_ActorRelay_UE) behind a single per-user settings bool; no per-project rule
// list until a second transparent type exists.
// ====================================================================================================================

namespace ck::DebugDepthTransparency
{
    // True when depth/nesting logic should look through InOwner: the per-user setting is on
    // AND InOwner's owning actor is an ACk_ActorRelay_UE. Safe on invalid handles (false).
    CKDEBUGGERCOMMON_API auto Get_IsTransparentOwner(const FCk_Handle& InOwner) -> bool;

    // The raw relay check without the settings gate (for surfaces that need "is relay"
    // regardless of the transparency toggle, e.g. the tree's affordance row).
    CKDEBUGGERCOMMON_API auto Get_IsRelayEntity(const FCk_Handle& InEntity) -> bool;
}

// ====================================================================================================================
