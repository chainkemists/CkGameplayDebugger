#include "CkDebugOverlay_Provider_Inventory.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Inventory utils — mirroring CkInspector_Inventories.cpp includes
#include "CkInventory/Inventory/CkInventory_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Inventory,
    "Ck.OnScreenDebugger.Provider.Inventory")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Inventory_Value,
    "Ck.OnScreenDebugger.Provider.Inventory.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Inventory; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Inventory_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Inventory::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Inventory::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_Inventories::CanInspect:
//   UCk_Utils_Inventory_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Inventory::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Inventory_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Enumerates every inventory on the entity via ForEach_Inventories (C++ TFunction overload).
// Each inventory contributes one row: "Inv[i]: N items".
// Rows share the single FieldTag_Value() tag (same pattern as FloatAttributes).
//
// BATCH-VERIFY:
//   UCk_Utils_Inventory_UE::Has_Any(Entity)                                        — confirmed CkInspector_Inventories.cpp:80
//   UCk_Utils_Inventory_UE::ForEach_Inventories(FCk_Handle&, TFunction<void(FCk_Handle_Inventory&)>) — confirmed CkInventory_Utils.h:162
//   UCk_Utils_Inventory_UE::Get_NumItems(const FCk_Handle_Inventory&) -> int32     — confirmed CkInventory_Utils.h:177
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Inventory::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    auto InvIndex      = 0;

    UCk_Utils_Inventory_UE::ForEach_Inventories(MutableEntity,
        [&Out, &InvIndex](FCk_Handle_Inventory& InInventory)
        {
            if (ck::Is_NOT_Valid(InInventory)) { return; }

            const auto NumItems  = UCk_Utils_Inventory_UE::Get_NumItems(InInventory);
            const auto DisplayStr = FString::Printf(TEXT("Inv[%d]: %d item%s"),
                InvIndex, NumItems, NumItems == 1 ? TEXT("") : TEXT("s"));

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Value();
            Row.Value    = FText::FromString(DisplayStr);
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));

            ++InvIndex;
        });
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken — "Inv:%d" showing total item count across all inventories.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Inventory::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    auto TotalItems    = 0;

    UCk_Utils_Inventory_UE::ForEach_Inventories(MutableEntity,
        [&TotalItems](FCk_Handle_Inventory& InInventory)
        {
            if (ck::Is_NOT_Valid(InInventory)) { return; }
            TotalItems += UCk_Utils_Inventory_UE::Get_NumItems(InInventory);
        });

    if (TotalItems == 0) { return {}; }
    return FString::Printf(TEXT("Inv:%d"), TotalItems);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Inventory)

// --------------------------------------------------------------------------------------------------------------------
