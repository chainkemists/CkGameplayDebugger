#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;

class SCkInspector_ResolverAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ResolverAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, FinalValueDiffMarked)
        SLATE_ARGUMENT(bool, PhaseDiffMarked)
        SLATE_ARGUMENT(bool, MetadataTagsDiffMarked)
        SLATE_ARGUMENT(bool, ModifierOpsDiffMarked)
        SLATE_ARGUMENT(bool, MetadataOpsDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ResolverAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_FinalValueText() const -> FString;
    auto Get_PhaseText() const -> FString;
    auto Get_MetadataTagsText() const -> FString;
    auto Get_ModifierOpsText() const -> FString;
    auto Get_MetadataOpsText() const -> FString;
    auto Is_FinalValueDiffMarked() const -> bool { return _FinalValueDiffMarked; }
    auto Is_PhaseDiffMarked() const -> bool { return _PhaseDiffMarked; }
    auto Is_MetadataTagsDiffMarked() const -> bool { return _MetadataTagsDiffMarked; }
    auto Is_ModifierOpsDiffMarked() const -> bool { return _ModifierOpsDiffMarked; }
    auto Is_MetadataOpsDiffMarked() const -> bool { return _MetadataOpsDiffMarked; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _FinalValueDiffMarked = false;
    bool _PhaseDiffMarked = false;
    bool _MetadataTagsDiffMarked = false;
    bool _ModifierOpsDiffMarked = false;
    bool _MetadataOpsDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Resolver : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Resolver() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Resolver; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 105; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_ResolverAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
