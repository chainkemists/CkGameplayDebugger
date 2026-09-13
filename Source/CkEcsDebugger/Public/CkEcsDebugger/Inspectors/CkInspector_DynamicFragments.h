#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class FCkDebuggerModel_EntitySelection;
class UScriptStruct;

class SCkInspector_DynamicFragmentAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_DynamicFragmentAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TWeakObjectPtr<const UScriptStruct>, FragmentType)
        SLATE_ARGUMENT(TWeakPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_DynamicFragmentAuthored() override;
    virtual auto Tick(const FGeometry& InGeometry, double InTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Properties() const -> TSharedPtr<FCkUiCollection> { return _Properties; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_IsReplicated() const -> bool;
    auto Get_CanRemove() const -> bool;
    auto Get_CanMarkReplicationDirty() const -> bool;
    auto Get_ActionDisabledReason() const -> FString;
    auto Get_MarkReplicationDirtyDisabledReason() const -> FString;

private:
    auto Refresh_PropertyRecords() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Request_Remove() -> void;
    auto Request_MarkReplicationDirty() -> void;
    auto Navigate_HandleProperty(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    TWeakObjectPtr<const UScriptStruct> _FragmentType;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Properties;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_DynamicFragments : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_DynamicFragments() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Fragment; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 85; }
    auto IsMultiSection() const -> bool override { return true; }
    auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> override;
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto BuildFragmentWidget(
        const FCk_Handle& Entity,
        const struct FInstancedStruct& InFragment) -> TSharedRef<SWidget>;

    TArray<TWeakPtr<SCkInspector_DynamicFragmentAuthored>> _AuthoredInstances;
    TArray<FString> _LastFragmentKeys;
    FString _LastAuthoredLoadError;
};
