#include "CkDebugOverlay_Provider_RotatorAttributes.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Attribute utils — mirroring the Float provider's include shape
#include "CkAttribute/RotatorAttribute/CkRotatorAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_RotatorAttributes,
    "Ck.OnScreenDebugger.Provider.RotatorAttributes")

// Single catch-all field tag — same rationale as the Float provider: attribute names are
// embedded in the row value instead of synthesizing per-attribute gameplay tags at runtime.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_RotatorAttributes_Value,
    "Ck.OnScreenDebugger.Provider.RotatorAttributes.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_RotatorAttributes; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_RotatorAttributes_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_RotatorAttributes::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_RotatorAttributes::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_RotatorAttributes::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_RotatorAttribute_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_RotatorAttributes::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    UCk_Utils_RotatorAttribute_UE::ForEach(MutableEntity,
        [&Out, &Cfg](FCk_Handle_RotatorAttribute InAttribute)
        {
            const auto AttributeTag  = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto AttributeName = AttributeTag.IsValid()
                ? AttributeTag.GetTagName().ToString()
                : FString(TEXT("Unnamed"));

            // User allow/deny filter (settings; editable live from the ECS Debugger popover).
            if (NOT UCk_DebugOverlay_Settings::Get_PassesAttributeFilter(AttributeName))
            { return; }

            if (NOT Cfg.EntryFilter.IsEmpty())
            {
                if (NOT AttributeTag.IsValid() || NOT Cfg.EntryFilter.Matches(FGameplayTagContainer(AttributeTag)))
                { return; }
            }

            const auto FinalVal = UCk_Utils_RotatorAttribute_UE::Get_FinalValue(
                InAttribute, ECk_MinMaxCurrent::Current);

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Value();
            Row.Value    = FText::FromString(AttributeName + TEXT(": ") +
                ck::Format_UE(TEXT("(P{:.1f} Y{:.1f} R{:.1f})"), FinalVal.Pitch, FinalVal.Yaw, FinalVal.Roll));
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_RotatorAttributes::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    int32 Count = 0;
    UCk_Utils_RotatorAttribute_UE::ForEach(MutableEntity,
        [&Count](FCk_Handle_RotatorAttribute) { ++Count; });

    if (Count == 0) { return {}; }
    return FString::Printf(TEXT("R:%dattr"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_RotatorAttributes)

// --------------------------------------------------------------------------------------------------------------------
