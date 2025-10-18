#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "Widgets/SCompoundWidget.h"

class ICkDebuggerComponentInspector_Base
{
public:
    virtual ~ICkDebuggerComponentInspector_Base() = default;

    virtual auto Get_ComponentName() const -> FText = 0;
    virtual auto CanInspect(const FCk_Handle& Entity) const -> bool = 0;
    virtual auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> = 0;
    virtual auto Get_SortPriority() const -> int32 = 0;
    virtual auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void = 0;
};