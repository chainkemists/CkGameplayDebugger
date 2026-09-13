#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_VfxAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_VfxAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_VfxAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_ComponentText() const -> FString;
    auto Get_ComponentColor() const -> FLinearColor;
    auto Get_StartTimeText() const -> FString;
    auto Get_HasFiniteDuration() const -> bool;
    auto Get_ElapsedFraction() const -> float;
    auto Get_ElapsedDurationText() const -> FString;
    auto Get_DurationText() const -> FString;
    auto Get_StateText() const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Request_Play() -> void;
    auto Request_Stop() -> void;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_Vfx : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Vfx() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Vfx; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C77DD9"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 110; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_VfxAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
