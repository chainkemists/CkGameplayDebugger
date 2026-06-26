#include "CkDebugOverlay_Provider_Team.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Team utils — mirroring CkInspector_Relationships.cpp includes
#include "CkRelationship/Team/CkTeam_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Team,
    "Ck.OnScreenDebugger.Provider.Team")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Team_Value,
    "Ck.OnScreenDebugger.Provider.Team.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Team; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Team_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Team::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Team::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors CkInspector_Relationships team branch:
//   UCk_Utils_Team_UE::Has(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Team::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Team_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Casts the entity to FCk_Handle_Team and reads Get_ID().
// A single row shows the team id; the inspector comment notes ids start from zero.
//
// BATCH-VERIFY:
//   UCk_Utils_Team_UE::Has(Entity)              — confirmed from CkTeam_Utils.h line 142
//   UCk_Utils_Team_UE::Cast(Entity)             — confirmed from CkInspector_Relationships.cpp line 45
//   UCk_Utils_Team_UE::Get_ID(TeamEntity)       — confirmed from CkInspector_Relationships.cpp line 46
//   ck::Format_UE(TEXT("{} (Starts from ZERO)"), Id) — confirmed from CkInspector_Relationships.cpp line 46
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Team::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    const auto TeamEntity = UCk_Utils_Team_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(TeamEntity)) { return; }

    const auto Id = UCk_Utils_Team_UE::Get_ID(TeamEntity);
    const auto DisplayStr = ck::Format_UE(TEXT("{} (Starts from ZERO)"), Id);

    FCk_DebugOverlay_Row Row;
    Row.FieldTag = FieldTag_Value();
    Row.Value    = FText::FromString(DisplayStr);
    Row.Severity = ECk_DebugOverlay_Severity::Normal;
    Out.Rows.Add(MoveTemp(Row));
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Team::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    const auto TeamEntity = UCk_Utils_Team_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(TeamEntity)) { return {}; }

    const auto Id = UCk_Utils_Team_UE::Get_ID(TeamEntity);
    return FString::Printf(TEXT("Team:%s"), *ck::Format_UE(TEXT("{}"), Id));
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Team)

// --------------------------------------------------------------------------------------------------------------------
