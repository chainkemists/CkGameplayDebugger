#pragma once

#include "CkAnimation/AnimPlan/CkAnimPlan_Fragment.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "GameplayTagContainer.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkInspectorEditScope;
class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_AnimPlansAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_AnimPlansAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_AnimPlansAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Plans() const -> TSharedPtr<FCkUiCollection> { return _Plans; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_EditEnabled() const -> bool;
    auto Get_DisabledReason() const -> FString;
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Records() -> bool;
    auto ResolvePlan(const FString& InStableKey, FCk_Handle_AnimPlan& OutPlan) const -> bool;
    auto Handle_TextChanged(const FString& InStableKey, const FText& InText) -> void;
    auto Commit_Cluster(const FString& InStableKey, const FText& InText) -> void;
    auto Stage_Cluster(const FString& InStableKey, const FText& InText) -> void;
    auto Stage_State(const FString& InStableKey, const FText& InText) -> void;
    auto Navigate(const FString& InStableKey) -> void;
    auto Apply_State(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    FString _Filter;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSharedPtr<FCkInspectorEditScope> _EditScope;
    TSharedPtr<FCkUiCollection> _Plans;
    TMap<FString, FCk_Handle_AnimPlan> _PlansByKey;
    TMap<FString, FGameplayTag> _PendingClusters;
    TMap<FString, FGameplayTag> _PendingStates;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_AnimPlans : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_AnimPlans() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Catalog; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A87FE8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 35; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto IsFilterable() const -> bool override { return true; }
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_AnimPlansAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
