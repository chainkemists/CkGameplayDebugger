#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_PathNetwork : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::PathNetwork; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("PathNetwork"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA46A"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 53; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override {}
};
