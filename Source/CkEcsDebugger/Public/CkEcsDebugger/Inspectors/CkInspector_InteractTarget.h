#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment_Data.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;

class SCkInspector_InteractTargetAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_InteractTargetAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Owner)
        SLATE_ARGUMENT(FCk_Handle_InteractTarget, Target)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_InteractTargetAuthored() override;
    virtual auto Tick(const FGeometry& InGeometry, double InTime, float InDeltaTime) -> void override;
    auto Release() -> void;

    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Interactions() const -> TSharedPtr<FCkUiCollection> { return _Interactions; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_ChannelText() const -> FString;
    auto Get_EnabledText() const -> FString;
    auto Get_EnabledTone() const -> FLinearColor;
    auto Get_EnabledBackground() const -> FLinearColor;
    auto Get_IsEnabled() const -> bool;
    auto Get_CompletionText() const -> FString;
    auto Get_DurationText() const -> FString;
    auto Get_DurationColor() const -> FLinearColor;
    auto Get_ConcurrentText() const -> FString;
    auto Get_RequestDisabledReason() const -> FString;
    auto Set_Enabled(bool InEnabled) -> void;
    auto Request_CancelAll() -> void;
    auto Navigate_Interaction(const FString& InStableKey) -> void;

private:
    auto Refresh_Interactions() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

    FCk_Handle _Owner;
    FCk_Handle_InteractTarget _Target;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Interactions;
    TSharedPtr<FCkUiView> _View;
    TMap<FString, FCk_Handle> _InteractionsByKey;
    TArray<FString> _InteractionSnapshot;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_InteractTarget : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_InteractTarget() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Interaction; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D95926"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 61; }
    auto IsMultiSection() const -> bool override { return true; }
    auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> override;
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_TargetNativeBody(const FCk_Handle_InteractTarget& InTarget) const -> TSharedRef<SWidget>;

    TArray<TWeakPtr<SCkInspector_InteractTargetAuthored>> _AuthoredInstances;
    TArray<FString> _LastTargetKeys;
    FString _LastAuthoredLoadError;
};
