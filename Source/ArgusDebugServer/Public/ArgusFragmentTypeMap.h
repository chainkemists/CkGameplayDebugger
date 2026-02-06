#pragma once

#include "CoreMinimal.h"
#include <entt/core/type_info.hpp>
#include <entt/entity/fwd.hpp>

// --------------------------------------------------------------------------------------------------------------------
// FArgusFragmentTypeMap
//
// Maps EnTT component type IDs to UScriptStruct* so the handshake builder can
// enumerate registered fragment types and use UE reflection to extract property metadata.
//
// Registration happens in StartupModule via ARGUS_REGISTER_FRAGMENT calls.
// UE reflection (TFieldIterator<FProperty>) provides all property info automatically —
// no per-property registration needed.
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

class FFragmentTypeMap
{
public:
    static auto Get() -> FFragmentTypeMap&;

    /**
     * Register a fragment type.
     * @tparam T_Fragment The C++ fragment type (must be a USTRUCT with StaticStruct()).
     * @param InStruct The UScriptStruct* obtained via T_Fragment::StaticStruct().
     */
    template <typename T_Fragment>
    auto Register(const UScriptStruct* InStruct) -> void
    {
        const auto TypeHash = entt::type_id<T_Fragment>().hash();
        Entries.Add(TypeHash, InStruct);
    }

    /** Get all registered entries. */
    auto GetAllEntries() const -> const TMap<entt::id_type, const UScriptStruct*>&;

    /** Number of registered fragment types. */
    auto Num() const -> int32;

private:
    TMap<entt::id_type, const UScriptStruct*> Entries;
};

} // namespace Argus

// Convenience macro for registration calls in ArgusFragmentRegistration.cpp
#define ARGUS_REGISTER_FRAGMENT(FragmentType) \
    Argus::FFragmentTypeMap::Get().Register<FragmentType>(FragmentType::StaticStruct())
