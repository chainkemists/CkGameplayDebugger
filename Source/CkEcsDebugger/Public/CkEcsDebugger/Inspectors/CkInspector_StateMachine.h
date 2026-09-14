#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class SBox;
enum class ECk_Tone : uint8;

class CKECSDEBUGGER_API SCkInspector_StateMachineAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_StateMachineAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkInspector_StateMachineAuthored() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_ForEntity(const FCk_Handle& InEntity) const -> bool { return _Entity == InEntity; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_Text(const FString& InKey) const -> FString;
    auto Get_Bool(const FString& InKey) const -> bool;
    auto Get_Tone(const FString& InKey) const -> ECk_Tone;
    auto Get_DiffColor(const FString& InLabel) const -> FLinearColor;

private:
    enum class ERequest : uint8 { Start, Stop, Pause, Resume };
    auto Build_AuthoredView() -> bool;
    auto Capture_History() -> bool;
    auto Get_SubStateMachine() const -> FCk_Handle;
    auto Navigate_SubStateMachine() -> void;
    auto Request(const FString& InRouteKey, ERequest InRequest) -> void;

private:
    FCk_Handle _Entity;
    FString _RouteKey;
    TSet<FString> _DiffLabels;
    TMap<FString, FString> _HistoryText;
    TSharedPtr<FCkUiCollection> _History;
    TSharedPtr<SBox> _TimelinePort;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _HistoryCaptured = false;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_StateMachine : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_StateMachine() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::StateMachine; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9085E9"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 90; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_StateMachineAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
