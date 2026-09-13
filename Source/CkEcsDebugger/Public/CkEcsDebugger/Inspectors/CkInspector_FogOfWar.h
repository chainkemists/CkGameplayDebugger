#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class FCkInspectorEditScope;
class SBox;

class SCkInspector_FogOfWarAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_FogOfWarAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()
    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_FogOfWarAuthored() override;
    virtual auto Tick(const FGeometry&, double, float) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_GridText() const -> FString;
    auto Get_ExploredText() const -> FString;
    auto Get_ExploredFraction() const -> float;
    auto Get_BoundsCenterText() const -> FString;
    auto Get_BoundsHalfExtentsText() const -> FString;
    auto Get_RevealersText() const -> FString;
    auto Get_ParamsRadiusText() const -> FString;
    auto Get_IntervalText() const -> FString;
    auto Get_StagedLocation() const -> FVector;
    auto Get_StagedRadius() const -> float;
    auto Commit_Location(const FVector&) -> void;
    auto Commit_Radius(float) -> void;
    auto Request_RevealAll() -> void;
    auto Request_Reset() -> void;
    auto Request_RevealHere() -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
private:
    auto Build_AuthoredView() -> bool;
    auto Build_LocationPort() -> TSharedRef<SWidget>;
    auto Build_RadiusPort() -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget>) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TSet<FString> _DiffLabels;
    FVector _RevealLocation = FVector::ZeroVector;
    float _RevealRadius = 0.0f;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_FogOfWar final : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_FogOfWar() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::FogOfWar; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 66; }
    auto Tick(const FCk_Handle&, float) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }
private:
    auto Build_NativeBody(const FCk_Handle&) const -> TSharedRef<SWidget>;
    auto Get_StructureMask(const FCk_Handle&) const -> uint8;

    // Native fallback scratch remains inspector-owned for compatibility. Authored instances deliberately
    // own independent payloads so retained views cannot overwrite each other's staged reveal request.
    TSharedRef<FVector> _RevealLocation = MakeShared<FVector>(FVector::ZeroVector);
    TSharedRef<float> _RevealRadius = MakeShared<float>(0.0f);
    TArray<TWeakPtr<SCkInspector_FogOfWarAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
    uint8 _StructureMask = 0;
};
