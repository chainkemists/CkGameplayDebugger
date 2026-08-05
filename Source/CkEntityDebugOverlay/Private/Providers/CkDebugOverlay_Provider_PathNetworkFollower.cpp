#include "CkDebugOverlay_Provider_PathNetworkFollower.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Route,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Status")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Failure,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Failure")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Corridor,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Corridor")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Goal,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Goal")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_LegacyRoute,
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Route")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower; }
    FGameplayTag FieldTag_Status() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Route; }
    FGameplayTag FieldTag_Failure() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Failure; }
    FGameplayTag FieldTag_Corridor() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Corridor; }
    FGameplayTag FieldTag_Goal() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Goal; }
    FGameplayTag FieldTag_LegacyRoute() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_LegacyRoute; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Status(), true }, { FieldTag_Failure(), true },
        { FieldTag_Corridor(), true }, { FieldTag_Goal(), true },
        { FieldTag_LegacyRoute(), false },
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_PathNetworkFollower::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_PathNetworkFollower_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Emits up to three rows when Route is enabled:
//   Row 1 — route status (severity Warning when Failed, plus the fail reason)
//   Row 2 — compiled corridor summary: waypoint count + total cost (Ready only)
//   Row 3 — goal location (whenever a route was ever requested)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    auto MutableEntity  = Entity;
    const auto Follower = UCk_Utils_PathNetworkFollower_UE::CastChecked(MutableEntity);

    if (ck::Is_NOT_Valid(Follower))
    { return; }

    const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(Follower);
    const auto Status = Result.Get_Status();
    const auto bLegacyRoute = Cfg.EnabledFields.HasTagExact(FieldTag_LegacyRoute());

    if (bLegacyRoute)
    {
        auto LegacyValue = ck::Format_UE(TEXT("{}"), Status);
        if (Status == ECk_PathNetwork_RouteStatus::Failed)
        { LegacyValue += ck::Format_UE(TEXT(" ({})"), Result.Get_FailReason()); }
        else if (Status == ECk_PathNetwork_RouteStatus::Ready)
        {
            LegacyValue += ck::Format_UE(TEXT(" / {} wps / {} legs / cost {}"),
                Result.Get_CompiledWaypoints().Num(), Result.Get_Legs().Num(), FMath::RoundToInt(Result.Get_TotalCost()));
        }
        if (Status != ECk_PathNetwork_RouteStatus::None)
        { LegacyValue += ck::Format_UE(TEXT(" / goal {}"), Result.Get_GoalLocation()); }
        FCk_DebugOverlay_Row Row; Row.FieldTag = FieldTag_LegacyRoute();
        Row.Value = FText::FromString(LegacyValue);
        Row.Severity = Status == ECk_PathNetwork_RouteStatus::Failed ? ECk_DebugOverlay_Severity::Bad : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Status()))
    {

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Status();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"), Status));
        Row.Severity = Status == ECk_PathNetwork_RouteStatus::Failed
            ? ECk_DebugOverlay_Severity::Warn
            : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 2 — corridor summary (Ready only)
    if (Status == ECk_PathNetwork_RouteStatus::Failed && Cfg.EnabledFields.HasTagExact(FieldTag_Failure()))
    {
        FCk_DebugOverlay_Row Row; Row.FieldTag = FieldTag_Failure();
        Row.Value = FText::FromString(ck::Format_UE(TEXT("{}"), Result.Get_FailReason()));
        Row.Severity = ECk_DebugOverlay_Severity::Bad; Out.Rows.Add(MoveTemp(Row));
    }
    if (Status == ECk_PathNetwork_RouteStatus::Ready && Cfg.EnabledFields.HasTagExact(FieldTag_Corridor()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Corridor();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("Corridor: {} wps / {} legs / cost {}"),
            Result.Get_CompiledWaypoints().Num(), Result.Get_Legs().Num(),
            FMath::RoundToInt(Result.Get_TotalCost())));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 3 — goal (any route ever requested)
    if (Status != ECk_PathNetwork_RouteStatus::None && Cfg.EnabledFields.HasTagExact(FieldTag_Goal()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Goal();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("Goal: {}"), Result.Get_GoalLocation()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity  = Entity;
    const auto Follower = UCk_Utils_PathNetworkFollower_UE::CastChecked(MutableEntity);

    if (ck::Is_NOT_Valid(Follower))
    { return {}; }

    return ck::Format_UE(TEXT("PathNet:{}"), UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(Follower));
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_PathNetworkFollower)

// --------------------------------------------------------------------------------------------------------------------
