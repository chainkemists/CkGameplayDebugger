#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_PathNetworkFollowerAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_PathNetworkFollowerAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, RouteStatusDiffMarked)
        SLATE_ARGUMENT(bool, FailReasonDiffMarked)
        SLATE_ARGUMENT(bool, GoalDiffMarked)
        SLATE_ARGUMENT(bool, LegsDiffMarked)
        SLATE_ARGUMENT(bool, WaypointsDiffMarked)
        SLATE_ARGUMENT(bool, TotalCostDiffMarked)
        SLATE_ARGUMENT(bool, FeatureDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_PathNetworkFollowerAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_RouteStatusText() const -> FString;
    auto Get_FailReasonText() const -> FString;
    auto Get_GoalXText() const -> FString;
    auto Get_GoalYText() const -> FString;
    auto Get_GoalZText() const -> FString;
    auto Get_LegsText() const -> FString;
    auto Get_WaypointsText() const -> FString;
    auto Get_TotalCostText() const -> FString;
    auto Get_RouteStatusForeground() const -> FLinearColor;
    auto Get_RouteStatusBackground() const -> FLinearColor;
    auto Get_FailReasonForeground() const -> FLinearColor;
    auto Get_FailReasonBackground() const -> FLinearColor;
    auto Is_RouteStatusDiffMarked() const -> bool { return _RouteStatusDiffMarked; }
    auto Is_FailReasonDiffMarked() const -> bool { return _FailReasonDiffMarked; }
    auto Is_GoalDiffMarked() const -> bool { return _GoalDiffMarked; }
    auto Is_LegsDiffMarked() const -> bool { return _LegsDiffMarked; }
    auto Is_WaypointsDiffMarked() const -> bool { return _WaypointsDiffMarked; }
    auto Is_TotalCostDiffMarked() const -> bool { return _TotalCostDiffMarked; }
    auto Is_FeatureDiffMarked() const -> bool { return _FeatureDiffMarked; }

private:
    auto Build_AuthoredView() -> bool;
    auto Remove() -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _RouteStatusDiffMarked = false;
    bool _FailReasonDiffMarked = false;
    bool _GoalDiffMarked = false;
    bool _LegsDiffMarked = false;
    bool _WaypointsDiffMarked = false;
    bool _TotalCostDiffMarked = false;
    bool _FeatureDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_PathNetworkFollower : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_PathNetworkFollower() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::PathNetworkFollower; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("PathNetworkFollower"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override
    {
        return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("58C0A8")));
    }
    auto CanInspect(const FCk_Handle& InEntity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& InEntity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 52; }
    auto Tick(const FCk_Handle& InEntity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_PathNetworkFollowerAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
