#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class ACk_ActorRelay_UE;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_ActorRelayAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ActorRelayAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ActorRelayAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_ClassText() const -> FString;
    auto Get_ActorText() const -> FString;
    auto Get_GroupTagText() const -> FString;
    auto Get_OwnershipText() const -> FString;
    auto Get_SelectionText() const -> FString;
    auto Get_DisconnectText() const -> FString;
    auto Get_ChannelFraction() const -> float;
    auto Get_ChannelText() const -> FString;
    auto Get_MaxEntitiesText() const -> FString;
    auto Get_HasChannelCapacity() const -> bool;
    auto Get_EntitiesFraction() const -> float;
    auto Get_EntitiesText() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Get_RelayActor() const -> ACk_ActorRelay_UE*;

private:
    FCk_Handle _Entity;
    TWeakObjectPtr<ACk_ActorRelay_UE> _RelayActor;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_ActorRelay : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_ActorRelay() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::ActorBridge; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7FB2E5"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 45; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_ActorRelayAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
