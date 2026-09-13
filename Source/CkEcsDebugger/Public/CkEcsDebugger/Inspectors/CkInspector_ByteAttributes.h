#pragma once

#include "CkAttribute/ByteAttribute/CkByteAttribute_Fragment_Data.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_ByteAttributesAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ByteAttributesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ByteAttributesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_RecordsCollection() const -> TSharedPtr<FCkUiCollection> { return _Records; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Records() -> bool;
    auto Resolve_Attribute(const FString& InKey, FCk_Handle_ByteAttribute& OutAttribute) const -> bool;
    auto Resolve_Modifier(const FString& InKey, FCk_Handle_ByteAttributeModifier& OutModifier) const -> bool;
    auto Commit_Override(const FString& InKey, int32 InValue, ECk_MinMaxCurrent InComponent) -> void;
    auto Commit_Modifier(const FString& InKey, int32 InValue) -> void;
    auto Remove_Modifier(const FString& InKey) -> void;
    auto Clear_Component(const FString& InKey) -> void;
    auto Navigate(const FString& InKey) -> void;

    FCk_Handle _Entity;
    FString _Filter;
    TSet<FString> _DiffLabels;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TSharedPtr<FCkUiCollection> _Records;
    TMap<FString, FCk_Handle_ByteAttribute> _Attributes;
    TMap<FString, FCk_Handle_ByteAttributeModifier> _Modifiers;
    TMap<FString, FString> _RouteAttributeKeys;
    TMap<FString, ECk_MinMaxCurrent> _RouteComponents;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_ByteAttributes : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_ByteAttributes() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::AttributeByte; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9884F"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 60; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto IsFilterable() const -> bool override { return true; }
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) -> TSharedRef<SWidget>;
    TArray<TWeakPtr<SCkInspector_ByteAttributesAuthored>> _Instances;
    FString _LastAuthoredLoadError;
};
