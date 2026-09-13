#pragma once

#include "CkTween/CkTween_Fragment_Data.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkInspectorEditScope;
class FCkUiView;
class SBox;

class SCkInspector_TweenAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_TweenAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_TweenAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_StateText() const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Get_TimeText() const -> FString;
    auto Get_TimeFraction() const -> float;
    auto Get_LoopText() const -> FString;
    auto Get_ReversedText() const -> FString;
    auto Get_ReversedColor() const -> FLinearColor;
    auto Get_MultiplierText() const -> FString;
    auto Get_NextTweenText() const -> FString;
    auto Request_Pause() -> void;
    auto Request_Resume() -> void;
    auto Request_Restart() -> void;
    auto Request_Stop() -> void;
    auto Commit_StopBehavior(int32 InIndex) -> void;
    auto Commit_TimeMultiplier(float InMultiplier) -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Build_AuthoredView() -> bool;
    auto Build_StopBehaviorPort() -> TSharedRef<SWidget>;
    auto Build_TimeMultiplierPort() -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TArray<TSharedPtr<FString>> _StopBehaviorOptions;
    TSet<FString> _DiffLabels;
    FString _LoadError;
    ECk_TweenStopBehavior _StopBehavior = ECk_TweenStopBehavior::DoNothing;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Tween : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Tween() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Tween; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 120; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Get_StructureMask(const FCk_Handle& Entity) const -> uint8;

    uint8 _StructureMask = 0;
    TArray<TWeakPtr<SCkInspector_TweenAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
