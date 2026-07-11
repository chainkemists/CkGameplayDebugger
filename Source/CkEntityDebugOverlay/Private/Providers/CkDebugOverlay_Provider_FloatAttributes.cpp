#include "CkDebugOverlay_Provider_FloatAttributes.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Attribute utils — mirroring CkInspector_FloatAttributes.cpp includes
#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_FloatAttributes,
    "Ck.OnScreenDebugger.Provider.FloatAttributes")

// A single "catch-all" field tag for the collection.
// Each attribute row uses this tag as its FieldTag, with the attribute name embedded in the value.
// This avoids synthesizing dynamic per-attribute gameplay tags at runtime (which would require
// RequestGameplayTag string-to-tag registration for every attribute name on every frame).
// Rendering: [FLOATATTR] [AttributeName value]  — key chip shows the attribute name, value shows the number.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_FloatAttributes_Value,
    "Ck.OnScreenDebugger.Provider.FloatAttributes.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()      { return TAG_Ck_OnScreenDebugger_Provider_FloatAttributes; }
    FGameplayTag FieldTag_Value()   { return TAG_Ck_OnScreenDebugger_Provider_FloatAttributes_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_FloatAttributes::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_FloatAttributes::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_FloatAttributes::CanInspect:
//   UCk_Utils_FloatAttribute_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_FloatAttributes::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_FloatAttribute_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: each attribute row shares the single FieldTag_Value() tag.
// The attribute name is placed as the VALUE prefix "[AttrName] number" so the
// key chip renders the attribute label naturally.
//
// Row layout rendered by the card: [FLOATATTR]  [VALUE AttrName: 42.00]  ...
// Because all rows share the same FieldTag, history diff-tracking buckets them
// separately only if you combine {EntityId, FieldTag} — since all share the same
// FieldTag, history comparisons merge across attributes. This is acceptable for
// the overlay (it's a read-only live debug view; separate history per attribute
// would require dynamic tag synthesis). Flag for future improvement if needed.
//
// EntryFilter: if Cfg.EntryFilter is non-empty, it is matched against the
// attribute's gameplay label tag. If the attribute has no label tag or the
// query doesn't match, the attribute is skipped.
//
// BATCH-VERIFY:
//   UCk_Utils_FloatAttribute_UE::Has_Any(Entity)                          — confirmed from CkInspector_FloatAttributes.cpp
//   UCk_Utils_FloatAttribute_UE::ForEach(Entity, TFunction<...>)         — confirmed from CkInspector_FloatAttributes.cpp
//   UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute)                   — confirmed from CkInspector_FloatAttributes.cpp
//   UCk_Utils_FloatAttribute_UE::Get_FinalValue(Attr, ECk_MinMaxCurrent::Current) — confirmed
//   FCk_Handle(InAttribute) cast from typed handle                        — confirmed from CkInspector_FloatAttributes.cpp
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_FloatAttributes::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    UCk_Utils_FloatAttribute_UE::ForEach(MutableEntity,
        [&Out, &Cfg](FCk_Handle_FloatAttribute InAttribute)
        {
            const auto AttributeTag  = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto AttributeName = AttributeTag.IsValid()
                ? AttributeTag.GetTagName().ToString()
                : FString(TEXT("Unnamed"));

            // User allow/deny filter (settings; editable live from the ECS Debugger popover).
            if (NOT UCk_DebugOverlay_Settings::Get_PassesAttributeFilter(AttributeName))
            { return; }

            // Apply EntryFilter: if a query is set, skip attributes whose label tag doesn't match.
            if (NOT Cfg.EntryFilter.IsEmpty())
            {
                if (NOT AttributeTag.IsValid() || NOT Cfg.EntryFilter.Matches(FGameplayTagContainer(AttributeTag)))
                { return; }
            }

            const auto FinalVal = UCk_Utils_FloatAttribute_UE::Get_FinalValue(
                InAttribute, ECk_MinMaxCurrent::Current);
            const auto BaseVal  = UCk_Utils_FloatAttribute_UE::Get_BaseValue(
                InAttribute, ECk_MinMaxCurrent::Current);

            auto DisplayStr = AttributeName + TEXT(": ") + ck::Format_UE(TEXT("{:.2f}"), FinalVal);

            if (NOT FMath::IsNearlyEqual(BaseVal, FinalVal, 0.001f))
            {
                DisplayStr += ck::Format_UE(TEXT(" (base {:.2f})"), BaseVal);
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

auto FCk_DebugOverlay_Provider_FloatAttributes::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    // Show count of float attributes in compact mode.
    auto MutableEntity = Entity;
    int32 Count = 0;
    UCk_Utils_FloatAttribute_UE::ForEach(MutableEntity,
        [&Count](FCk_Handle_FloatAttribute) { ++Count; });

    if (Count == 0) { return {}; }
    return FString::Printf(TEXT("F:%dattr"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_FloatAttributes)

// --------------------------------------------------------------------------------------------------------------------
