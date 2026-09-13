#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class FCkDebuggerModel_EntitySelection;
class SBox;
class SVerticalBox;
class SWindow;

namespace ck_inspector_inventories
{
    auto Get_DetachedLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>;
    auto Get_PendingRemovalLifetimeOwnedItems(const FCk_Handle& InInventory) -> TArray<FCk_Handle>;
}

class CKECSDEBUGGER_API SCkInspector_InventoriesAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_InventoriesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
        SLATE_ARGUMENT(TFunction<void(const FCk_Handle&)>, OpenSpatialGrid)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_InventoriesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_RecordsCollection() const -> TSharedPtr<FCkUiCollection> { return _Records; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_ForEntity(const FCk_Handle& InEntity) const -> bool { return _Entity == InEntity; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;

private:
    struct FRoute
    {
        FCk_Handle Inventory;
        FCk_Handle Item;
        bool IsInventory = false;
    };

    auto Build_AuthoredView() -> bool;
    auto Refresh_Records() -> bool;
    auto Resolve_Inventory(const FString& InKey, FCk_Handle& OutInventory) const -> bool;
    auto Resolve_Item(const FString& InKey, FCk_Handle& OutInventory, FCk_Handle& OutItem) const -> bool;
    auto Navigate(const FString& InKey) -> void;
    auto Open_Grid(const FString& InKey) -> void;
    auto Commit_Bound(const FString& InKey, int32 InValue) -> void;
    auto Commit_Consume(const FString& InKey, int32 InValue) -> void;
    auto Commit_Tag(const FString& InKey, const FText& InValue) -> void;
    auto Consume(const FString& InKey) -> void;
    auto Change_Tag(const FString& InKey, bool bInAdd) -> void;

    FCk_Handle _Entity;
    FString _Filter;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSet<FString> _DiffLabels;
    TFunction<void(const FCk_Handle&)> _OpenSpatialGrid;
    TSharedPtr<FCkUiCollection> _Records;
    TMap<FString, FRoute> _Routes;
    TMap<FString, int32> _PendingConsume;
    TMap<FString, FString> _PendingTags;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_Inventories : public ICkDebuggerComponentInspector_Base
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
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_AuthoredOrNative(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;
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
    TArray<TWeakPtr<SCkInspector_InventoriesAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;

    /** Tracks a single spatial-grid popup window keyed by its inventory handle. */
    struct FSpatialGridPopup
    {
        FCk_Handle          InventoryHandle;
        TWeakPtr<SWindow>   Window;
        TWeakPtr<SBox>      GridHost;
    };
    TArray<FSpatialGridPopup> _SpatialGridPopups;
};
