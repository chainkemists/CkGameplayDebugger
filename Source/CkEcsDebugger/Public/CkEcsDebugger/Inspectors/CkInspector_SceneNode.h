#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/Layout/SWrapBox.h"

class FCkInspector_SceneNode : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 22; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    TSharedPtr<SWrapBox> _SiblingsBox;
    int32 _LastSiblingCount = -1;
};
