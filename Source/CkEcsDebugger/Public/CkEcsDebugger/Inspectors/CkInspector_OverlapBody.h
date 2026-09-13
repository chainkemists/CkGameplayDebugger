#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_OverlapBodyAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_OverlapBodyAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, EnabledDiffMarked)
        SLATE_ARGUMENT(bool, StateDiffMarked)
        SLATE_ARGUMENT(bool, ShapeDiffMarked)
        SLATE_ARGUMENT(bool, MarkerOverlapsDiffMarked)
        SLATE_ARGUMENT(bool, NonMarkerOverlapsDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_OverlapBodyAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_HasMarker() const -> bool;
    auto Get_HasSensor() const -> bool;
    auto Get_MarkerEnabled() const -> bool;
    auto Get_SensorEnabled() const -> bool;
    auto Get_MarkerCanToggle() const -> bool;
    auto Get_SensorCanToggle() const -> bool;
    auto Get_MarkerDisabledReason() const -> FString;
    auto Get_SensorDisabledReason() const -> FString;
    auto Get_MarkerStateText() const -> FString;
    auto Get_SensorStateText() const -> FString;
    auto Get_MarkerShapeText() const -> FString;
    auto Get_SensorShapeText() const -> FString;
    auto Get_MarkerStateForeground() const -> FLinearColor;
    auto Get_MarkerStateBackground() const -> FLinearColor;
    auto Get_SensorStateForeground() const -> FLinearColor;
    auto Get_SensorStateBackground() const -> FLinearColor;
    auto Get_MarkerShapeForeground() const -> FLinearColor;
    auto Get_MarkerShapeBackground() const -> FLinearColor;
    auto Get_SensorShapeForeground() const -> FLinearColor;
    auto Get_SensorShapeBackground() const -> FLinearColor;
    auto Get_MarkerOverlapCountText() const -> FString;
    auto Get_NonMarkerOverlapCountText() const -> FString;
    auto Set_MarkerEnabled(bool InIsEnabled) -> void;
    auto Set_SensorEnabled(bool InIsEnabled) -> void;
    auto Is_EnabledDiffMarked() const -> bool { return _EnabledDiffMarked; }
    auto Is_StateDiffMarked() const -> bool { return _StateDiffMarked; }
    auto Is_ShapeDiffMarked() const -> bool { return _ShapeDiffMarked; }
    auto Is_MarkerOverlapsDiffMarked() const -> bool { return _MarkerOverlapsDiffMarked; }
    auto Is_NonMarkerOverlapsDiffMarked() const -> bool { return _NonMarkerOverlapsDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _EnabledDiffMarked = false;
    bool _StateDiffMarked = false;
    bool _ShapeDiffMarked = false;
    bool _MarkerOverlapsDiffMarked = false;
    bool _NonMarkerOverlapsDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_OverlapBody : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_OverlapBody() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::OverlapBody; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 125; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_OverlapBodyAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
