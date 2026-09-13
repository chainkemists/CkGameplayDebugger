#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_UIAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_UIAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_UIAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasControls() const -> bool;
    auto Get_CanEdit() const -> bool;
    auto Get_EditDisabledReason() const -> FString;
    auto Get_Text(const FString& InKey) const -> FString;
    auto Get_Number(const FString& InKey) const -> float;
    auto Get_Bool(const FString& InKey) const -> bool;
    auto Get_Color(const FString& InKey) const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;
    auto Change_Enabled(bool bInEnabled) -> void;
    auto Change_Policy(const FString& InKey, const FString& InValue) -> void;
    auto Commit_Number(const FString& InKey, float InValue) -> void;
    auto Change_TraceChannel(const FString& InValue) -> void;

    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _ScalingPolicies;
    TSharedPtr<FCkUiCollection> _FadingPolicies;
    TSharedPtr<FCkUiCollection> _OcclusionPolicies;
    TSharedPtr<FCkUiCollection> _TraceChannels;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_UI : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_UI() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::UIWindow; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 150; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;
    TArray<TWeakPtr<SCkInspector_UIAuthored>> _Instances;
    FString _LastAuthoredLoadError;
};
