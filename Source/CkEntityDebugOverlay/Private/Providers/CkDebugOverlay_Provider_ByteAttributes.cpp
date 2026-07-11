#include "CkDebugOverlay_Provider_ByteAttributes.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Attribute utils — mirroring CkInspector_ByteAttributes.cpp includes
#include "CkAttribute/ByteAttribute/CkByteAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_ByteAttributes,
    "Ck.OnScreenDebugger.Provider.ByteAttributes")

// A single "catch-all" field tag for the collection (same rationale as FloatAttributes/IntegerAttributes).
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_ByteAttributes_Value,
    "Ck.OnScreenDebugger.Provider.ByteAttributes.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()      { return TAG_Ck_OnScreenDebugger_Provider_ByteAttributes; }
    FGameplayTag FieldTag_Value()   { return TAG_Ck_OnScreenDebugger_Provider_ByteAttributes_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_ByteAttributes::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_ByteAttributes::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_ByteAttributes::CanInspect:
//   UCk_Utils_ByteAttribute_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_ByteAttributes::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_ByteAttribute_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: same as FloatAttributes/IntegerAttributes — all rows share FieldTag_Value().
// Attribute name is embedded in the Value string as "AttrName: 42".
//
// EntryFilter: if non-empty, matched against the attribute's gameplay label tag.
//
// BATCH-VERIFY:
//   UCk_Utils_ByteAttribute_UE::Has_Any(Entity)                             — confirmed from CkInspector_ByteAttributes.cpp
//   UCk_Utils_ByteAttribute_UE::ForEach(Entity, TFunction<...>)            — confirmed from CkByteAttribute_Utils.h
//   UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute)                      — confirmed from CkInspector_ByteAttributes.cpp
//   UCk_Utils_ByteAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent)    — confirmed from CkByteAttribute_Utils.h
//   UCk_Utils_ByteAttribute_UE::Get_BaseValue(Attr, ECk_MinMaxCurrent)     — confirmed from CkByteAttribute_Utils.h
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_ByteAttributes::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    UCk_Utils_ByteAttribute_UE::ForEach(MutableEntity,
        [&Out, &Cfg](FCk_Handle_ByteAttribute InAttribute)
        {
            const auto AttributeTag  = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto AttributeName = AttributeTag.IsValid()
                ? AttributeTag.GetTagName().ToString()
                : FString(TEXT("Unnamed"));

            // Apply EntryFilter: if a query is set, skip attributes whose label tag doesn't match.
            // User allow/deny filter (settings; editable live from the ECS Debugger popover).
            if (NOT UCk_DebugOverlay_Settings::Get_PassesAttributeFilter(AttributeName))
            { return; }

            if (NOT Cfg.EntryFilter.IsEmpty())
            {
                if (NOT AttributeTag.IsValid() || NOT Cfg.EntryFilter.Matches(FGameplayTagContainer(AttributeTag)))
                { return; }
            }

            const auto FinalVal = UCk_Utils_ByteAttribute_UE::Get_FinalValue(
                InAttribute, ECk_MinMaxCurrent::Current);
            const auto BaseVal  = UCk_Utils_ByteAttribute_UE::Get_BaseValue(
                InAttribute, ECk_MinMaxCurrent::Current);

            auto DisplayStr = AttributeName + TEXT(": ") + ck::Format_UE(TEXT("{}"), FinalVal);

            if (BaseVal != FinalVal)
            {
                DisplayStr += ck::Format_UE(TEXT(" (base {})"), BaseVal);
            }

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Value();
            Row.Value    = FText::FromString(DisplayStr);
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        });
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_ByteAttributes::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    // Show count of byte attributes in compact mode.
    auto MutableEntity = Entity;
    int32 Count = 0;
    UCk_Utils_ByteAttribute_UE::ForEach(MutableEntity,
        [&Count](FCk_Handle_ByteAttribute) { ++Count; });

    if (Count == 0) { return {}; }
    return FString::Printf(TEXT("B:%dattr"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_ByteAttributes)

// --------------------------------------------------------------------------------------------------------------------
