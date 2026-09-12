#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_ObjectiveAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ObjectiveAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, NameDiffMarked)
        SLATE_ARGUMENT(bool, DisplayDiffMarked)
        SLATE_ARGUMENT(bool, DescriptionDiffMarked)
        SLATE_ARGUMENT(bool, StatusDiffMarked)
        SLATE_ARGUMENT(bool, ControlDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ObjectiveAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_NameDiffMarked() const -> bool { return _NameDiffMarked; }
    auto Is_DisplayDiffMarked() const -> bool { return _DisplayDiffMarked; }
    auto Is_DescriptionDiffMarked() const -> bool { return _DescriptionDiffMarked; }
    auto Get_IsAvailable() const -> bool;
    auto Get_NameText() const -> FString;
    auto Get_DisplayText() const -> FString;
    auto Get_DescriptionText() const -> FString;
    auto Get_StatusText() const -> FString;
    auto Get_StatusForeground() const -> FLinearColor;
    auto Get_StatusBackground() const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;
    auto Request_Start() -> void;
    auto Request_Complete() -> void;
    auto Request_Fail() -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _NameDiffMarked = false;
    bool _DisplayDiffMarked = false;
    bool _DescriptionDiffMarked = false;
    bool _StatusDiffMarked = false;
    bool _ControlDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Objective : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Objective() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Objective; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FB0A5"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_ObjectiveAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
