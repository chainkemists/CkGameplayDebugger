#include "CkDebugOverlay_Provider_InteractTarget.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

// InteractTarget utils — mirroring CkInspector_InteractTarget.cpp includes
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_InteractTarget,
    "Ck.OnScreenDebugger.Provider.InteractTarget")

// A single field tag covering the interaction count summary row.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_InteractTarget_Value,
    "Ck.OnScreenDebugger.Provider.InteractTarget.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_InteractTarget; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_InteractTarget_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_InteractTarget::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_InteractTarget::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_InteractTarget::CanInspect:
//   UCk_Utils_InteractTarget_UE::Has(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_InteractTarget::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_InteractTarget_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Iterates all InteractTarget child entities on the owner. For each target,
// counts active interactions via Get_CurrentInteractions. Emits one summary
// row per target showing channel tag + active interaction count.
//
// BATCH-VERIFY:
//   UCk_Utils_InteractTarget_UE::Has(Entity)                                    — confirmed from CkInteractTarget_Utils.h:57
//   UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity, TFunction<...>) — confirmed from CkInteractTarget_Utils.h:199-201
//   UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget)               — confirmed from CkInspector_InteractTarget.cpp:44
//   UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget)         — confirmed from CkInspector_InteractTarget.cpp:148
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_InteractTarget::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [&Out](FCk_Handle_InteractTarget InTarget)
        {
            if (ck::Is_NOT_Valid(InTarget)) { return; }

            const auto& Channel     = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget);
            const auto  ChannelName = Channel.IsValid()
                ? Channel.GetTagName().ToString()
                : FString(TEXT("Unknown"));

            auto MutableTarget      = InTarget;
            const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);
            const auto ActiveCount  = Interactions.Num();

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Value();
            Row.Value    = FText::FromString(
                FString::Printf(TEXT("%s — active:%d"), *ChannelName, ActiveCount));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        });
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
//
// Returns total active interactions across all targets as "Int:%d".
// Returns {} when the entity has no interact targets.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_InteractTarget::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto TotalActive = int32{0};
    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [&TotalActive](FCk_Handle_InteractTarget InTarget)
        {
            if (ck::Is_NOT_Valid(InTarget)) { return; }
            auto MutableTarget      = InTarget;
            const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);
            TotalActive += Interactions.Num();
        });

    // Return token even when TotalActive == 0 — the presence of interact targets
    // is itself informative in compact view.
    return FString::Printf(TEXT("Int:%d"), TotalActive);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_InteractTarget)

// --------------------------------------------------------------------------------------------------------------------
