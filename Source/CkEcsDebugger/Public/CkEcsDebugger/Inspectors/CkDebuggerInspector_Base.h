#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;

class ICkDebuggerComponentInspector_Base
{
public:
    virtual ~ICkDebuggerComponentInspector_Base() = default;

    virtual auto Get_ComponentName() const -> FText = 0;
    virtual auto CanInspect(const FCk_Handle& Entity) const -> bool = 0;
    virtual auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> = 0;
    virtual auto Get_SortPriority() const -> int32 = 0;
    virtual auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void = 0;

    virtual auto IsFilterable() const -> bool { return false; }
    virtual auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> { return Build_Inspector(Entity); }

    virtual auto Set_SelectionModel(TSharedPtr<FCkDebuggerModel_EntitySelection> InModel) -> void { SelectionModel = MoveTemp(InModel); }
    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection> { return SelectionModel; }

protected:
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
};