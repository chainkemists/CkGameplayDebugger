#include "CkDebugOverlay_Provider_EntityCollection.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

// EntityCollection utils — mirroring CkInspector_EntityCollections.cpp includes
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_EntityCollection,
    "Ck.OnScreenDebugger.Provider.EntityCollection")

// A single "catch-all" field tag for the collection.
// Each collection row uses this tag as its FieldTag, with the collection name and entity count
// embedded in the value string.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_EntityCollection_Value,
    "Ck.OnScreenDebugger.Provider.EntityCollection.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_EntityCollection; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_EntityCollection_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityCollection::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_EntityCollection::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_EntityCollections::CanInspect:
//   UCk_Utils_EntityCollection_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityCollection::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_EntityCollection_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: all rows share FieldTag_Value() (same rationale as FloatAttributes —
// avoids runtime dynamic tag synthesis for each collection name).
// Each row shows "[CollectionName]: N entities".
//
// BATCH-VERIFY:
//   UCk_Utils_EntityCollection_UE::Has_Any(Entity)                                   — confirmed from CkInspector_EntityCollections.cpp
//   UCk_Utils_EntityCollection_UE::ForEach_EntityCollection(FCk_Handle&, TFunction)  — confirmed from CkEntityCollection_Utils.h
//   UCk_Utils_GameplayLabel_UE::Get_Label(InCollection)                              — confirmed from CkInspector_EntityCollections.cpp
//   UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(FCk_Handle_EntityCollection) — confirmed from CkEntityCollection_Utils.h + inspector
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_EntityCollection::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    UCk_Utils_EntityCollection_UE::ForEach_EntityCollection(MutableEntity,
        [&Out](FCk_Handle_EntityCollection InCollection)
        {
            const auto CollectionTag  = UCk_Utils_GameplayLabel_UE::Get_Label(InCollection);
            const auto CollectionName = CollectionTag.IsValid()
                ? CollectionTag.GetTagName().ToString()
                : FString(TEXT("Unnamed"));

            const auto Count = UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(InCollection);

            const auto DisplayStr = FString::Printf(TEXT("%s: %d entities"), *CollectionName, Count);

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

auto FCk_DebugOverlay_Provider_EntityCollection::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    // Sum entity counts across all collections for the compact pill.
    auto MutableEntity = Entity;
    int32 TotalEntities = 0;
    UCk_Utils_EntityCollection_UE::ForEach_EntityCollection(MutableEntity,
        [&TotalEntities](FCk_Handle_EntityCollection InCollection)
        {
            TotalEntities += UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(InCollection);
        });

    if (TotalEntities == 0) { return {}; }
    return FString::Printf(TEXT("Col:%d"), TotalEntities);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_EntityCollection)

// --------------------------------------------------------------------------------------------------------------------
