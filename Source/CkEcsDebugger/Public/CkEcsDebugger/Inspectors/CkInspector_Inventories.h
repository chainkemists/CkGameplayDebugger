#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class SBox;
class SWindow;

class FCkInspector_Inventories : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Inventories() override;

    auto Get_ComponentName() const -> FText override;
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 75; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto BuildInventoryGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;
    auto Build_SpatialGridContent(const FCk_Handle& InInventoryHandle) -> TSharedRef<SWidget>;
    auto RefreshSpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void;
    auto OpenOrFocus_SpatialGridPopup(const FCk_Handle& InInventoryHandle) -> void;
    auto Close_AllSpatialGridPopups() -> void;

    /** Cached total item count across all inventories for structural change detection */
    int32 _CachedTotalItemCount = -1;

    /** Tracks a single spatial-grid popup window keyed by its inventory handle. */
    struct FSpatialGridPopup
    {
        FCk_Handle          InventoryHandle;
        TWeakPtr<SWindow>   Window;
        TWeakPtr<SBox>      GridHost;
    };
    TArray<FSpatialGridPopup> _SpatialGridPopups;
};
