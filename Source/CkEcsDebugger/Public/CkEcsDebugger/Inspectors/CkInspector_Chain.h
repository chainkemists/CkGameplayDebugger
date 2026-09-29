#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkInspector_Chain : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Connection; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor{0.05f, 0.75f, 0.7f}; }
    auto CanInspect(const FCk_Handle& InEntity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& InEntity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) -> TSharedRef<SWidget>;
    TSharedRef<bool> _Draw = MakeShared<bool>(false);
    TArray<uint32> _LinkIds;
    TOptional<uint32> _EntityId;
};

// --------------------------------------------------------------------------------------------------------------------

class FCkInspector_ChainLink : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Connection; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor{0.05f, 0.75f, 0.7f}; }
    auto CanInspect(const FCk_Handle& InEntity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& InEntity, float InDeltaTime) -> void override {}

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) -> TSharedRef<SWidget>;
};

// --------------------------------------------------------------------------------------------------------------------
