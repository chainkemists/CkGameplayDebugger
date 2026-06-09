#include "CkDebugOverlay_Provider_EntityInfo.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
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
    const FGameplayTag& ProviderTag()
    {
        return TAG_Ck_OnScreenDebugger_Provider_EntityInfo;
    }
    const FGameplayTag& FieldTag_Class()
    {
        return TAG_Ck_OnScreenDebugger_Provider_EntityInfo_Class;
    }
    const FGameplayTag& FieldTag_Lifetime()
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

    // --- Class (debug name) ---
    // Inspector reads: UCk_Utils_Handle_UE::Get_DebugName(E).ToString()
    // "Lifetime" is not directly exposed via the inspector — the inspector
    // shows Name/ID/Actor, not a numeric lifetime counter. We expose the
    // debug-name as "Class" (it typically encodes the script class leaf).
    // BATCH-VERIFY: confirm this is the intended mapping or add a
    // CkEntityLifetime fragment accessor for a tick/seconds counter.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Class()))
    {
        const FName DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Class();
        Row.Value    = FText::FromName(DebugName);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- Lifetime ---
    // The ECS inspector shows entity handle ID, not a wall-clock lifetime.
    // We emit the handle string as the "Lifetime" row since there is no
    // readily accessible lifetime-seconds accessor visible in CkEcs/CkEcsExt.
    // BATCH-VERIFY: locate FCk_Fragment_EntityLifetime or equivalent and
    // replace with Get_LifetimeSeconds() if it exists.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Lifetime()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Lifetime();
        Row.Value    = UCk_Utils_Handle_UE::Conv_HandleToText(Entity);
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
    return FString::Printf(TEXT("Info:%s"), *DebugName.ToString());
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_EntityInfo)

// --------------------------------------------------------------------------------------------------------------------
