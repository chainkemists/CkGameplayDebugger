#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_AggroAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_AggroAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_AggroAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsOwnerAvailable() const -> bool;
    auto Get_IsTargetAvailable() const -> bool;
    auto Get_CanRequestOwner() const -> bool;
    auto Get_CanRequestTarget() const -> bool;
    auto Get_OwnerDisabledReason() const -> FString;
    auto Get_TargetDisabledReason() const -> FString;
    auto Get_Text(const FString& InKey) const -> FString;
    auto Get_Number(const FString& InKey) const -> float;
    auto Get_Bool(const FString& InKey) const -> bool;
    auto Get_DiffColor(const FString& InLabel) const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;
    auto Get_OwnerActiveTrackedEntity() const -> FCk_Handle;
    auto Get_TargetTrackedEntity() const -> FCk_Handle;
    auto Navigate_OwnerActive() -> void;
    auto Navigate_TargetTracked() -> void;
    auto Set_OwnerEnabled(bool bInEnabled) -> void;
    auto Request_ClearAllTargets() -> void;
    auto Request_ClearActiveTarget() -> void;
    auto Commit_SetThreat(float InThreat) -> void;
    auto Commit_AddThreat(float InDelta) -> void;
    auto Request_MarkUnperceived() -> void;
    auto Request_ResetPerception() -> void;
    auto Request_Forget() -> void;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_Aggro : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Aggro() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Aggro; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E08A3C"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 100; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_AggroAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
