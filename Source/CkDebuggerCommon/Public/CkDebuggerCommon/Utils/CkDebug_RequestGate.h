#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "Engine/EngineBaseTypes.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"

// ====================================================================================================================
// Should this debugger control be clickable in THIS world?
//
// Every interactive inspector row fires a public Utils Request_*. Some of those are processor-
// enforced authority-only (StateMachine, Aggro, MontagePlayer, Inventories): pressing the button on
// a client silently ensure-and-drops, which reads to the user as "the debugger is broken". Others
// are cosmetic-only and are a no-op on a dedicated server. The gate turns both into a GREYED control
// carrying the reason as its tooltip, instead of a live button that does nothing.
//
// Net mode comes from the entity's WORLD (UCk_Utils_EntityLifetime_UE::Get_WorldForEntity ->
// UWorld::GetNetMode), the same source SCkDebug_WorldSelector renders its per-world buttons from.
// CkEcs's UCk_Utils_Net_UE::Get_EntityNetMode is deliberately NOT the source here: it answers
// Unknown for every entity without FFragment_Net_Params, which is most of them in a single-player
// gym, and would grey out the whole inspector where nothing is networked at all.
// ====================================================================================================================

enum class ECk_DebugRequest_Requirement : uint8
{
    // The processor behind this request only runs with authority. Disabled on a client world.
    AuthorityOnly,

    // Purely visual/audible. Never runs on a dedicated server, which has no local client.
    CosmeticOnly,

    // Runs anywhere. On a client world a replicated value may be server-stomped, which is an
    // advisory (the control stays enabled), not a block.
    LocalOk,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Reason semantics: when IsEnabled is false, Reason is WHY the control is greyed. When IsEnabled is
 * true and Reason is non-empty, it is an ADVISORY tooltip ("this will be overwritten"), which is how
 * the attribute rows warn about server projection without blocking a legitimate local experiment.
 */
struct CKDEBUGGERCOMMON_API FCk_DebugRequest_GateVerdict
{
    bool  IsEnabled = true;
    FText Reason;

    auto Get_IsEnabled() const -> bool { return IsEnabled; }
    auto Get_Reason() const -> const FText& { return Reason; }
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugRequestGate
{
    /**
     * The whole truth table, as a pure function — no world, no handle, fully spec-testable.
     * An unset net mode means "the entity has no resolvable world"; the two net-sensitive
     * requirements refuse rather than guess, LocalOk still passes.
     */
    CKDEBUGGERCOMMON_API auto Evaluate(
        const TOptional<ENetMode>&   InNetMode,
        ECk_DebugRequest_Requirement InRequirement) -> FCk_DebugRequest_GateVerdict;

    /** The entity's world net mode, unset when the handle is invalid or has no world yet. */
    CKDEBUGGERCOMMON_API auto Get_NetMode(
        const FCk_Handle& InHandle) -> TOptional<ENetMode>;

    /** Convenience over the two above — what every inspector row calls. */
    CKDEBUGGERCOMMON_API auto Evaluate(
        const FCk_Handle&            InHandle,
        ECk_DebugRequest_Requirement InRequirement) -> FCk_DebugRequest_GateVerdict;
}

// ====================================================================================================================
