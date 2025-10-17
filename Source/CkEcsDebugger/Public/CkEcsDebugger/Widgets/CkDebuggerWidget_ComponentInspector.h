#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CkEcs/Handle/CkHandle.h"

class ICkDebuggerComponentInspector_Base
{
public:
    virtual ~ICkDebuggerComponentInspector_Base() = default;

    virtual auto Get_InspectorName() const -> FText = 0;
    virtual auto Get_SortPriority() const -> int32 = 0;
    virtual auto CanInspect(const FCk_Handle& InEntity) const -> bool = 0;
    virtual auto Build_InspectorContent(const FCk_Handle& InEntity) -> TSharedRef<SWidget> = 0;
};