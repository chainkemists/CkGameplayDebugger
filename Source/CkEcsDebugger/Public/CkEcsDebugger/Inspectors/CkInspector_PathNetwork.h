#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_PathNetworkAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_PathNetworkAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, BuiltDiffMarked)
        SLATE_ARGUMENT(bool, NodesDiffMarked)
        SLATE_ARGUMENT(bool, EdgesDiffMarked)
        SLATE_ARGUMENT(bool, BuildEpochDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_PathNetworkAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_BuiltDiffMarked() const -> bool { return _BuiltDiffMarked; }
    auto Is_NodesDiffMarked() const -> bool { return _NodesDiffMarked; }
    auto Is_EdgesDiffMarked() const -> bool { return _EdgesDiffMarked; }
    auto Is_BuildEpochDiffMarked() const -> bool { return _BuildEpochDiffMarked; }
    auto Get_BuiltText() const -> FString;
    auto Get_NodesText() const -> FString;
    auto Get_EdgesText() const -> FString;
    auto Get_BuildEpochText() const -> FString;
    auto Get_BuiltForeground() const -> FLinearColor;
    auto Get_BuiltBackground() const -> FLinearColor;
    auto Get_NodesForeground() const -> FLinearColor;
    auto Get_NodesBackground() const -> FLinearColor;
    auto Get_EdgesForeground() const -> FLinearColor;
    auto Get_EdgesBackground() const -> FLinearColor;
    auto Get_BuildEpochForeground() const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _BuiltDiffMarked = false;
    bool _NodesDiffMarked = false;
    bool _EdgesDiffMarked = false;
    bool _BuildEpochDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_PathNetwork : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_PathNetwork() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::PathNetwork; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("PathNetwork"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA46A"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 53; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_PathNetworkAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
