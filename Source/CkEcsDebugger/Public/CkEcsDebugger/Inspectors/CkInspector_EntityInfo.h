#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"

class FCkInspectorEditScope;
class FCkUiView;
class SBox;

class SCkInspector_EntityInfoAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_EntityInfoAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(bool, NameDiffMarked)
        SLATE_ARGUMENT(bool, SetNameDiffMarked)
        SLATE_ARGUMENT(bool, IdDiffMarked)
        SLATE_ARGUMENT(bool, ActorDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_EntityInfoAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_NameDiffMarked() const -> bool { return _NameDiffMarked; }
    auto Is_SetNameDiffMarked() const -> bool { return _SetNameDiffMarked; }
    auto Is_IdDiffMarked() const -> bool { return _IdDiffMarked; }
    auto Is_ActorDiffMarked() const -> bool { return _ActorDiffMarked; }
    auto Get_NameText() const -> FString;
    auto Get_ActorText() const -> FString;

private:
    auto Build_AuthoredView() -> bool;
    auto Populate_NativePorts() -> bool;
    auto Detach_NativePorts() -> void;
    auto Handle_NameChanged(const FText& InText) -> void;
    auto Handle_NameCommitted(const FText& InText, ETextCommit::Type InCommitType) -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<SBox> _RootHost;
    TSharedPtr<SBox> _IdPort;
    TSharedPtr<SEditableTextBox> _NameInput;
    TSharedPtr<FCkInspectorEditScope> _EditScope;
    FString _LoadError;
    bool _NameDiffMarked = false;
    bool _SetNameDiffMarked = false;
    bool _IdDiffMarked = false;
    bool _ActorDiffMarked = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_EntityInfo : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_EntityInfo() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::EntityInfo; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 0; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_EntityInfoAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
