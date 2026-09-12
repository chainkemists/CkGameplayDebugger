#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_NetworkAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_NetworkAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, NetModeDiffMarked)
        SLATE_ARGUMENT(bool, NetRoleDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_NetworkAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_NetModeDiffMarked() const -> bool { return _NetModeDiffMarked; }
    auto Is_NetRoleDiffMarked() const -> bool { return _NetRoleDiffMarked; }
    auto Get_NetModeText() const -> FString;
    auto Get_NetRoleText() const -> FString;
    auto Get_NetModeForeground() const -> FLinearColor;
    auto Get_NetModeBackground() const -> FLinearColor;
    auto Get_NetRoleForeground() const -> FLinearColor;
    auto Get_NetRoleBackground() const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _NetModeDiffMarked = false;
    bool _NetRoleDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Network : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Network() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Network; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3987E5"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 20; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_NetworkAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
