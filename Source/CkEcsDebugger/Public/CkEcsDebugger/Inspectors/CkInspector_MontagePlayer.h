#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkInspectorEditScope;
class FCkUiView;
class SBox;

class CKECSDEBUGGER_API SCkInspector_MontagePlayerAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_MontagePlayerAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_MontagePlayerAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasControls() const -> bool;
    auto Get_StateText() const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Get_ActiveMontageText() const -> FString;
    auto Get_ActiveMontageColor() const -> FLinearColor;
    auto Get_PositionFraction() const -> float;
    auto Get_PositionText() const -> FString;
    auto Get_AnimInstanceText() const -> FString;
    auto Get_AnimInstanceColor() const -> FLinearColor;
    auto Get_PlayRateText() const -> FString;
    auto Get_CatchUpRemainingText() const -> FString;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_PendingBlendOut() const -> float { return _PendingBlendOut; }
    auto Commit_PendingBlendOut(float InSeconds) -> void;
    auto Get_PendingSectionText() const -> FString;
    auto Commit_PendingSection(FName InSection) -> void;
    auto Request_Pause() -> void;
    auto Request_Resume() -> void;
    auto Request_Stop() -> void;
    auto Request_Jump() -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Build_AuthoredView() -> bool;
    auto Build_BlendOutPort() -> TSharedRef<SWidget>;
    auto Build_SectionPort() -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TSet<FString> _DiffLabels;
    FString _LoadError;
    float _PendingBlendOut = 0.25f;
    FName _PendingSection;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_MontagePlayer : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_MontagePlayer() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Cinematic; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D97FB8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 140; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    TArray<TWeakPtr<SCkInspector_MontagePlayerAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
