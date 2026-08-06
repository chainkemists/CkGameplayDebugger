#include "CkEcsDebugger_FeatureFlags.h"

#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Fragment.h"
#include "CkAnimation/MontagePlayer/CkMontagePlayer_Fragment.h"
#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment.h"
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Fragment.h"
#include "CkAttribute/RotatorAttribute/CkRotatorAttribute_Fragment.h"
#include "CkAttribute/VectorAttribute/CkVectorAttribute_Fragment.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkCamera/Camera/CkCamera_Fragment.h"
#include "CkCamera/CameraShake/CkCameraShake_Fragment.h"
#include "CkChaos/GeometryCollection/CkGeometryCollection_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkEcs/OwningActor/CkOwningActor_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEntityCollection/CkEntityCollection_Fragment.h"
#include "CkEntityExtension/CkEntityExtension_Fragment.h"
#include "CkEntityTag/CkEntityTag_Fragment.h"
#include "CkEqs/Query/CkEqs_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment.h"
#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Fragment.h"
#include "CkInteraction/InteractSource/CkInteractSource_Fragment.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"
#include "CkInventory/Inventory/CkInventory_Fragment.h"
#include "CkInventory/Item/CkItem_Fragment.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment.h"
#include "CkIsmRenderer/Proxy/CkIsmProxy_Fragment.h"
#include "CkLabel/CkLabel_Fragment.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkOverlapBody/Marker/CkMarker_Fragment.h"
#include "CkOverlapBody/Sensor/CkSensor_Fragment.h"
#include "CkPhysics/Velocity/CkVelocity_Fragment.h"
#include "CkProjectile/CkProjectile_Fragment.h"
#include "CkRaySense/CkRaySense_Fragment.h"
#include "CkRelationship/Player/CkPlayer_Fragment.h"
#include "CkRelationship/Team/CkTeam_Fragment.h"
#include "CkRenderTarget/RenderTarget/CkRenderTarget_Fragment.h"
#include "CkSnapshot/SaveKey/CkSnapshot_SaveKey_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpline/CkSpline_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkTagSet/CkTagSet_Fragment.h"
#include "CkTimer/CkTimer_Fragment.h"
#include "CkTween/CkTween_Fragment.h"
#include "CkUI/WorldSpaceWidget/CkWorldSpaceWidget_Fragment.h"
#include "CkUnrealComponent/CkUnrealComponent_Fragment.h"
#include "CkVat/Proxy/CkVatProxy_Fragment.h"
#include "CkVfx/Cue/CkVfxCue_Fragment.h"
#include "ResolverSource/CkResolverSource_Fragment.h"
#include "ResolverTarget/CkResolverTarget_Fragment.h"
#include "Sfx/CkSfx_Fragment.h"
#include "Vfx/CkVfx_Fragment.h"

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

    debug_feature_flags::RegisterFlag<FFragment_Timer>(TEXT("Timer"));
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
    // "VfxCue", not "Vfx": CkVfx's cue coordinator and CkFx's leaf Niagara wrapper are
    // distinct live features — the leaf owns the plain name below.
    debug_feature_flags::RegisterFlag<FFragment_VfxCue_Current>(TEXT("VfxCue"));
    debug_feature_flags::RegisterFlag<FFragment_Camera_Params>(TEXT("Camera"));
    debug_feature_flags::RegisterFlag<FFragment_Goap_Planner_Current>(TEXT("Goap"));
    debug_feature_flags::RegisterFlag<FFragment_EqsQuery_State>(TEXT("Eqs"));
    debug_feature_flags::RegisterFlag<FFragment_IsmProxy_Current>(TEXT("IsmProxy"));
    debug_feature_flags::RegisterFlag<FFragment_IskmProxy_Current>(TEXT("IskmProxy"));
    debug_feature_flags::RegisterFlag<FFragment_OwningActor_Current>(TEXT("ActorBridge"));
    debug_feature_flags::RegisterFlag<FFragment_Tween_Current>(TEXT("Tween"));
    debug_feature_flags::RegisterFlag<FFragment_EntityCollection_Params>(TEXT("EntityCollection"));

    // Third batch (full-inventory audit, 2026-07-11): every CkFoundation feature whose
    // canonical fragment is added at feature-Add() time (each verified against its
    // Add<>/AddOrGet<> call site). 56/64 bits used after this batch — the next batch of
    // this size needs the flag cache widened past one uint64 row (CkDebugFeatureFlags.h).
    //
    // Deliberately unflagged (no stable per-feature marker, or covered elsewhere):
    //   Cue/Messaging (record + transient message entities only), Nav (request-driven,
    //   AddOrGet<FFragment_Nav_Requests> per path request), AStar (params only ride on
    //   Goap planners + tests — redundant chip), EntitySpawner (PendingSpawn is transient),
    //   Variables (per-type fragments, no single marker), Dynamic (runtime-typed script
    //   fragments), Shapes (4 per-shape types attach to Probe/Marker/Sensor), Homing/
    //   BallisticMotion (Projectile sub-details), CameraLayer (Camera internal), AnimAsset
    //   (asset-carrier child), LagCompensation (niche; revisit), IsmRenderer/IskmRenderer
    //   (per-world singletons — arch: filter finds them), owner-side pairs (AggroOwner/
    //   ObjectiveOwner/GeometryCollectionOwner), grid/SM/Goap sub-entities, and pure
    //   infrastructure (Net, EntityScript, Record, Substep, RenderStatus, ResourceLoader,
    //   ReplicatedObjects, ContextOwner, DeferredEntity).
    debug_feature_flags::RegisterFlag<FFragment_EntityExtension_Params>(TEXT("EntityExtension"));
    debug_feature_flags::RegisterFlag<FFragment_UnrealComponent_Params>(TEXT("UnrealComponent"));
    debug_feature_flags::RegisterFlag<FFragment_SaveKey>(TEXT("Snapshot"));
    debug_feature_flags::RegisterFlag<FFragment_TagSet>(TEXT("TagSet"));
    debug_feature_flags::RegisterFlag<FFragment_EntityTag_Current>(TEXT("EntityTag"));
    debug_feature_flags::RegisterFlag<FFragment_RotatorAttribute_Current>(TEXT("RotatorAttribute"));
    debug_feature_flags::RegisterFlag<FFragment_CrowdAgent_Params>(TEXT("CrowdAgent"));
    debug_feature_flags::RegisterFlag<FFragment_2dGridSystem_Params>(TEXT("Grid"));
    debug_feature_flags::RegisterFlag<FFragment_Marker_Params>(TEXT("Marker"));
    debug_feature_flags::RegisterFlag<FFragment_Sensor_Params>(TEXT("Sensor"));
    debug_feature_flags::RegisterFlag<FFragment_RaySense_Params>(TEXT("RaySense"));
    debug_feature_flags::RegisterFlag<FFragment_Velocity_Params>(TEXT("Velocity"));
    debug_feature_flags::RegisterFlag<FFragment_Spline_Params>(TEXT("Spline"));
    debug_feature_flags::RegisterFlag<FFragment_InteractSource_Params>(TEXT("InteractSource"));
    debug_feature_flags::RegisterFlag<FFragment_InteractTarget_Params>(TEXT("InteractTarget"));
    debug_feature_flags::RegisterFlag<FFragment_Inventory_Params>(TEXT("Inventory"));
    debug_feature_flags::RegisterFlag<FFragment_InventoryItem>(TEXT("Item"));
    debug_feature_flags::RegisterFlag<FFragment_TeamInfo>(TEXT("Team"));
    debug_feature_flags::RegisterFlag<FFragment_PlayerInfo>(TEXT("Player"));
    debug_feature_flags::RegisterFlag<FTag_Projectile>(TEXT("Projectile"));
    debug_feature_flags::RegisterFlag<FFragment_ResolverSource_Params>(TEXT("ResolverSource"));
    debug_feature_flags::RegisterFlag<FFragment_ResolverTarget_Params>(TEXT("ResolverTarget"));
    debug_feature_flags::RegisterFlag<FFragment_GeometryCollection_Params>(TEXT("GeometryCollection"));
    debug_feature_flags::RegisterFlag<FFragment_AnimPlan_Params>(TEXT("AnimPlan"));
    debug_feature_flags::RegisterFlag<FFragment_MontagePlayer_Params>(TEXT("MontagePlayer"));
    debug_feature_flags::RegisterFlag<FFragment_VatProxy_Params>(TEXT("VatProxy"));
    debug_feature_flags::RegisterFlag<FFragment_RenderTarget_Params>(TEXT("RenderTarget"));
    debug_feature_flags::RegisterFlag<FFragment_WorldSpaceWidget_Params>(TEXT("WorldSpaceWidget"));
    debug_feature_flags::RegisterFlag<FFragment_CameraShake_Params>(TEXT("CameraShake"));
    debug_feature_flags::RegisterFlag<FFragment_Vfx_Params>(TEXT("Vfx"));
    debug_feature_flags::RegisterFlag<FFragment_AudioDirector_Params>(TEXT("AudioDirector"));
    debug_feature_flags::RegisterFlag<FFragment_Sfx_Params>(TEXT("Sfx"));
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
