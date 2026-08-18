#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class SBox;
class SVerticalBox;
class SWindow;

namespace ck_inspector_inventories
{
    auto Get_DetachedLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>;
    auto Get_PendingRemovalLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>;
}

class FCkInspector_Inventories : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Inventories() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Inventory; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A9803C"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 75; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool override;
    auto OnDeactivated() -> void override;

private:
    auto BuildInventoryGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;
    struct FInventoryViewState;
    auto PopulateInventoryGrid(FInventoryViewState& InViewState) -> void;
    auto PopulateInventoryItemRows(SVerticalBox& InHost, const FCk_Handle& InInventory, const FString& InFilter) -> void;
    auto Build_SpatialGridContent(const FCk_Handle& InInventoryHandle) -> TSharedRef<SWidget>;
    auto RefreshSpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void;
    auto OpenOrFocus_SpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void;
    auto Close_AllSpatialGridPopups() -> void;

    struct FInventoryItemRowsHost
    {
        FCk_Handle Inventory;
        TWeakPtr<SVerticalBox> Host;
    };

    struct FInventoryViewState
    {
        FCk_Handle Entity;
        TArray<FCk_Handle> CachedStructureHandles;
        TArray<FCk_Handle> CachedInventoryHandles;
        FString ActiveFilter;
        TWeakPtr<SVerticalBox> InventoryGridHost;
        TArray<FInventoryItemRowsHost> InventoryItemRowsHosts;
    };
    TArray<FInventoryViewState> _ViewStates;

    /** Tracks a single spatial-grid popup window keyed by its inventory handle. */
    struct FSpatialGridPopup
    {
        FCk_Handle          InventoryHandle;
        TWeakPtr<SWindow>   Window;
        TWeakPtr<SBox>      GridHost;
    };
    TArray<FSpatialGridPopup> _SpatialGridPopups;
};
