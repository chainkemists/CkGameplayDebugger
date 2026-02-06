#include "ArgusFragmentTypeMap.h"

// Fragment headers — only fragments that are USTRUCTs (with GENERATED_BODY + UPROPERTY)
// can provide full reflection. Non-USTRUCT fragments are registered for type identification
// but will have empty property lists.

// NOTE: Most CkFoundation fragment types (e.g., ck::FFragment_Transform, ck::FFragment_LifetimeOwner)
// are plain C++ structs with CK_GENERATED_BODY() but NOT USTRUCT(). They don't have UE reflection.
// However, many of their _Data types (e.g., FCk_Net_ConnectionSettings, FCk_Transform_ParamsData)
// ARE USTRUCTs with UPROPERTY fields.
//
// For Phase 1, we register the _Data USTRUCTs that Argus can inspect via UE reflection.
// As fragments are migrated to USTRUCT, they can be added here with full reflection support.

// --- USTRUCT data types with full UE reflection ---
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"
#include "CkEcs/Net/CkNet_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

auto RegisterAllFragments() -> void
{
    auto& Map = FFragmentTypeMap::Get();

    // USTRUCTs with full UPROPERTY reflection:
    // These provide complete property metadata (name, type, offset, size)

    // Transform params data — has interpolation settings with UPROPERTY fields
    Map.Register<FCk_Transform_ParamsData>(FCk_Transform_ParamsData::StaticStruct());

    // Network connection settings — has replication/netmode/netrole UPROPERTY fields
    Map.Register<FCk_Net_ConnectionSettings>(FCk_Net_ConnectionSettings::StaticStruct());

    // Transform interpolation params data
    Map.Register<FCk_TransformInterpolation_ParamsData>(FCk_TransformInterpolation_ParamsData::StaticStruct());

    // Transform interpolation settings
    Map.Register<FCk_Transform_Interpolation_Settings>(FCk_Transform_Interpolation_Settings::StaticStruct());

    // NOTE: As CkFoundation fragments are migrated to USTRUCT, add them here:
    // Map.Register<ck::FFragment_Transform>(ck::FFragment_Transform::StaticStruct());
    // Map.Register<ck::FFragment_LifetimeOwner>(ck::FFragment_LifetimeOwner::StaticStruct());
    // etc.

    UE_LOG(LogTemp, Log, TEXT("ArgusDebugServer: Registered %d fragment types"), Map.Num());
}

} // namespace Argus
