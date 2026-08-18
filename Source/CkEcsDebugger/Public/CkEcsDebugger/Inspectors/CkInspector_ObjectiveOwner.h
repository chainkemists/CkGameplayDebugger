#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_ObjectiveOwner : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Objective; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FB0A5"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 56; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildOwnerGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;

    int32 _CachedObjectiveCount = -1;
};
