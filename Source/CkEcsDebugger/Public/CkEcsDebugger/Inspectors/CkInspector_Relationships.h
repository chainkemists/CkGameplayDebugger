#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class SBox;

class SCkInspector_RelationshipsAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_RelationshipsAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(bool, TeamDiffMarked)
        SLATE_ARGUMENT(bool, ContextOwnerDiffMarked)
        SLATE_ARGUMENT(bool, LifetimeOwnerDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_RelationshipsAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_TeamDiffMarked() const -> bool { return _TeamDiffMarked; }
    auto Is_ContextOwnerDiffMarked() const -> bool { return _ContextOwnerDiffMarked; }
    auto Is_LifetimeOwnerDiffMarked() const -> bool { return _LifetimeOwnerDiffMarked; }
    auto Get_TeamText() const -> FString;
    auto Get_TeamColor() const -> FLinearColor;

private:
    auto Build_AuthoredView() -> bool;
    auto Populate_NativePorts() -> bool;
    auto Detach_NativePorts() -> void;

private:
    FCk_Handle              _Entity;
    TSharedPtr<FCkUiView>   _View;
    TSharedPtr<SBox>        _ContextOwnerPort;
    TSharedPtr<SBox>        _LifetimeOwnerPort;
    FString                 _LoadError;
    bool                    _TeamDiffMarked = false;
    bool                    _ContextOwnerDiffMarked = false;
    bool                    _LifetimeOwnerDiffMarked = false;
    bool                    _Active = true;
    bool                    _Mounted = false;
};

class FCkInspector_Relationships : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Relationships() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Relationships; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 30; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_RelationshipsAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
