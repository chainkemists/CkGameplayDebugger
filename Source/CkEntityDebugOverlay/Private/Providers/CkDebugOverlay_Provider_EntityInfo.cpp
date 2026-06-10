#include "CkDebugOverlay_Provider_EntityInfo.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags — provider tag + one leaf per field
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_EntityInfo,
    "Ck.OnScreenDebugger.Provider.EntityInfo")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_EntityInfo_Class,
    "Ck.OnScreenDebugger.Provider.EntityInfo.Class")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_EntityInfo_Lifetime,
    "Ck.OnScreenDebugger.Provider.EntityInfo.Lifetime")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()
    {
        return TAG_Ck_OnScreenDebugger_Provider_EntityInfo;
    }
    FGameplayTag FieldTag_Class()
    {
        return TAG_Ck_OnScreenDebugger_Provider_EntityInfo_Class;
    }
    FGameplayTag FieldTag_Lifetime()
    {
        return TAG_Ck_OnScreenDebugger_Provider_EntityInfo_Lifetime;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityInfo::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_EntityInfo::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Class(),    true  },
        { FieldTag_Lifetime(), true  },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_EntityInfo::CanInspect:
//   return ck::IsValid(Entity);
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityInfo::CanProvide(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityInfo::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    // --- Class (cleaned debug name) ---
    // Default__/_C/_0 noise and embedded handle echoes are stripped — the raw
    // form ("SCENE NODE: [37|0(37)(Default__…)]") tripled the card width.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Class()))
    {
        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Class();
        Row.Value    = FText::FromString(ck::DebugNameClean::Get_CleanName(DebugName.ToString()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- Lifetime (owner) ---
    // The entity's own handle is already in the card header — echoing it here was
    // pure duplication. Show the lifetime OWNER instead: that's the one piece of
    // identity the header does NOT carry.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Lifetime()))
    {
        auto OwnerStr = FString{};

        const auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Entity);
        if (ck::Is_NOT_Valid(LifetimeOwner) ||
            UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(LifetimeOwner))
        {
            OwnerStr = TEXT("top-level");
        }
        else
        {
            const auto OwnerName = UCk_Utils_Handle_UE::Get_DebugName(LifetimeOwner);
            const auto OwnerNum  = static_cast<uint32>(LifetimeOwner.Get_Entity().Get_EntityNumber());
            OwnerStr = OwnerName.IsNone()
                ? FString::Printf(TEXT("→ #%u"), OwnerNum)
                : FString::Printf(TEXT("→ %s"), *ck::DebugNameClean::Get_CleanName(OwnerName.ToString()));
        }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Lifetime();
        Row.Value    = FText::FromString(OwnerStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityInfo::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }
    const FName DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
    return FString::Printf(TEXT("Info:%s"), *ck::DebugNameClean::Get_CleanName(DebugName.ToString()));
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_EntityInfo)

// --------------------------------------------------------------------------------------------------------------------
