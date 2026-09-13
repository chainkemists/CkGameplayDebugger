#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class FCkDebuggerModel_EntitySelection;

class SCkInspector_EntityCollectionsAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_EntityCollectionsAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TWeakPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_EntityCollectionsAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_CollectionsCollection() const -> TSharedPtr<FCkUiCollection> { return _CollectionsCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CollectionCount() const -> int32;
    auto Is_CollectionDiffMarked(const FString& InStableKey) const -> bool { return _CollectionDiffMarkedKeys.Contains(InStableKey); }
    auto Is_MemberDiffMarked(const FString& InStableKey) const -> bool { return _MemberDiffMarkedKeys.Contains(InStableKey); }

private:
    auto Build_CollectionRecords() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Request_SelectCollection(const FString& InStableKey) -> void;
    auto Request_RemoveMember(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    FString _Filter;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSharedPtr<FCkUiCollection> _CollectionsCollection;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    TSet<FString> _CollectionDiffMarkedKeys;
    TSet<FString> _MemberDiffMarkedKeys;
    bool _DiffMarksCaptured = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_EntityCollections : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_EntityCollections() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::EntityCollection; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FB25F"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 70; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto IsFilterable() const -> bool override { return true; }
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_EntityCollectionsAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
