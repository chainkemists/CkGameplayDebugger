#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkInspectorEditGuard;
class FCkInspectorEditScope;
class FCkUiCollection;
class FCkUiView;
class SBox;

class SCkInspector_EntityTagQueryAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_EntityTagQueryAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_EntityTagQueryAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_RowsCollection() const -> TSharedPtr<FCkUiCollection> { return _RowsCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_IsSatisfiedText() const -> FString;
    auto Get_RequirementsLabel() const -> FString;
    auto Get_RequirementsText() const -> FString;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_PendingTagText() const -> FString;
    auto Get_PendingKindText() const -> FString;
    auto Get_PendingCount() const -> int32 { return _PendingCount; }

    auto Commit_Tag(FName InValue) -> void;
    auto Commit_Kind(int32 InValue) -> void;
    auto Commit_Count(int32 InValue) -> void;
    auto Request_Add() -> void;

private:
    auto Build_RowRecords() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Populate_NativePorts() -> bool;
    auto Build_TagValue() -> TSharedRef<SWidget>;
    auto Build_KindValue() -> TSharedRef<SWidget>;
    auto Build_CountValue() -> TSharedRef<SWidget>;
    auto Present_EditControl(TSharedRef<SWidget> InInput, TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>;
    auto Detach_NativePorts() -> void;
    auto Request_Remove(const FString& InStableKey) -> void;
    auto Navigate_Match(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TSharedPtr<FCkInspectorEditScope> _TagEditScope;
    TSharedPtr<FCkInspectorEditScope> _CountEditScope;
    TSharedPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSharedPtr<FCkUiCollection> _RowsCollection;
    TSharedPtr<FCkUiView> _View;
    TMap<FString, FName> _RequirementsByKey;
    TMap<FString, FCk_Handle> _MatchesByKey;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FString>> _KindOptions;
    FName _PendingTag = NAME_None;
    int32 _PendingKind = 0;
    int32 _PendingCount = 1;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_EntityTagQuery : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_EntityTagQuery() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Label; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0B8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 27; }
    auto IsFilterable() const -> bool override { return false; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_EntityTagQueryAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
