// --------------------------------------------------------------------------------------------------------------------
// ArgusFragmentRegistration_Temp.cpp
//
// TEMPORARY — This file registers fragment USTRUCTs that have not yet been converted to
// self-register via CK_REGISTER_ECS_FRAGMENT_REFLECTED at their definition site in CkFoundation.
//
// As CkFoundation fragments are migrated to USTRUCT and self-register, remove the corresponding
// lines here. When this file is empty, delete it.
// --------------------------------------------------------------------------------------------------------------------

#include "CkEcs/Reflection/CkFragmentReflection_Macros.h"

// --- USTRUCT data types with full UE reflection ---
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"
#include "CkEcs/Net/CkNet_Fragment_Data.h"

// Transform params data — has interpolation settings with UPROPERTY fields
CK_REGISTER_ECS_FRAGMENT_REFLECTED(FCk_Transform_ParamsData)

// Network connection settings — has replication/netmode/netrole UPROPERTY fields
CK_REGISTER_ECS_FRAGMENT_REFLECTED(FCk_Net_ConnectionSettings)

// Transform interpolation params data
CK_REGISTER_ECS_FRAGMENT_REFLECTED(FCk_TransformInterpolation_ParamsData)

// Transform interpolation settings
CK_REGISTER_ECS_FRAGMENT_REFLECTED(FCk_Transform_Interpolation_Settings)
