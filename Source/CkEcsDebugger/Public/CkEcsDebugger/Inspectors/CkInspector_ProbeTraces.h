#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment_Data.h"

class FCkInspector_ProbeTraces : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 75; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto BuildTraceGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto DisableDebugDraw() -> void;

    FCk_Handle LastInspectedEntity;
};
