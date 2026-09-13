#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_InteractionResolverAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_InteractionResolverAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_InteractionResolverAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Mappings() const -> TSharedPtr<FCkUiCollection> { return _Mappings; }
    auto Get_AvailableTargets() const -> TSharedPtr<FCkUiCollection> { return _AvailableTargets; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_ActiveIntentsText() const -> FString;
    auto Get_HasAvailableTargets() const -> bool;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Collections() -> bool;
    auto Request_StartIntent(const FString& InStableKey) -> void;
    auto Request_StopIntent(const FString& InStableKey) -> void;
    auto Request_ClearChannel(const FString& InStableKey) -> void;
    auto Navigate_BestTarget(const FString& InStableKey) -> void;
    auto Navigate_AvailableTarget(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Mappings;
    TSharedPtr<FCkUiCollection> _AvailableTargets;
    TMap<FString, FCk_Handle> _BestTargetsByKey;
    TMap<FString, FCk_Handle> _AvailableTargetsByKey;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_InteractionResolver : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_InteractionResolver() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Interaction; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("InteractionResolver"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D95926"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 79; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_InteractionResolverAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
