#include "CkDebugOverlay_Provider_VisualLod.h"

#include "NativeGameplayTags.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkVisualLod/CkVisualLod_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLod,                "Ck.OnScreenDebugger.Provider.VisualLod")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLod_Representation, "Ck.OnScreenDebugger.Provider.VisualLod.Representation")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLod_Fade,           "Ck.OnScreenDebugger.Provider.VisualLod.Fade")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_VisualLod_Locks,          "Ck.OnScreenDebugger.Provider.VisualLod.Locks")

namespace
{
    FGameplayTag ProviderTag()             { return TAG_Ck_OnScreenDebugger_Provider_VisualLod; }
    FGameplayTag FieldTag_Representation() { return TAG_Ck_OnScreenDebugger_Provider_VisualLod_Representation; }
    FGameplayTag FieldTag_Fade()           { return TAG_Ck_OnScreenDebugger_Provider_VisualLod_Fade; }
    FGameplayTag FieldTag_Locks()          { return TAG_Ck_OnScreenDebugger_Provider_VisualLod_Locks; }
}

auto FCk_DebugOverlay_Provider_VisualLod::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_VisualLod::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Representation(), true },
        { FieldTag_Fade(),           true },
        { FieldTag_Locks(),          true },
    };
}

auto FCk_DebugOverlay_Provider_VisualLod::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_VisualLod_UE::Has(Entity);
}

auto FCk_DebugOverlay_Provider_VisualLod::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    const auto VisualLod = UCk_Utils_VisualLod_UE::CastChecked(Entity);

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Representation()))
    {
        const auto Representation = UCk_Utils_VisualLod_UE::Get_Representation(VisualLod);
        const auto IsHidden       = UCk_Utils_VisualLod_UE::Get_IsHidden(VisualLod);
        const auto MemberIndex    = UCk_Utils_VisualLod_UE::Get_MemberIndex(VisualLod);

        auto Value = ck::Format_UE(TEXT("{}"), Representation);
        if (MemberIndex != INDEX_NONE)
        { Value = ck::Format_UE(TEXT("{} [slot {}]"), Representation, MemberIndex); }
        if (IsHidden)
        { Value = ck::Format_UE(TEXT("{} (HIDDEN)"), Value); }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Representation();
        Row.Value    = FText::FromString(Value);
        Row.Severity = IsHidden ? ECk_DebugOverlay_Severity::Warn : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Fade()))
    {
        const auto FadeAlpha = UCk_Utils_VisualLod_UE::Get_FadeAlpha(VisualLod);
        if (FadeAlpha < 1.0f)
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Fade();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("fading — member alpha {:.2f}"), FadeAlpha));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        }
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Locks()))
    {
        const auto LockCount = UCk_Utils_VisualLod_UE::Get_PromoteLockCount(VisualLod);
        if (LockCount > 0)
        {
            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Locks();
            Row.Value    = FText::FromString(ck::Format_UE(TEXT("promote locks: {}"), LockCount));
            Row.Severity = ECk_DebugOverlay_Severity::Warn;
            Out.Rows.Add(MoveTemp(Row));
        }
    }
}

auto FCk_DebugOverlay_Provider_VisualLod::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString
{
    const auto VisualLod = UCk_Utils_VisualLod_UE::CastChecked(Entity);

    switch (UCk_Utils_VisualLod_UE::Get_Representation(VisualLod))
    {
        case ECk_VisualLod_Representation::PromotedProxy: return TEXT("LOD:P");
        case ECk_VisualLod_Representation::FarMember:     return TEXT("LOD:F");
        default:                                          return TEXT("LOD:-");
    }
}

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_VisualLod)
