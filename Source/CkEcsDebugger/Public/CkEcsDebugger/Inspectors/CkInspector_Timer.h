#pragma once

#include "CkCore/Enums/CkEnums.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkInspectorEditScope;
class FCkUiView;
class SBox;
enum class ECk_Timer_Manipulate : uint8;

class SCkInspector_TimerAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_TimerAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_TimerAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_NameText() const -> FString;
    auto Get_DirectionText() const -> FString;
    auto Get_BehaviorText() const -> FString;
    auto Get_StateText() const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Get_GoalText() const -> FString;
    auto Get_ElapsedText() const -> FString;
    auto Get_ElapsedFraction() const -> float;
    auto Get_RemainingText() const -> FString;
    auto Get_DoneText() const -> FString;
    auto Get_DoneColor() const -> FLinearColor;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Request_Manipulate(ECk_Timer_Manipulate InManipulate) -> void;
    auto Request_Reverse() -> void;
    auto Commit_Direction(int32 InIndex) -> void;
    auto Commit_JumpMode(int32 InIndex) -> void;
    auto Commit_Jump(float InSeconds) -> void;
    auto Commit_Consume(float InSeconds) -> void;

private:
    auto Build_AuthoredView() -> bool;
    auto Build_DirectionValue() -> TSharedRef<SWidget>;
    auto Build_JumpModeValue() -> TSharedRef<SWidget>;
    auto Build_NumberValue(FName InTag, bool bJump) -> TSharedRef<SWidget>;
    auto Present_EditControl(TSharedRef<SWidget> InInput, TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TArray<TSharedPtr<FString>> _DirectionOptions;
    TArray<TSharedPtr<FString>> _JumpModeOptions;
    TSet<FString> _DiffLabels;
    FString _LoadError;
    float _JumpSeconds = 1.0f;
    float _ConsumeSeconds = 1.0f;
    ECk_RelativeAbsolute _JumpMode = ECk_RelativeAbsolute::Relative;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Timer : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Timer() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Timer; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Timer"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C98500"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 50; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    // Pending arguments for the Jump / Consume verbs — the timer itself stores none of these, so the
    // rows display what was last typed rather than a live value.
    //
    // Shared boxes rather than plain members: the row lambdas capture them BY VALUE, so a widget that
    // briefly outlives this inspector during panel teardown still reads a live object (the idiom
    // FCkInspector_Probes established for its debug-draw box).
    TSharedRef<float>                _JumpSeconds    = MakeShared<float>(1.0f);
    TSharedRef<ECk_RelativeAbsolute> _JumpMode       = MakeShared<ECk_RelativeAbsolute>(ECk_RelativeAbsolute::Relative);
    TSharedRef<float>                _ConsumeSeconds = MakeShared<float>(1.0f);
    TArray<TWeakPtr<SCkInspector_TimerAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
