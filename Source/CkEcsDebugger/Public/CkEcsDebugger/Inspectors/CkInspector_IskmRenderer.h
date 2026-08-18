#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_IskmRenderer : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::IsmRenderer; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A648"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildIskmRendererGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
};
