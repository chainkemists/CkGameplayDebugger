#include "CkDebugOverlay_Provider_VisualLodArbiter.h"

#include "NativeGameplayTags.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkVisualLod/CkVisualLodArbiter_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter,          "Ck.OnScreenDebugger.Provider.VisualLodArbiter")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter_Budgets,  "Ck.OnScreenDebugger.Provider.VisualLodArbiter.Budgets")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter_Observer, "Ck.OnScreenDebugger.Provider.VisualLodArbiter.Observer")

namespace
{
    FGameplayTag ProviderTag()       { return TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter; }
    FGameplayTag FieldTag_Budgets()  { return TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter_Budgets; }
    FGameplayTag FieldTag_Observer() { return TAG_Ck_OnScreenDebugger_Provider_VisualLodArbiter_Observer; }
}

auto FCk_DebugOverlay_Provider_VisualLodArbiter::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_VisualLodArbiter::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Budgets(),  true },
        { FieldTag_Observer(), true },
    };
}

auto FCk_DebugOverlay_Provider_VisualLodArbiter::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_VisualLodArbiter_UE::Has(Entity);
}

auto FCk_DebugOverlay_Provider_VisualLodArbiter::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    const auto Arbiter = UCk_Utils_VisualLodArbiter_UE::CastChecked(Entity);

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Budgets()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Budgets();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("promoted {} (near {} | locked {} | unbudgeted {})"),
            UCk_Utils_VisualLodArbiter_UE::Get_PromotedCount(Arbiter),
            UCk_Utils_VisualLodArbiter_UE::Get_NearPromotedCount(Arbiter),
            UCk_Utils_VisualLodArbiter_UE::Get_LockedPromotedCount(Arbiter),
            UCk_Utils_VisualLodArbiter_UE::Get_UnbudgetedPromotedCount(Arbiter)));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Observer()))
    {
        const auto Observer = UCk_Utils_VisualLodArbiter_UE::Get_Observer(Arbiter);

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Observer();
        Row.Value    = FText::FromString(ck::IsValid(Observer)
            ? ck::Format_UE(TEXT("observer: {}"), Observer)
            : FString{TEXT("observer: local-view discovery")});
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_VisualLodArbiter)
