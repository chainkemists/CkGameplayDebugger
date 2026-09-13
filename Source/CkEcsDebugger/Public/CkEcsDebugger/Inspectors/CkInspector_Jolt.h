#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiFloatSeries;
class FCkUiView;

class SCkInspector_JoltAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_JoltAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_JoltAuthored() override;
    virtual auto Tick(const FGeometry&, double, float) -> void override;
    auto Release() -> void;

    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_SpeedSeries() const -> TSharedPtr<FCkUiFloatSeries> { return _SpeedSeries; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasBody() const -> bool;
    auto Get_HasCharacter() const -> bool;
    auto Get_HasStaticActor() const -> bool;
    auto Get_BodyIdText() const -> FString;
    auto Get_MotionTypeText() const -> FString;
    auto Get_MotionTypeForeground() const -> FLinearColor;
    auto Get_MotionTypeBackground() const -> FLinearColor;
    auto Get_SleepStateText() const -> FString;
    auto Get_SleepStateForeground() const -> FLinearColor;
    auto Get_SleepStateBackground() const -> FLinearColor;
    auto Get_BodyAddedText() const -> FString;
    auto Get_BodyAddedForeground() const -> FLinearColor;
    auto Get_BodyAddedBackground() const -> FLinearColor;
    auto Get_LinearVelocityAxisText(int32 InAxis) const -> FString;
    auto Get_LinearSpeedText() const -> FString;
    auto Get_GroundStateText() const -> FString;
    auto Get_GroundStateForeground() const -> FLinearColor;
    auto Get_GroundStateBackground() const -> FLinearColor;
    auto Get_GroundNormalAxisText(int32 InAxis) const -> FString;
    auto Get_GroundVelocityAxisText(int32 InAxis) const -> FString;
    auto Get_SourceActorText() const -> FString;
    auto Get_NumBodiesText() const -> FString;
    auto Get_NumBodiesForeground() const -> FLinearColor;
    auto Get_NumBodiesBackground() const -> FLinearColor;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Build_AuthoredView() -> bool;
    auto Push_SpeedSample() -> void;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkUiFloatSeries> _SpeedSeries;
    TSet<FString> _DiffLabels;
    TArray<float> _SpeedSamples;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Jolt : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Jolt() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Jolt; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 137; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    uint8 _StructureMask = 0;
    TArray<TWeakPtr<SCkInspector_JoltAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
