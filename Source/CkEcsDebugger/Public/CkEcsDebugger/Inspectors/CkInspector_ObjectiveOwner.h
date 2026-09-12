#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class FCkDebuggerModel_EntitySelection;
enum class ECk_Tone : uint8;

class SCkInspector_ObjectiveOwnerAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ObjectiveOwnerAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TWeakPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(bool, ProgressDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ObjectiveOwnerAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_ObjectivesCollection() const -> TSharedPtr<FCkUiCollection> { return _ObjectivesCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_ProgressDiffMarked() const -> bool { return _ProgressDiffMarked; }
    auto Get_IsAvailable() const -> bool;
    auto Get_ProgressText() const -> FString;
    auto Get_ProgressFraction() const -> float;
    auto Get_ObjectiveCount() const -> int32;
    auto Get_ObjectiveStatusText(const FString& InStableKey) const -> FString;
    auto Get_ObjectiveTone(const FString& InStableKey) const -> ECk_Tone;
    auto Get_ObjectiveStatusForeground(const FString& InStableKey) const -> FLinearColor;
    auto Get_ObjectiveStatusBackground(const FString& InStableKey) const -> FLinearColor;
    auto Is_ObjectiveDiffMarked(const FString& InStableKey) const -> bool;

private:
    auto Build_ObjectiveRecords() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Request_SelectObjective(const FString& InStableKey) -> void;
    auto Request_RemoveObjective(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    FString _Filter;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSharedPtr<FCkUiCollection> _ObjectivesCollection;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _ProgressDiffMarked = false;
    TSet<FString> _ObjectiveDiffMarkedKeys;
    bool _ObjectiveDiffMarksCaptured = false;
    bool _ProgressVisible = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_ObjectiveOwner : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_ObjectiveOwner() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Objective; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FB0A5"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 56; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_ObjectiveOwnerAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
