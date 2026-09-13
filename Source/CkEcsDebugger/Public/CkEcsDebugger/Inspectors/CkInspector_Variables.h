#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class FCkDebuggerModel_EntitySelection;

class CKECSDEBUGGER_API SCkInspector_VariablesAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_VariablesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_VariablesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_RecordsCollection() const -> TSharedPtr<FCkUiCollection> { return _Records; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanEdit() const -> bool;
    auto Get_EditDisabledReason() const -> FString;

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Records() -> bool;
    auto Resolve_Route(const FString& InKey, FString& OutType, FName& OutName) const -> bool;
    auto Change_Bool(const FString& InKey, bool bInValue) -> void;
    auto Commit_Integer(const FString& InKey, int32 InValue) -> void;
    auto Commit_Number(const FString& InKey, float InValue) -> void;
    auto Commit_Text(const FString& InKey, const FText& InValue) -> void;
    auto Commit_Axis(const FString& InKey, float InValue, int32 InAxis) -> void;
    auto Navigate(const FString& InKey) -> void;

    FCk_Handle _Entity;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Records;
    TMap<FString, FString> _RouteTypes;
    TMap<FString, FName> _RouteNames;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_Variables : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Variables() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Variables; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7D8FA8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 80; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto BuildVariablesGrid(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    TArray<TWeakPtr<SCkInspector_VariablesAuthored>> _Instances;
    FString _LastAuthoredLoadError;
};
