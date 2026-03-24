#include "CkInspector_Inventories.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkInventory/Inventory/CkInventory_Utils.h"
#include "CkInventory/Item/CkInventoryItem_Utils.h"
#include "CkInventory/Item/CkInventoryItem_Definition.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Inventories)

static const FLinearColor Color_InventoryName = FLinearColor(0.55f, 0.78f, 0.95f);
static const FLinearColor Color_InventoryType = FLinearColor(0.75f, 0.75f, 0.75f);
static const FLinearColor Color_ItemName = FLinearColor(0.85f, 0.75f, 0.55f);

// =====================================================================================================================

auto FCkInspector_Inventories::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Inventories"));
}

auto FCkInspector_Inventories::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Inventory_UE::Has_Any(Entity);
}

auto FCkInspector_Inventories::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildInventoryGrid(Entity, FString());
}

auto FCkInspector_Inventories::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildInventoryGrid(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_Inventories::BuildInventoryGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    const auto Inventories = UCk_Utils_Inventory_UE::RecordOfInventories_Utils::Get_ValidEntries(MutableEntity);

    for (const auto& InventoryHandle : Inventories)
    {
        if (ck::Is_NOT_Valid(InventoryHandle)) { continue; }

        const auto Inventory = UCk_Utils_Inventory_UE::CastChecked(InventoryHandle);

        // Inventory name from params
        const auto InventoryType = UCk_Utils_Inventory_UE::Get_InventoryType(Inventory);
        const auto IsSpatial = InventoryType == ECk_InventoryType::Spatial;
        const auto CapturedInventory = Inventory;

        // Clickable inventory header — navigates to the inventory entity
        Builder.AddClickableRow(
            FText::FromString(InventoryHandle.ToString()),
            [CapturedInventory, IsSpatial](const FCk_Handle& E)
            {
                const auto NumItems = UCk_Utils_Inventory_UE::Get_NumItems(CapturedInventory);
                const auto TypeStr = IsSpatial ? TEXT("Spatial") : TEXT("DataOnly");
                return FText::FromString(ck::Format_UE(TEXT("{} | {} items"), TypeStr, NumItems));
            },
            Color_InventoryName,
            [WeakSelectionModel, InventoryHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(InventoryHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ InventoryHandle });
                }
            });

        // List items in this inventory
        const auto Items = UCk_Utils_Inventory_UE::Get_Items(Inventory);
        for (const auto& ItemHandle : Items)
        {
            if (ck::Is_NOT_Valid(ItemHandle)) { continue; }

            const auto* Definition = UCk_Utils_InventoryItem_UE::Get_Definition(ItemHandle);
            const auto ItemName = (Definition != nullptr)
                ? Definition->Get_CoreInfo().Get_Name().ToString()
                : ItemHandle.ToString();

            const auto ItemEntity = FCk_Handle(ItemHandle);

            Builder.AddClickableRow(
                FText::FromString(ck::Format_UE(TEXT("  {}"), ItemName)),
                [](const FCk_Handle& E) { return FText::GetEmpty(); },
                Color_ItemName,
                [WeakSelectionModel, ItemEntity]()
                {
                    if (WeakSelectionModel.IsValid() && ck::IsValid(ItemEntity))
                    {
                        WeakSelectionModel->Set_SelectedEntities({ ItemEntity });
                    }
                });
        }
    }

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_Inventories::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
