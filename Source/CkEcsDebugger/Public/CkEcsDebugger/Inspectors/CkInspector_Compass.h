#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class FCkInspectorEditScope;

class SCkInspector_CompassAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_CompassAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(bool, HeadingDiffMarked)
        SLATE_ARGUMENT(bool, ManualHeadingDiffMarked)
        SLATE_ARGUMENT(bool, SourceDiffMarked)
        SLATE_ARGUMENT(bool, ObserverDiffMarked)
        SLATE_ARGUMENT(bool, ArcDiffMarked)
        SLATE_ARGUMENT(bool, EntriesDiffMarked)
        SLATE_ARGUMENT(bool, FilterDiffMarked)
        SLATE_ARGUMENT(bool, IntervalDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_CompassAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HeadingText() const -> FString;
    auto Get_ManualHeading() const -> float;
    auto Get_ManualHeadingText() const -> FString;
    auto Get_SourceText() const -> FString;
    auto Get_Observer() const -> FCk_Handle;
    auto Get_ArcText() const -> FString;
    auto Get_EntriesText() const -> FString;
    auto Get_EntriesFraction() const -> float;
    auto Get_FilterText() const -> FString;
    auto Get_IntervalText() const -> FString;
    auto Get_CanEditManualHeading() const -> bool;
    auto Get_ManualHeadingDisabledReason() const -> FString;
    auto Commit_ManualHeading(float InHeadingDegrees) -> void;
    auto Is_HeadingDiffMarked() const -> bool { return _HeadingDiffMarked; }
    auto Is_ManualHeadingDiffMarked() const -> bool { return _ManualHeadingDiffMarked; }
    auto Is_SourceDiffMarked() const -> bool { return _SourceDiffMarked; }
    auto Is_ObserverDiffMarked() const -> bool { return _ObserverDiffMarked; }
    auto Is_ArcDiffMarked() const -> bool { return _ArcDiffMarked; }
    auto Is_EntriesDiffMarked() const -> bool { return _EntriesDiffMarked; }
    auto Is_FilterDiffMarked() const -> bool { return _FilterDiffMarked; }
    auto Is_IntervalDiffMarked() const -> bool { return _IntervalDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Build_ManualHeadingValue() -> TSharedRef<SWidget>;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditScope> _ManualHeadingEditScope;
    FString _LoadError;
    bool _HeadingDiffMarked = false;
    bool _ManualHeadingDiffMarked = false;
    bool _SourceDiffMarked = false;
    bool _ObserverDiffMarked = false;
    bool _ArcDiffMarked = false;
    bool _EntriesDiffMarked = false;
    bool _FilterDiffMarked = false;
    bool _IntervalDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Compass : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Compass() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Compass; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 64; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_CompassAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
