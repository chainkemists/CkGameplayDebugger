#include "CkDebugOverlay_Provider_Aggro.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkAggro/CkAggro_Utils.h"
#include "CkAggro/CkAggroTarget_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Aggro,
    "Ck.OnScreenDebugger.Provider.Aggro")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Aggro_Value,
    "Ck.OnScreenDebugger.Provider.Aggro.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Aggro; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Aggro_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Aggro::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Aggro::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — this entity is an Aggro owner (holds the threat table).
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Aggro::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Aggro_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Emits up to two rows when Value is enabled:
//   Row 1 — the active target's tracked entity (or "(None)" when there is no active target)
//   Row 2 — the active target's threat + score (omitted when there is no active target)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Aggro::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto       MutableEntity = Entity;
    const auto Aggro         = UCk_Utils_Aggro_UE::CastChecked(MutableEntity);
    const auto ActiveTarget  = UCk_Utils_Aggro_UE::Get_ActiveTarget(Aggro);

    // Row 1 — active target's tracked entity
    {
        const auto TargetStr = ck::IsValid(ActiveTarget)
            ? ck::Format_UE(TEXT("Target: [{}]"), UCk_Utils_AggroTarget_UE::Get_TrackedEntity(ActiveTarget))
            : FString(TEXT("Target: (None)"));

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(TargetStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 2 — threat + score (only when there is an active target)
    if (ck::IsValid(ActiveTarget))
    {
        const auto Threat = UCk_Utils_AggroTarget_UE::Get_Threat(ActiveTarget);
        const auto Score  = UCk_Utils_AggroTarget_UE::Get_Score(ActiveTarget);
        const auto Str    = FString::Printf(TEXT("Threat: %.1f  Score: %.1f"), Threat, Score);

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(Str);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Aggro::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto       MutableEntity = Entity;
    const auto Aggro         = UCk_Utils_Aggro_UE::CastChecked(MutableEntity);
    const auto ActiveTarget  = UCk_Utils_Aggro_UE::Get_ActiveTarget(Aggro);

    if (NOT ck::IsValid(ActiveTarget))
    { return FString(TEXT("Aggro:idle")); }

    const auto Threat = UCk_Utils_AggroTarget_UE::Get_Threat(ActiveTarget);
    return FString::Printf(TEXT("Aggro:%.0f"), Threat);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Aggro)

// --------------------------------------------------------------------------------------------------------------------
