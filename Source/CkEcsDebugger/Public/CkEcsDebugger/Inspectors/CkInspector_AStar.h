#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_AStarAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_AStarAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, StatusDiffMarked)
        SLATE_ARGUMENT(bool, OpenSetDiffMarked)
        SLATE_ARGUMENT(bool, ClosedSetDiffMarked)
        SLATE_ARGUMENT(bool, IterationsDiffMarked)
        SLATE_ARGUMENT(bool, TimeDiffMarked)
        SLATE_ARGUMENT(bool, BudgetUsedDiffMarked)
        SLATE_ARGUMENT(bool, BudgetDiffMarked)
        SLATE_ARGUMENT(bool, MaxIterationsDiffMarked)
        SLATE_ARGUMENT(bool, CostThresholdDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_AStarAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_HasSearch() const -> bool;
    auto Get_HasParams() const -> bool;
    auto Get_StatusText() const -> FString;
    auto Get_OpenSetText() const -> FString;
    auto Get_ClosedSetText() const -> FString;
    auto Get_IterationsText() const -> FString;
    auto Get_TimeText() const -> FString;
    auto Get_BudgetUsedText() const -> FString;
    auto Get_BudgetFraction() const -> float;
    auto Get_StatusForeground() const -> FLinearColor;
    auto Get_StatusBackground() const -> FLinearColor;
    auto Get_BudgetFill() const -> FLinearColor;
    auto Get_BudgetText() const -> FString;
    auto Get_MaxIterationsText() const -> FString;
    auto Get_CostThresholdText() const -> FString;
    auto Is_StatusDiffMarked() const -> bool { return _StatusDiffMarked; }
    auto Is_OpenSetDiffMarked() const -> bool { return _OpenSetDiffMarked; }
    auto Is_ClosedSetDiffMarked() const -> bool { return _ClosedSetDiffMarked; }
    auto Is_IterationsDiffMarked() const -> bool { return _IterationsDiffMarked; }
    auto Is_TimeDiffMarked() const -> bool { return _TimeDiffMarked; }
    auto Is_BudgetUsedDiffMarked() const -> bool { return _BudgetUsedDiffMarked; }
    auto Is_BudgetDiffMarked() const -> bool { return _BudgetDiffMarked; }
    auto Is_MaxIterationsDiffMarked() const -> bool { return _MaxIterationsDiffMarked; }
    auto Is_CostThresholdDiffMarked() const -> bool { return _CostThresholdDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _StatusDiffMarked = false;
    bool _OpenSetDiffMarked = false;
    bool _ClosedSetDiffMarked = false;
    bool _IterationsDiffMarked = false;
    bool _TimeDiffMarked = false;
    bool _BudgetUsedDiffMarked = false;
    bool _BudgetDiffMarked = false;
    bool _MaxIterationsDiffMarked = false;
    bool _CostThresholdDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_AStar : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_AStar() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::AStar; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 115; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_AStarAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
