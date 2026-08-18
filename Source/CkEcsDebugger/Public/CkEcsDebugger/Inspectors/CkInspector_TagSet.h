#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkInspector_TagSet : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Label; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0B8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 25; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    auto BuildTagSetGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;
};
