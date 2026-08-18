#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_DynamicFragments : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Fragment; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 85; }
    auto IsMultiSection() const -> bool override { return true; }
    auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> override;
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildFragmentWidget(
        const FCk_Handle& Entity,
        const struct FInstancedStruct& InFragment) -> TSharedRef<SWidget>;
};
