#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;

class SCkInspector_PoiAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_PoiAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, CategoryTagsDiffMarked)
        SLATE_ARGUMENT(bool, LabelDiffMarked)
        SLATE_ARGUMENT(bool, StateDiffMarked)
        SLATE_ARGUMENT(bool, DisabledDiffMarked)
        SLATE_ARGUMENT(bool, WorldPositionDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_PoiAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_CategoryTagsCollection() const -> TSharedPtr<FCkUiCollection> { return _CategoryTagsCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_LabelText() const -> FString;
    auto Get_StateText() const -> FString;
    auto Get_IsDisabled() const -> bool;
    auto Get_CanToggleDisabled() const -> bool;
    auto Get_WorldPositionAxisText(int32 InAxis) const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Is_CategoryTagsDiffMarked() const -> bool { return _CategoryTagsDiffMarked; }
    auto Is_LabelDiffMarked() const -> bool { return _LabelDiffMarked; }
    auto Is_StateDiffMarked() const -> bool { return _StateDiffMarked; }
    auto Is_DisabledDiffMarked() const -> bool { return _DisabledDiffMarked; }
    auto Is_WorldPositionDiffMarked() const -> bool { return _WorldPositionDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_CategoryTagRecords() -> bool;
    auto Refresh_CategoryTagRecords() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Set_Disabled(bool InIsDisabled) -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiCollection> _CategoryTagsCollection;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _CategoryTagsDiffMarked = false;
    bool _LabelDiffMarked = false;
    bool _StateDiffMarked = false;
    bool _DisabledDiffMarked = false;
    bool _WorldPositionDiffMarked = false;
    TOptional<bool> _PendingDisabled;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Poi : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Poi() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Pin; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 63; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_PoiAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
