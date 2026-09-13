#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_IsmProxyAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_IsmProxyAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_IsmProxyAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_MeshText() const -> FString;
    auto Get_MobilityText() const -> FString;
    auto Get_LocationXText() const -> FString;
    auto Get_LocationYText() const -> FString;
    auto Get_LocationZText() const -> FString;
    auto Get_RotationRollText() const -> FString;
    auto Get_RotationPitchText() const -> FString;
    auto Get_RotationYawText() const -> FString;
    auto Get_ScaleXText() const -> FString;
    auto Get_ScaleYText() const -> FString;
    auto Get_ScaleZText() const -> FString;
    auto Get_IsEnabled() const -> bool;
    auto Get_CanEdit() const -> bool;
    auto Get_CanEditCustomValue() const -> bool;
    auto Get_EditDisabledReason() const -> FString;
    auto Get_CustomDataValueDisabledReason() const -> FString;
    auto Get_CustomDataIndex() const -> float;
    auto Get_CustomDataValue() const -> float;
    auto Set_Enabled(bool InIsEnabled) -> void;
    auto Commit_CustomDataIndex(float InValue) -> void;
    auto Commit_CustomDataValue(float InValue) -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    int32 _CustomDataIndex = 0;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_IsmProxy : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_IsmProxy() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::IsmRenderer; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A648"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 55; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_IsmProxyAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
