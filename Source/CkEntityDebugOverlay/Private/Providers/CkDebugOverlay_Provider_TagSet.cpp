#include "CkDebugOverlay_Provider_TagSet.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

// TagSet utils — mirroring CkInspector_TagSet.cpp includes
#include "CkTagSet/CkTagSet_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_TagSet,
    "Ck.OnScreenDebugger.Provider.TagSet")

// A single "catch-all" field tag for the tag listing.
// Each row shares this FieldTag; the tag name is embedded in the Value string.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_TagSet_Value,
    "Ck.OnScreenDebugger.Provider.TagSet.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_TagSet; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_TagSet_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_TagSet::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_TagSet::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_TagSet::CanInspect:
//   UCk_Utils_TagSet_UE::Has(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_TagSet::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_TagSet_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: all rows share FieldTag_Value(); the tag name is embedded
// in the Value string as the display text.
//
// The header row shows the total count ("Tags: N") so a collapsed overlay
// conveys cardinality at a glance. Additional per-tag rows list each tag name
// for expanded views.
//
// BATCH-VERIFY:
//   UCk_Utils_TagSet_UE::Has(Entity)                          — confirmed from CkInspector_TagSet.cpp
//   UCk_Utils_TagSet_UE::Cast(MutableEntity)                  — confirmed from CkInspector_TagSet.cpp
//   UCk_Utils_TagSet_UE::Get_NumTags(TagSetHandle)            — confirmed from CkTagSet_Utils.h
//   UCk_Utils_TagSet_UE::Get_Tags(TagSetHandle)               — confirmed from CkTagSet_Utils.h
//   FCk_Handle_TagSet cast requires non-const FCk_Handle ref  — confirmed from CkInspector_TagSet.cpp
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_TagSet::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    const auto TagSetHandle = UCk_Utils_TagSet_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(TagSetHandle)) { return; }

    const auto NumTags = UCk_Utils_TagSet_UE::Get_NumTags(TagSetHandle);

    // Count header row
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(FString::Printf(TEXT("Tags: %d"), NumTags));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // Individual tag rows
    const auto Tags = UCk_Utils_TagSet_UE::Get_Tags(TagSetHandle);
    for (const auto& Tag : Tags)
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(Tag.ToString());
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_TagSet::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    const auto TagSetHandle = UCk_Utils_TagSet_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(TagSetHandle)) { return {}; }

    const auto NumTags = UCk_Utils_TagSet_UE::Get_NumTags(TagSetHandle);
    if (NumTags == 0) { return {}; }

    return FString::Printf(TEXT("Tags:%d"), NumTags);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_TagSet)

// --------------------------------------------------------------------------------------------------------------------
