#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkInspector_RotateTowards : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Compass; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor{0.85f, 0.55f, 0.1f}; }
    auto CanInspect(const FCk_Handle& InEntity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 56; }
    auto Tick(const FCk_Handle& InEntity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) -> TSharedRef<SWidget>;
    TSharedRef<bool> _Draw = MakeShared<bool>(false);
    TOptional<uint32> _EntityId;
    bool _HadRangeClamp = false;
};

// --------------------------------------------------------------------------------------------------------------------
