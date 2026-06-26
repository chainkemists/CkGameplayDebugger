#include "CkDebugOverlay_Provider_Transform.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Transform,
    "Ck.OnScreenDebugger.Provider.Transform")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Transform_Location,
    "Ck.OnScreenDebugger.Provider.Transform.Location")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Transform_Rotation,
    "Ck.OnScreenDebugger.Provider.Transform.Rotation")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Transform_Scale,
    "Ck.OnScreenDebugger.Provider.Transform.Scale")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()   { return TAG_Ck_OnScreenDebugger_Provider_Transform; }
    FGameplayTag FieldTag_Location() { return TAG_Ck_OnScreenDebugger_Provider_Transform_Location; }
    FGameplayTag FieldTag_Rotation() { return TAG_Ck_OnScreenDebugger_Provider_Transform_Rotation; }
    FGameplayTag FieldTag_Scale()    { return TAG_Ck_OnScreenDebugger_Provider_Transform_Scale; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Transform::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Transform::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Location(), true  },
        { FieldTag_Rotation(), true  },
        { FieldTag_Scale(),    false },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_Transform::CanInspect:
//   return ck::IsValid(Entity) && UCk_Utils_Transform_UE::Has(Entity);
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Transform::CanProvide(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Transform_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect — mirrors FCkInspector_Transform::Build_Inspector extraction.
//
// BATCH-VERIFY:
//   UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity)
//   returns FTransform. Verified from inspector source. Confirm the
//   TypeUnsafe variant is accessible from CkEntityDebugOverlay's module
//   (it lives in CkEcsExt which is already a dep).
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Transform::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT UCk_Utils_Transform_UE::Has(Entity)) { return; }

    const FTransform T = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Location()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Location();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"), T.GetLocation()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Rotation()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Rotation();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"), T.GetRotation().Rotator()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    if (Cfg.EnabledFields.HasTagExact(FieldTag_Scale()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Scale();
        Row.Value    = FText::FromString(ck::Format_UE(TEXT("{}"), T.GetScale3D()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Transform::Get_CompactToken(
    const FCk_Handle&                      /*Entity*/,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    // Transform is shown as the "T" badge on near plates and as the "[id]" header — it must
    // NOT emit a far-pill token, or every entity in the world spams "Loc:(x,y,z)" pills that
    // flood the view and drift away from their markers via the de-overlap stacking.
    return {};
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Transform)

// --------------------------------------------------------------------------------------------------------------------
