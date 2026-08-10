#include "CkDebug_RequestGate.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "Engine/World.h"

// ====================================================================================================================

namespace ck_debug_request_gate
{
    auto Make_Enabled() -> FCk_DebugRequest_GateVerdict
    {
        return FCk_DebugRequest_GateVerdict{true, FText::GetEmpty()};
    }

    auto Make_Enabled_WithAdvisory(const TCHAR* InAdvisory) -> FCk_DebugRequest_GateVerdict
    {
        return FCk_DebugRequest_GateVerdict{true, FText::FromString(InAdvisory)};
    }

    auto Make_Disabled(const TCHAR* InReason) -> FCk_DebugRequest_GateVerdict
    {
        return FCk_DebugRequest_GateVerdict{false, FText::FromString(InReason)};
    }
}

// ====================================================================================================================

auto
    ck::DebugRequestGate::
    Evaluate(
        const TOptional<ENetMode>&   InNetMode,
        ECk_DebugRequest_Requirement InRequirement)
    -> FCk_DebugRequest_GateVerdict
{
    using namespace ck_debug_request_gate;

    // No world => nothing can be said about authority. LocalOk is the only requirement that does not
    // depend on the answer, so it is the only one that survives.
    if (NOT InNetMode.IsSet())
    {
        switch (InRequirement)
        {
            case ECk_DebugRequest_Requirement::AuthorityOnly:
                return Make_Disabled(TEXT("Entity has no world — authority cannot be established."));

            case ECk_DebugRequest_Requirement::CosmeticOnly:
                return Make_Disabled(TEXT("Entity has no world — there is nothing to play this on."));

            case ECk_DebugRequest_Requirement::LocalOk:
                return Make_Enabled();
        }

        return Make_Enabled();
    }

    const auto NetMode = InNetMode.GetValue();

    switch (InRequirement)
    {
        case ECk_DebugRequest_Requirement::AuthorityOnly:
        {
            // Standalone and both server flavours have authority; only a pure client does not.
            return NetMode == NM_Client
                ? Make_Disabled(TEXT("Client world — this request is authority-only and the server owns it. "
                                     "Run it from the server world."))
                : Make_Enabled();
        }

        case ECk_DebugRequest_Requirement::CosmeticOnly:
        {
            return NetMode == NM_DedicatedServer
                ? Make_Disabled(TEXT("Dedicated server — cosmetic-only requests never run here."))
                : Make_Enabled();
        }

        case ECk_DebugRequest_Requirement::LocalOk:
        {
            return NetMode == NM_Client
                ? Make_Enabled_WithAdvisory(TEXT("Local only — the server's next replication will overwrite this."))
                : Make_Enabled();
        }
    }

    return Make_Enabled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugRequestGate::
    Get_NetMode(
        const FCk_Handle& InHandle)
    -> TOptional<ENetMode>
{
    if (ck::Is_NOT_Valid(InHandle))
    { return {}; }

    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);

    if (ck::Is_NOT_Valid(World))
    { return {}; }

    return World->GetNetMode();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::DebugRequestGate::
    Evaluate(
        const FCk_Handle&            InHandle,
        ECk_DebugRequest_Requirement InRequirement)
    -> FCk_DebugRequest_GateVerdict
{
    return Evaluate(Get_NetMode(InHandle), InRequirement);
}

// ====================================================================================================================
