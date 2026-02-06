#pragma once

#include "CoreMinimal.h"
#include "ArgusProtocolTypes.h"

// --------------------------------------------------------------------------------------------------------------------
// FArgusHandshakeBuilder
//
// Builds a HandshakeResponse by:
//   1. Validating the client's ENTT version
//   2. Collecting process metadata (PID, project name, engine version)
//   3. Enumerating all active entt::registry pointers from UE worlds
//   4. Building component type info via UE reflection (TFieldIterator<FProperty>)
//
// MUST be called on the game thread (accesses UE subsystems and GEngine).
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

class FHandshakeBuilder
{
public:
    /**
     * Build a complete HandshakeResponse from the client's request.
     * Must be called on the game thread.
     */
    static auto Build(const FHandshakeRequest& InRequest) -> FHandshakeResponse;

private:
    /** Enumerate all active ECS registries across all worlds. */
    static auto EnumerateRegistries() -> TArray<FRegistryInfo>;

    /** Build component type info from the fragment type map using UE reflection. */
    static auto BuildComponentTypes() -> TArray<FComponentTypeInfo>;

    /** Build property list for a single UScriptStruct using TFieldIterator. */
    static auto BuildPropertyList(const UScriptStruct* InStruct) -> TArray<FPropertyInfo>;
};

} // namespace Argus
