#include "CkDebugOverlay_Provider_Label.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Label,
    "Ck.OnScreenDebugger.Provider.Label")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Label_Value,
    "Ck.OnScreenDebugger.Provider.Label.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Label; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Label_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Label::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Label::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — UCk_Utils_GameplayLabel_UE::Has(Entity)
// BATCH-VERIFY: Has(const FCk_Handle&) confirmed from CkLabel_Utils.h line 32.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Label::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_GameplayLabel_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Reads the entity's gameplay label tag and emits one row showing the leaf
// segment of the tag name (e.g. "Ck.Game.Role.Scout" → "Scout").
//
// BATCH-VERIFY:
//   UCk_Utils_GameplayLabel_UE::Has(const FCk_Handle&)       — confirmed CkLabel_Utils.h:32
//   UCk_Utils_GameplayLabel_UE::Get_Label(const FCk_Handle&) — confirmed CkLabel_Utils.h:47
//   FGameplayTag::GetTagName()                               — standard UE API
//   FName::ToString()                                        — standard UE API
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Label::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    const auto LabelTag = UCk_Utils_GameplayLabel_UE::Get_Label(Entity);
    if (NOT LabelTag.IsValid()) { return; }

    const FString FullName = LabelTag.GetTagName().ToString();
    int32 LastDot = INDEX_NONE;
    FullName.FindLastChar(TEXT('.'), LastDot);
    const FString Leaf = (LastDot != INDEX_NONE)
        ? FullName.Mid(LastDot + 1)
        : FullName;

    FCk_DebugOverlay_Row Row;
    Row.FieldTag = FieldTag_Value();
    Row.Value    = FText::FromString(Leaf);
    Row.Severity = ECk_DebugOverlay_Severity::Normal;
    Out.Rows.Add(MoveTemp(Row));
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken — "Lbl:<leaf>" e.g. "Lbl:Scout"
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Label::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }
    if (NOT UCk_Utils_GameplayLabel_UE::Has(Entity)) { return {}; }

    const auto LabelTag = UCk_Utils_GameplayLabel_UE::Get_Label(Entity);
    if (NOT LabelTag.IsValid()) { return {}; }

    const FString FullName = LabelTag.GetTagName().ToString();
    int32 LastDot = INDEX_NONE;
    FullName.FindLastChar(TEXT('.'), LastDot);
    const FString Leaf = (LastDot != INDEX_NONE)
        ? FullName.Mid(LastDot + 1)
        : FullName;

    if (Leaf.IsEmpty()) { return {}; }
    return FString::Printf(TEXT("Lbl:%s"), *Leaf);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Label)

// --------------------------------------------------------------------------------------------------------------------
