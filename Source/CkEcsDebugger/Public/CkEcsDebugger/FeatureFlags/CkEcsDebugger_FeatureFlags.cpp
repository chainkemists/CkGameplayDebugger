#include "CkEcsDebugger_FeatureFlags.h"

#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"
#include "CkLabel/CkLabel_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkTimer/CkTimer_Fragment.h"

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
    debug_feature_flags::RegisterFlag<FFragment_Timer_Params>(TEXT("Timer"));
    // FFragment_Transform is the real pool — FFragment_Transform_Params is a ParamsData
    // ALIAS that is never added to any entity (CkTransform_Utils.cpp adds FFragment_Transform).
    debug_feature_flags::RegisterFlag<FFragment_Transform>(TEXT("Transform"));
    debug_feature_flags::RegisterFlag<FFragment_SceneNode_Current>(TEXT("SceneNode"));
    debug_feature_flags::RegisterFlag<FFragment_Probe_Params>(TEXT("Probe"));
    debug_feature_flags::RegisterFlag<FFragment_FloatAttribute_Current>(TEXT("FloatAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_ByteAttribute_Current>(TEXT("ByteAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_IntegerAttribute_Current>(TEXT("IntegerAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_Sm_Params>(TEXT("StateMachine"));
    debug_feature_flags::RegisterFlag<FFragment_Aggro_Current>(TEXT("Aggro"));
    debug_feature_flags::RegisterFlag<FFragment_GameplayLabel>(TEXT("Label"));
    debug_feature_flags::RegisterFlag<FFragment_InteractionResolver_Params>(TEXT("InteractionResolver"));
    debug_feature_flags::RegisterFlag<FFragment_AudioTrack_Params>(TEXT("AudioTrack"));
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
