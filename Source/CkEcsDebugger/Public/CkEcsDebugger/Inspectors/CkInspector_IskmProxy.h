#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_IskmProxyAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_IskmProxyAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_IskmProxyAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_PoseSourceText() const -> FString;
    auto Get_PoseSourceForeground() const -> FLinearColor;
    auto Get_PoseSourceBackground() const -> FLinearColor;
    auto Get_PlayingAnimationText() const -> FString;
    auto Get_PlayTimeText() const -> FString;
    auto Get_PlayTimeFraction() const -> float;
    auto Get_AnimInstanceText() const -> FString;
    auto Get_ActiveMontageText() const -> FString;
    auto Get_RagdollingText() const -> FString;
    auto Get_RagdollingForeground() const -> FLinearColor;
    auto Get_RagdollingBackground() const -> FLinearColor;
    auto Get_SubmeshesText() const -> FString;
    auto Get_CustomDataSlotZeroText() const -> FString;
    auto Get_VisibilityIntent() const -> bool { return _VisibilityIntent; }
    auto Get_PlayRateIntent() const -> float { return _PlayRateIntent; }
    auto Get_MorphNameText() const -> FString;
    auto Get_MorphWeight() const -> float;
    auto Get_CustomDataSlot() const -> float { return static_cast<float>(_CustomDataSlot); }
    auto Get_CustomDataValue() const -> float;
    auto Get_CanEndRagdoll() const -> bool;
    auto Set_VisibilityIntent(bool InVisible) -> void;
    auto Commit_PlayRate(float InRate) -> void;
    auto Commit_MorphName(const FString& InName) -> void;
    auto Commit_MorphWeight(float InValue) -> void;
    auto Commit_CustomDataSlot(float InSlot) -> void;
    auto Commit_CustomDataValue(float InValue) -> void;
    auto Request_StopAnimation() -> void;
    auto Request_EndRagdoll() -> void;
    auto Request_ClearMorphs() -> void;
    auto Request_ClearMaterials() -> void;
    auto Request_DetachSubmeshes() -> void;
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
    bool _VisibilityIntent = true;
    float _PlayRateIntent = 1.0f;
    FName _MorphName = NAME_None;
    int32 _CustomDataSlot = 0;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_IskmProxy : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_IskmProxy() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::IsmRenderer; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A648"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 56; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto BuildIskmProxyGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_IskmProxyAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
