#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_Inventories : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 75; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildInventoryGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;

    /** Cached total item count across all inventories for structural change detection */
    int32 _CachedTotalItemCount = -1;
};
