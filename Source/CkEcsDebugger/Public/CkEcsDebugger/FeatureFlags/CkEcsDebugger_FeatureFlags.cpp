#include "CkEcsDebugger_FeatureFlags.h"

#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Fragment.h"
#include "CkAttribute/VectorAttribute/CkVectorAttribute_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkCamera/Camera/CkCamera_Fragment.h"
#include "CkEcs/OwningActor/CkOwningActor_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEntityCollection/CkEntityCollection_Fragment.h"
#include "CkEqs/Query/CkEqs_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Fragment.h"
#include "CkLabel/CkLabel_Fragment.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkTimer/CkTimer_Fragment.h"
#include "CkTween/CkTween_Fragment.h"
#include "CkVfx/Cue/CkVfxCue_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_feature_flags::
    RegisterAll()
    -> void
{
    // First batch — marker fragments verified against their feature's _Fragment.h.
    // Extend alongside new per-inspector Get_FeatureFlagId overrides (Phase 1 tracker
    // lists the remaining candidates: Inventory, Objective, Vfx, Tween, Ism/IskmProxy,
    // EntityTag, Net, ActorBridge, Camera).
    // Infrastructure flag (underscore prefix = structural, ignored by classification and
    // feature tokens): LifetimeOwner sits on every tree-relevant entity, so its sinks make
    // Get_Revision bump on every spawn/destroy — the incremental world model's O(1) churn
    // detector.
    debug_feature_flags::RegisterFlag<FFragment_LifetimeOwner>(TEXT("_TreeEntity"));

    debug_feature_flags::RegisterFlag<FFragment_Timer_Params>(TEXT("Timer"));
    // FFragment_Transform is the real pool — FFragment_Transform_Params is a ParamsData
    // ALIAS that is never added to any entity (CkTransform_Utils.cpp adds FFragment_Transform).
    debug_feature_flags::RegisterFlag<FFragment_Transform>(TEXT("Transform"));
    debug_feature_flags::RegisterFlag<FFragment_SceneNode_Current>(TEXT("SceneNode"));
    debug_feature_flags::RegisterFlag<FFragment_Probe_Params>(TEXT("Probe"));
    debug_feature_flags::RegisterFlag<FFragment_FloatAttribute_Current>(TEXT("FloatAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_ByteAttribute_Current>(TEXT("ByteAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_IntegerAttribute_Current>(TEXT("IntegerAttribute"));
    // Same TUtils_Attribute<_Current> machinery as Float/Byte/Integer — parity carries over.
    debug_feature_flags::RegisterFlag<FFragment_VectorAttribute_Current>(TEXT("VectorAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_Sm_Params>(TEXT("StateMachine"));
    debug_feature_flags::RegisterFlag<FFragment_Aggro_Current>(TEXT("Aggro"));
    debug_feature_flags::RegisterFlag<FFragment_GameplayLabel>(TEXT("Label"));
    debug_feature_flags::RegisterFlag<FFragment_InteractionResolver_Params>(TEXT("InteractionResolver"));
    debug_feature_flags::RegisterFlag<FFragment_AudioTrack_Params>(TEXT("AudioTrack"));

    // Second batch (rail coverage) — each keyed on the feature's canonical always-present
    // fragment; drives rail/badges/queries. Inspector fast-path wiring (Get_FeatureFlagId)
    // stays unwired for these until parity is individually verified.
    debug_feature_flags::RegisterFlag<FFragment_Objective_Current>(TEXT("Objective"));
    debug_feature_flags::RegisterFlag<FFragment_VfxCue_Current>(TEXT("Vfx"));
    debug_feature_flags::RegisterFlag<FFragment_Camera_Params>(TEXT("Camera"));
    debug_feature_flags::RegisterFlag<FFragment_Goap_Planner_Current>(TEXT("Goap"));
    debug_feature_flags::RegisterFlag<FFragment_EqsQuery_State>(TEXT("Eqs"));
    debug_feature_flags::RegisterFlag<FFragment_IsmProxy_Current>(TEXT("IsmProxy"));
    debug_feature_flags::RegisterFlag<FFragment_IskmProxy_Current>(TEXT("IskmProxy"));
    debug_feature_flags::RegisterFlag<FFragment_OwningActor_Current>(TEXT("ActorBridge"));
    debug_feature_flags::RegisterFlag<FFragment_Tween_Current>(TEXT("Tween"));
    debug_feature_flags::RegisterFlag<FFragment_EntityCollection_Params>(TEXT("EntityCollection"));
}

auto
    ck::ecs_debugger_feature_flags::
    EnableFor(
        const FCk_Registry& InRegistry)
    -> void
{
    debug_feature_flags::Enable(InRegistry);
}

auto
    ck::ecs_debugger_feature_flags::
    DisableFor(
        const FCk_Registry& InRegistry)
    -> void
{
    debug_feature_flags::Disable(InRegistry);
}
