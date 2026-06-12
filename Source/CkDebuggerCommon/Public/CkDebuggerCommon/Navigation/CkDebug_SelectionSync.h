#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// Cross-debugger selection sync — sibling of CkDebug_Navigator.
//
// Selecting an agent in one debugger selects the matching entity in every
// other debugger. The wrinkle: each debugger lists a DIFFERENT entity for the
// same conceptual agent (Crowd lists the crowd-agent feature entity, GOAP
// lists the owner NPC, ECS lists everything), so receivers match by LINEAGE
// instead of equality: a candidate matches when it is an ancestor-or-self of
// the broadcast entity, or the broadcast entity is an ancestor-or-self of the
// candidate (lifetime-owner chain).
//
// Deliberately NOT "walk to the top-level root and compare": in gyms every
// NPC is lifetime-owned by its station, so root-comparison would cross-match
// unrelated NPCs under the same station. Lineage containment cannot.
//
// Loop protection is two-layered:
//   - Receivers apply selection inside an FApplyGuard scope; Broadcast() is a
//     no-op while any guard is alive.
//   - Receivers ignore broadcasts whose InSource equals their own source tag,
//     and skip re-broadcasting selections applied with ESelectInfo::Direct.
// ====================================================================================================================

namespace ck::DebugSelectionSync
{
    DECLARE_MULTICAST_DELEGATE_TwoParams(FCkDebug_OnGlobalSelection, const FCk_Handle& /*SelectedEntity*/, FName /*SourceDebugger*/);

    CKDEBUGGERCOMMON_API auto Get_OnSelection() -> FCkDebug_OnGlobalSelection&;

    // Broadcast a user-driven selection to every other debugger. No-op when
    // the handle is invalid or while a received selection is being applied.
    CKDEBUGGERCOMMON_API auto Broadcast(const FCk_Handle& InSelected, FName InSource) -> void;

    // True while some receiver is applying a broadcast selection.
    CKDEBUGGERCOMMON_API auto Is_Applying() -> bool;

    // RAII scope receivers hold while pushing a broadcast selection into their
    // own view-model / list widget, so their selection handlers don't echo.
    struct CKDEBUGGERCOMMON_API FApplyGuard
    {
        FApplyGuard();
        ~FApplyGuard();
    };

    // True when one handle is an ancestor-or-self of the other along the
    // lifetime-owner chain (either direction). Cross-registry handles never
    // match. Walk is depth-capped and stops at the transient root.
    CKDEBUGGERCOMMON_API auto Is_SameLineage(const FCk_Handle& InA, const FCk_Handle& InB) -> bool;
}

// ====================================================================================================================
