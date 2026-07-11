#include "CkDebugOverlay_Provider_Crowd.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Crowd state — mirroring the crowd debugger collector's read set (read-only fragment
// access is the established collector pattern in this plugin).
#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"
#include "CkCrowd/Agent/CkCrowdAgent_Neighbors_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Crowd,
    "Ck.OnScreenDebugger.Provider.Crowd")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Crowd_Status,
    "Ck.OnScreenDebugger.Provider.Crowd.Status")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Crowd_Speed,
    "Ck.OnScreenDebugger.Provider.Crowd.Speed")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Crowd_Goal,
    "Ck.OnScreenDebugger.Provider.Crowd.Goal")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Crowd_Neighbors,
    "Ck.OnScreenDebugger.Provider.Crowd.Neighbors")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()         { return TAG_Ck_OnScreenDebugger_Provider_Crowd; }
    FGameplayTag FieldTag_Status()     { return TAG_Ck_OnScreenDebugger_Provider_Crowd_Status; }
    FGameplayTag FieldTag_Speed()      { return TAG_Ck_OnScreenDebugger_Provider_Crowd_Speed; }
    FGameplayTag FieldTag_Goal()       { return TAG_Ck_OnScreenDebugger_Provider_Crowd_Goal; }
    FGameplayTag FieldTag_Neighbors()  { return TAG_Ck_OnScreenDebugger_Provider_Crowd_Neighbors; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Crowd::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Crowd::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Status(),    true },
        { FieldTag_Speed(),     true },
        { FieldTag_Goal(),      true },
        { FieldTag_Neighbors(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Crowd::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_CrowdAgent_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Crowd::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    auto MutableEntity = Entity;
    const auto Agent = UCk_Utils_CrowdAgent_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(Agent))
    { return; }

    const auto IsWalking   = Entity.Has<ck::FTag_CrowdAgent_Walking>();
    const auto HasOverride = UCk_Utils_CrowdAgent_UE::Get_HasDebugOverride(Agent);

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Status()))
    {
        auto StatusStr = FString{ IsWalking ? TEXT("Walking") : TEXT("Idle") };
        if (HasOverride)
        { StatusStr += TEXT(" [DEBUG OVERRIDE]"); }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Status();
        Row.Value    = FText::FromString(StatusStr);
        Row.Severity = HasOverride ? ECk_DebugOverlay_Severity::Warn
                     : IsWalking   ? ECk_DebugOverlay_Severity::Good
                                   : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Speed()) &&
        Entity.Has<ck::FFragment_CrowdAgent_DesiredVelocity>())
    {
        const auto Velocity = Entity.Get<ck::FFragment_CrowdAgent_DesiredVelocity>().Get_Velocity();
        const auto MaxSpeed = Entity.Has<ck::FFragment_CrowdAgent_Params>()
            ? Entity.Get<ck::FFragment_CrowdAgent_Params>().Get_MaxSpeed()
            : 0.0f;

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Speed();
        Row.Value    = FText::FromString(
            ck::Format_UE(TEXT("{:.0f}/{:.0f} cm/s"), Velocity.Size(), MaxSpeed));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Active-goal state — only meaningful while Walking; a stale goal on an idle agent
    // is skipped (mirrors the crowd debugger collector).
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Goal()) &&
        IsWalking && Entity.Has<ck::FFragment_CrowdAgent_PathFollow>())
    {
        const auto& PathFollow = Entity.Get<ck::FFragment_CrowdAgent_PathFollow>();
        const auto  Goal       = PathFollow.Get_ActiveGoal();

        auto DistStr = FString{};
        if (auto TransformHandle = UCk_Utils_Transform_UE::Cast(MutableEntity);
            ck::IsValid(TransformHandle))
        {
            const auto Position = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(TransformHandle);
            DistStr = ck::Format_UE(TEXT("  d={:.0f}"), FVector::Dist(Position, Goal));
        }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Goal();
        Row.Value    = FText::FromString(
            ck::Format_UE(TEXT("({:.0f}, {:.0f}, {:.0f}){}"), Goal.X, Goal.Y, Goal.Z, DistStr));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Neighbors()) &&
        Entity.Has<ck::FFragment_CrowdAgent_NeighborCache>())
    {
        const auto NeighborCount =
            Entity.Get<ck::FFragment_CrowdAgent_NeighborCache>().Get_Neighbors().Num();

        const auto SeparationSize = Entity.Has<ck::FFragment_CrowdAgent_SeparationForce>()
            ? Entity.Get<ck::FFragment_CrowdAgent_SeparationForce>().Get_Force().Size()
            : 0.0f;

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Neighbors();
        Row.Value    = FText::FromString(
            ck::Format_UE(TEXT("{}  sep={:.0f}"), NeighborCount, SeparationSize));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Crowd::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    return Entity.Has<ck::FTag_CrowdAgent_Walking>() ? TEXT("Crowd:Walk") : TEXT("Crowd:Idle");
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Crowd)

// --------------------------------------------------------------------------------------------------------------------
