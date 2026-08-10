#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_MontagePlayer : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("FilmReel"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D97FB8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 140; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
};
