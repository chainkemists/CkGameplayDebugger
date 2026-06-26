#include "CkDebugOverlay_Provider_Aggro.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Aggro utils — mirroring CkInspector_Aggro.cpp includes
#include "CkAggro/CkAggroOwner_Utils.h"
#include "CkAggro/CkAggro_Utils.h"

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
// CanProvide — mirrors FCkInspector_Aggro::CanInspect (AggroOwner branch):
//   UCk_Utils_AggroOwner_UE::Has(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Aggro::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_AggroOwner_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Emits up to two rows when Value is enabled:
//   Row 1 — best aggro target handle (or "(None)" when the table is empty)
//   Row 2 — score of the best aggro entry (omitted when no best target)
//
// BATCH-VERIFY:
//   UCk_Utils_AggroOwner_UE::Has(const FCk_Handle&)                       — confirmed from CkAggroOwner_Utils.h
//   UCk_Utils_AggroOwner_UE::CastChecked(FCk_Handle)                      — confirmed via CK_DEFINE_CPP_CASTCHECKED_TYPESAFE
//   UCk_Utils_AggroOwner_UE::Get_BestAggro(const FCk_Handle_AggroOwner&)  — confirmed from CkAggroOwner_Utils.h
//   UCk_Utils_Aggro_UE::Get_AggroScore(const FCk_Handle_Aggro&)           — confirmed from CkAggro_Utils.h, returns float
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

    auto MutableEntity        = Entity;
    const auto AggroOwner     = UCk_Utils_AggroOwner_UE::CastChecked(MutableEntity);
    const auto BestAggro      = UCk_Utils_AggroOwner_UE::Get_BestAggro(AggroOwner);

    // Row 1 — best target
    {
        const auto TargetStr = ck::IsValid(BestAggro)
            ? ck::Format_UE(TEXT("Target: [{}]"), FCk_Handle(BestAggro))
            : FString(TEXT("Target: (None)"));

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(TargetStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Row 2 — score (only when there is a best aggro)
    if (ck::IsValid(BestAggro))
    {
        const auto Score    = UCk_Utils_Aggro_UE::Get_AggroScore(BestAggro);
        const auto ScoreStr = FString::Printf(TEXT("Score: %.0f"), Score);

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(ScoreStr);
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

    auto MutableEntity    = Entity;
    const auto AggroOwner = UCk_Utils_AggroOwner_UE::CastChecked(MutableEntity);
    const auto BestAggro  = UCk_Utils_AggroOwner_UE::Get_BestAggro(AggroOwner);

    if (NOT ck::IsValid(BestAggro))
    { return FString(TEXT("Aggro:idle")); }

    const auto Score = UCk_Utils_Aggro_UE::Get_AggroScore(BestAggro);
    return FString::Printf(TEXT("Aggro:%.0f"), Score);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Aggro)

// --------------------------------------------------------------------------------------------------------------------
