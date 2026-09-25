#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkInspector_ProceduralAnimation final : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Aim; }
    auto CanInspect(const FCk_Handle& InEntity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& InEntity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto IsFilterable() const -> bool override { return true; }
    auto Get_SortPriority() const -> int32 override { return 138; }
    auto Tick(const FCk_Handle&, float) -> void override {}
};

// --------------------------------------------------------------------------------------------------------------------
