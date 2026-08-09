#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspector_Inventories.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkInventory/Inventory/DataOnly/CkInventory_DataOnly_Utils.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkItem_Fragment.h"
#include "CkInventory/Item/CkItem_Utils.h"

#include "Widgets/SBoxPanel.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerInventoryInspector_DirectInventoryIsInspectable,
    "Ck.EcsDebugger.Inspectors.InventoryVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerInventoryInspector_DirectInventoryIsInspectable::RunTest(const FString&)
{
    TestTrue(
        TEXT("panel ticks a currently applicable inspector"),
        ck_debugger_panel_inspector::Should_TickInspector(true, false));
    TestTrue(
        TEXT("panel permits an opted-in cleanup tick after applicability is lost"),
        ck_debugger_panel_inspector::Should_TickInspector(false, true));
    TestFalse(
        TEXT("panel does not tick an inapplicable inspector without an opt-in"),
        ck_debugger_panel_inspector::Should_TickInspector(false, false));

    auto World = ck::FEcsWorld{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    const auto UnrelatedEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());

    const auto Params = UCk_Utils_Inventory_DataOnly_UE::Make_Params_Bounded(
        FGameplayTag{},
        1,
        FCk_Delegate_Inventory_CustomCanAcceptItem_Dynamic{},
        FCk_Delegate_Inventory_CustomCanStackItems_Dynamic{});
    auto Inventory = UCk_Utils_Inventory_DataOnly_UE::Add(
        Owner,
        Params,
        ECk_Replication::DoesNotReplicate);

    TestTrue(TEXT("fixture creates an inventory"), ck::IsValid(Inventory));

    const auto& Registry = FCkDebuggerInspectorRegistry::Get();
    const auto InspectorID = FName(TEXT("FCkInspector_Inventories"));
    TestTrue(TEXT("inventory owner exposes the inventory inspector"), Registry.Test(InspectorID, Owner));
    TestTrue(TEXT("direct inventory exposes the inventory inspector"), Registry.Test(InspectorID, Inventory));
    TestFalse(TEXT("unrelated entity does not expose the inventory inspector"), Registry.Test(InspectorID, UnrelatedEntity));

    auto ActiveItemEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Inventory);
    ActiveItemEntity.Add<ck::FFragment_InventoryItem>();
    auto ActiveItem = UCk_Utils_Item_UE::Cast(ActiveItemEntity);
    auto InventoryEntity = FCk_Handle{Inventory};
    UCk_Utils_Inventory_UE::RecordOfInventoryItems_Utils::Request_Connect(
        InventoryEntity,
        ActiveItem,
        ECk_Record_LabelRequirementPolicy::Optional);
    ck::TUtils_Item_ParentInventory::AddOrReplace(ActiveItem, Inventory);

    auto DetachedItemEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Inventory);
    DetachedItemEntity.Add<ck::FFragment_InventoryItem>();

    auto PendingRemovalItemEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Inventory);
    PendingRemovalItemEntity.Add<ck::FFragment_InventoryItem>();
    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(PendingRemovalItemEntity);

    TestEqual(TEXT("fixture has one authoritative inventory member"), UCk_Utils_Inventory_UE::Get_NumItems(Inventory), 1);
    TestEqual(
        TEXT("inspector reports the additional lifetime-owned Item child as detached"),
        ck_inspector_inventories::Get_DetachedLifetimeOwnedItems(Inventory).Num(),
        1);
    TestEqual(
        TEXT("inspector distinguishes a lifetime-owned Item that is already being destroyed"),
        ck_inspector_inventories::Get_PendingRemovalLifetimeOwnedItems(Inventory).Num(),
        1);

    auto Inspector = FCkInspector_Inventories{};
    const auto RenderedInspector = Inspector.Build_Inspector(Owner);
    TestEqual(TEXT("rendered inventory starts with header and item rows"), RenderedInspector->GetChildren()->Num(), 2);

    UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Request_Disconnect(Owner, Inventory);
    TestFalse(TEXT("owner no longer has an intrinsically inspectable inventory"), Registry.Test(InspectorID, Owner));
    TestTrue(
        TEXT("rendered inventory opts into a cleanup tick after its last inventory is removed"),
        Inspector.Wants_TickWhenNotInspectable(Owner));

    Inspector.Tick(Owner, 0.0f);
    TestEqual(TEXT("cleanup tick replaces stale inventory rows with an empty-state row"), RenderedInspector->GetChildren()->Num(), 1);

    Inspector.OnDeactivated();
    TestFalse(
        TEXT("deactivated inventory inspector does not request inapplicable ticks"),
        Inspector.Wants_TickWhenNotInspectable(Owner));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
