#pragma once

#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkInspectorEditScope;
class FCkUiFloatSeries;
class FCkUiView;
class SBox;

class SCkInspector_PhysicsAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_PhysicsAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_PhysicsAuthored() override;
    virtual auto Tick(const FGeometry&, double, float) -> void override;
    auto Release() -> void;

    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_VelocitySeries() const -> TSharedPtr<FCkUiFloatSeries> { return _VelocitySeries; }
    auto Get_PredictedVelocitySeries() const -> TSharedPtr<FCkUiFloatSeries> { return _PredictedVelocitySeries; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasVelocity() const -> bool;
    auto Get_HasAcceleration() const -> bool;
    auto Get_HasPredictedVelocity() const -> bool;
    auto Get_HasEulerIntegrator() const -> bool;
    auto Get_CanOverrideVelocity() const -> bool;
    auto Get_CanOverrideAcceleration() const -> bool;
    auto Get_CanStartIntegrator() const -> bool;
    auto Get_CanStopIntegrator() const -> bool;
    auto Get_DisabledReason(ECk_DebugRequest_Requirement InRequirement) const -> FString;
    auto Get_StartIntegratorDisabledReason() const -> FString;
    auto Get_StopIntegratorDisabledReason() const -> FString;
    auto Get_Vector(const FString& InKind) const -> FVector;
    auto Get_AxisText(const FString& InKind, int32 InAxis) const -> FString;
    auto Get_SpeedText(const FString& InKind) const -> FString;
    auto Get_PreviousDeltaTimeText() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Commit_Velocity(const FVector& InValue) -> void;
    auto Commit_Acceleration(const FVector& InValue) -> void;
    auto Request_StartIntegrator() -> void;
    auto Request_StopIntegrator() -> void;

private:
    auto Build_AuthoredView() -> bool;
    auto Build_VectorValue(FName InTagPrefix, TFunction<FVector()> InGet, TFunction<void(const FVector&)> InSet,
        TFunction<bool()> InIsEnabled, ECk_DebugRequest_Requirement InRequirement) -> TSharedRef<SWidget>;
    auto Present_EditControl(TSharedRef<SWidget> InInput, TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;
    auto Push_SpeedSamples() -> void;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkUiFloatSeries> _VelocitySeries;
    TSharedPtr<FCkUiFloatSeries> _PredictedVelocitySeries;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TSet<FString> _DiffLabels;
    TArray<float> _VelocitySamples;
    TArray<float> _PredictedVelocitySamples;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Physics : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Physics() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Physics; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 135; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;

    uint8 _StructureMask = 0;
    TArray<TWeakPtr<SCkInspector_PhysicsAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
