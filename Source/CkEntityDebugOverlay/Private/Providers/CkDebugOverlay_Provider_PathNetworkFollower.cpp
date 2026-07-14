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
    "Ck.OnScreenDebugger.Provider.PathNetworkFollower.Route")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower; }
    FGameplayTag FieldTag_Route() { return TAG_Ck_OnScreenDebugger_Provider_PathNetworkFollower_Route; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_PathNetworkFollower::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Route(), true },
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

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Route()))
    { return; }

    auto MutableEntity  = Entity;
    const auto Follower = UCk_Utils_PathNetworkFollower_UE::CastChecked(MutableEntity);

    if (ck::Is_NOT_Valid(Follower))
    { return; }

    const auto Result = UCk_Utils_PathNetworkFollower_UE::Get_RouteResult(Follower);
    const auto Status = Result.Get_Status();

    // Row 1 — status
    {
        const auto StatusStr = Status == ECk_PathNetwork_RouteStatus::Failed
            ? ck::Format_UE(TEXT("Route: {} ({})"), Status, Result.Get_FailReason())
            : ck::Format_UE(TEXT("Route: {}"), Status);

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Route();
        Row.Value    = FText::FromString(StatusStr);
        Row.Severity = Status == ECk_PathNetwork_RouteStatus::Failed
            ? ECk_DebugOverlay_Severity::Warn
            : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 2 — corridor summary (Ready only)
    if (Status == ECk_PathNetwork_RouteStatus::Ready)
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Route();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("Corridor: {} wps / {} legs / cost {}"),
            Result.Get_CompiledWaypoints().Num(), Result.Get_Legs().Num(),
            FMath::RoundToInt(Result.Get_TotalCost())));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 3 — goal (any route ever requested)
    if (Status != ECk_PathNetwork_RouteStatus::None)
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Route();
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
