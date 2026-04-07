#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment_Data.h"

class FCkInspector_InteractTarget : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 61; }
    auto IsMultiSection() const -> bool override { return true; }
    auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> override;
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildTargetWidget(const FCk_Handle_InteractTarget& InTarget) -> TSharedRef<SWidget>;

    int32 _LastTargetCount = 0;
};
