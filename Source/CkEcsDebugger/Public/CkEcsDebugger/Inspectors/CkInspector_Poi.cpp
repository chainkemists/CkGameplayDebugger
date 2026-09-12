#include "CkInspector_Poi.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkEntityTag/CkEntityTag_Fragment.h"
#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkPoi/CkPoi_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Poi)

namespace ck_inspector_poi
{
    struct FPendingDisabledState final
    {
        TOptional<bool> Desired;
    };

    auto TryGetPoi(const FCk_Handle& InEntity, FCk_Handle_Poi& OutPoi) -> bool
    {
        OutPoi = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Poi_UE::Has(InEntity)
            || NOT UCk_Utils_Transform_UE::Has(InEntity)) { return false; }
        auto Mutable = InEntity;
        OutPoi = UCk_Utils_Poi_UE::Cast(Mutable);
        return ck::IsValid(OutPoi);
    }

    auto HasFullComposition(const FCk_Handle& InEntity) -> bool
    { auto Poi = FCk_Handle_Poi{}; return TryGetPoi(InEntity, Poi); }

    auto HasPendingEntityTagRequests(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_EntityTag_Requests>();
    }

    auto GetCategoryTags(const FCk_Handle& InEntity) -> FGameplayTagContainer
    { auto Poi = FCk_Handle_Poi{}; return TryGetPoi(InEntity, Poi) ? UCk_Utils_Poi_UE::Get_CategoryTags(Poi) : FGameplayTagContainer{}; }

    auto GetLabelText(const FCk_Handle& InEntity) -> FString
    {
        auto Poi = FCk_Handle_Poi{};
        if (NOT TryGetPoi(InEntity, Poi)) { return TEXT("--"); }
        if (NOT UCk_Utils_GameplayLabel_UE::Has(Poi)) { return TEXT("(none)"); }
        const FGameplayTag Label = UCk_Utils_GameplayLabel_UE::Get_Label(Poi);
        return Label.IsValid() ? Label.ToString() : TEXT("(none)");
    }

    auto IsDisabled(const FCk_Handle& InEntity) -> bool
    {
        auto Poi = FCk_Handle_Poi{};
        return TryGetPoi(InEntity, Poi)
            && UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(FCk_Handle{Poi}, Tag_Poi_DisabledName);
    }

    auto GetWorldPositionAxisText(const FCk_Handle& InEntity, const int32 InAxis) -> FString
    {
        auto Poi = FCk_Handle_Poi{};
        if (NOT TryGetPoi(InEntity, Poi) || InAxis < 0 || InAxis >= 3) { return TEXT("--"); }
        return ck::Format_UE(TEXT("{:.0f}"), UCk_Utils_Poi_UE::Get_WorldLocation(Poi)[InAxis]);
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    { return bDiffMarked ? CkStyle::Accent() : CkStyle::Text(); }
}

auto SCkInspector_PoiAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _CategoryTagsDiffMarked = InArgs._CategoryTagsDiffMarked;
    _LabelDiffMarked = InArgs._LabelDiffMarked;
    _StateDiffMarked = InArgs._StateDiffMarked;
    _DisabledDiffMarked = InArgs._DisabledDiffMarked;
    _WorldPositionDiffMarked = InArgs._WorldPositionDiffMarked;
    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_CategoryTagRecords() && Build_AuthoredView()) { RootHost->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_PoiAuthored::~SCkInspector_PoiAuthored() { Release(); }

auto SCkInspector_PoiAuthored::Get_IsAvailable() const -> bool
{ return _Active && ck_inspector_poi::HasFullComposition(_Entity); }

auto SCkInspector_PoiAuthored::Get_LabelText() const -> FString
{ return _Active ? ck_inspector_poi::GetLabelText(_Entity) : FString{}; }

auto SCkInspector_PoiAuthored::Get_StateText() const -> FString
{ return NOT Get_IsAvailable() ? TEXT("--") : Get_IsDisabled() ? TEXT("Disabled") : TEXT("Enabled"); }

auto SCkInspector_PoiAuthored::Get_IsDisabled() const -> bool
{
    if (NOT Get_IsAvailable()) { return false; }
    return _PendingDisabled.Get(ck_inspector_poi::IsDisabled(_Entity));
}

auto SCkInspector_PoiAuthored::Get_CanToggleDisabled() const -> bool
{
    return Get_IsAvailable() && NOT _PendingDisabled.IsSet();
}

auto SCkInspector_PoiAuthored::Get_WorldPositionAxisText(const int32 InAxis) const -> FString
{ return _Active ? ck_inspector_poi::GetWorldPositionAxisText(_Entity, InAxis) : FString{}; }

auto SCkInspector_PoiAuthored::Get_StateForeground() const -> FLinearColor
{ return CkStyle::GetToneColor(NOT Get_IsAvailable() ? ECk_Tone::Neutral : Get_IsDisabled() ? ECk_Tone::Err : ECk_Tone::Ok); }

auto SCkInspector_PoiAuthored::Get_StateBackground() const -> FLinearColor
{ return CkStyle::GetToneDimColor(NOT Get_IsAvailable() ? ECk_Tone::Neutral : Get_IsDisabled() ? ECk_Tone::Err : ECk_Tone::Ok); }

auto SCkInspector_PoiAuthored::Build_CategoryTagRecords() -> bool
{
    const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate({FCkUiFieldSchema{TEXT("tag"), ECkUiFieldKind::Text}}, _CategoryTagsCollection);
    if (NOT CreateResult.Succeeded || NOT _CategoryTagsCollection.IsValid()) { _LoadError = FString::Join(CreateResult.Errors, TEXT("\n")); return false; }
    return Refresh_CategoryTagRecords();
}

auto SCkInspector_PoiAuthored::Refresh_CategoryTagRecords() -> bool
{
    if (NOT _CategoryTagsCollection.IsValid()) { return false; }
    auto Records = TArray<FCkUiRecordData>{};
    for (const FGameplayTag& CategoryTag : ck_inspector_poi::GetCategoryTags(_Entity))
    {
        auto Record = FCkUiRecordData{};
        Record.Key = CategoryTag.ToString();
        Record.Fields.Add(TEXT("tag"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Record.Key)});
        Records.Add(MoveTemp(Record));
    }

    const TArray<TSharedPtr<const FCkUiRecord>>& Current = _CategoryTagsCollection->GetRecords();
    bool IsUnchanged = Current.Num() == Records.Num();
    for (int32 Index = 0; IsUnchanged && Index < Current.Num(); ++Index)
    {
        IsUnchanged = Current[Index].IsValid() && Current[Index]->GetKey() == Records[Index].Key;
    }
    if (IsUnchanged) { return true; }

    const FCkUiLoadResult RecordsResult = _CategoryTagsCollection->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded) { _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n")); return false; }
    return true;
}

auto SCkInspector_PoiAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    const TWeakPtr<SCkInspector_PoiAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, WeakWidget](const FString& Name, FString (SCkInspector_PoiAuthored::* Getter)() const)
    { Data.Text.Add(Name, TAttribute<FText>::CreateLambda([WeakWidget, Getter]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); })); };
    BindText(TEXT("poi-label"), &SCkInspector_PoiAuthored::Get_LabelText);
    BindText(TEXT("poi-state"), &SCkInspector_PoiAuthored::Get_StateText);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        Data.Text.Add(FString::Printf(TEXT("poi-world-%c"), TCHAR('x' + Axis)), TAttribute<FText>::CreateLambda([WeakWidget, Axis]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_WorldPositionAxisText(Axis)) : FText::GetEmpty(); }));
    }
    Data.Visibility.Add(TEXT("poi-category-tags-present"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && Widget->_CategoryTagsCollection.IsValid() && Widget->_CategoryTagsCollection->GetRecords().Num() > 0; }));
    Data.Visibility.Add(TEXT("poi-category-tags-empty"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && Widget->_CategoryTagsCollection.IsValid() && Widget->_CategoryTagsCollection->GetRecords().Num() == 0; }));
    Data.Visibility.Add(TEXT("poi-category-tags-unavailable"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("poi-available"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanToggleDisabled(); }));
    Data.Visibility.Add(TEXT("poi-disabled"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsDisabled(); }));
    Data.BoolChanged.Add(TEXT("poi-disabled-changed"), FCkUiOnBoolChanged::CreateLambda([WeakWidget](const bool Value) { if (const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Set_Disabled(Value); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    const auto BindColor = [&Data, WeakWidget](const FString& Name, FLinearColor (SCkInspector_PoiAuthored::* Getter)() const)
    { Data.Color.Add(Name, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Getter]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() ? (Widget.Get()->*Getter)() : FLinearColor::Transparent; })); };
    BindColor(TEXT("poi-state-foreground"), &SCkInspector_PoiAuthored::Get_StateForeground);
    BindColor(TEXT("poi-state-background"), &SCkInspector_PoiAuthored::Get_StateBackground);
    const auto BindDiff = [&Data, WeakWidget](const FString& Name, const bool SCkInspector_PoiAuthored::* Member)
    { Data.Color.Add(Name, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Member]() { const TSharedPtr<SCkInspector_PoiAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_poi::DiffColor(Widget.Get()->*Member) : FLinearColor::Transparent; })); };
    BindDiff(TEXT("poi-category-tags-diff-color"), &SCkInspector_PoiAuthored::_CategoryTagsDiffMarked);
    BindDiff(TEXT("poi-label-diff-color"), &SCkInspector_PoiAuthored::_LabelDiffMarked);
    BindDiff(TEXT("poi-state-diff-color"), &SCkInspector_PoiAuthored::_StateDiffMarked);
    BindDiff(TEXT("poi-disabled-diff-color"), &SCkInspector_PoiAuthored::_DisabledDiffMarked);
    BindDiff(TEXT("poi-world-position-diff-color"), &SCkInspector_PoiAuthored::_WorldPositionDiffMarked);
    Data.Collections.Add(TEXT("poi-category-tags"), _CategoryTagsCollection);
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorPoi.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorPoi.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_PoiAuthored::Set_Disabled(const bool InIsDisabled) -> void
{
    auto Poi = FCk_Handle_Poi{};
    if (NOT Get_CanToggleDisabled() || NOT ck_inspector_poi::TryGetPoi(_Entity, Poi)) { return; }
    _PendingDisabled = InIsDisabled;
    auto MutableEntity = FCk_Handle{Poi};
    if (InIsDisabled) { UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(MutableEntity, Tag_Poi_DisabledName); }
    else if (UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag(
        MutableEntity, Tag_Poi_DisabledName, {}) == ECk_SucceededFailed::Failed)
    {
        _PendingDisabled.Reset();
    }
}

auto SCkInspector_PoiAuthored::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) -> void
{
    SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    const bool IsAvailable = Get_IsAvailable();
    if (NOT IsAvailable)
    {
        _PendingDisabled.Reset();
    }
    else if (_PendingDisabled.IsSet()
        && NOT ck_inspector_poi::HasPendingEntityTagRequests(_Entity))
    {
        _PendingDisabled.Reset();
    }
    if (NOT Refresh_CategoryTagRecords())
    {
        return;
    }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_PoiAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _PendingDisabled.Reset();
    _CategoryTagsCollection.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Poi::~FCkInspector_Poi() { OnDeactivated(); }
auto FCkInspector_Poi::Get_ComponentName() const -> FText { return FText::FromString(TEXT("Poi")); }
auto FCkInspector_Poi::CanInspect(const FCk_Handle& Entity) const -> bool { return ck_inspector_poi::HasFullComposition(Entity); }

auto FCkInspector_Poi::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    if (NOT ck_inspector_poi::HasFullComposition(Entity)) { return Builder.Build(Entity, FString{}); }
    const FCk_Handle CapturedEntity = Entity;
    auto Chips = TArray<FCkInspector_Chip>{};
    for (const FGameplayTag& Tag : ck_inspector_poi::GetCategoryTags(CapturedEntity)) { Chips.Add(FCkInspector_Chip{FText::FromString(Tag.ToString()), ECk_Tone::Neutral}); }
    if (NOT Chips.IsEmpty()) { Builder.AddChipsRow(FText::FromString(TEXT("Category Tags:")), Chips); }
    else { Builder.AddRow(FText::FromString(TEXT("Category Tags:")), [](const FCk_Handle&) { return FText::FromString(TEXT("(none)")); }, CkStyle::TextDim()); }
    Builder.AddRow(FText::FromString(TEXT("Label:")), [CapturedEntity](const FCk_Handle&) { return FText::FromString(ck_inspector_poi::GetLabelText(CapturedEntity)); }, CkStyle::Value_Tag());
    Builder.AddStatusPillRow(FText::FromString(TEXT("State:")), TAttribute<FText>::CreateLambda([CapturedEntity]() { return FText::FromString(ck_inspector_poi::HasFullComposition(CapturedEntity) ? (ck_inspector_poi::IsDisabled(CapturedEntity) ? TEXT("Disabled") : TEXT("Enabled")) : TEXT("--")); }), TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]() { return NOT ck_inspector_poi::HasFullComposition(CapturedEntity) ? ECk_Tone::Neutral : ck_inspector_poi::IsDisabled(CapturedEntity) ? ECk_Tone::Err : ECk_Tone::Ok; }));
    const TSharedRef<ck_inspector_poi::FPendingDisabledState> PendingDisabled =
        MakeShared<ck_inspector_poi::FPendingDisabledState>();
    const auto ResolveDisabled = [CapturedEntity, PendingDisabled]()
    {
        if (NOT ck_inspector_poi::HasFullComposition(CapturedEntity))
        {
            PendingDisabled->Desired.Reset();
            return false;
        }
        const bool IsDisabled = ck_inspector_poi::IsDisabled(CapturedEntity);
        if (PendingDisabled->Desired.IsSet()
            && NOT ck_inspector_poi::HasPendingEntityTagRequests(CapturedEntity))
        {
            PendingDisabled->Desired.Reset();
        }
        return PendingDisabled->Desired.Get(IsDisabled);
    };
    Builder.AddToggleRow(
        FText::FromString(TEXT("Disabled:")),
        TAttribute<bool>::CreateLambda(ResolveDisabled),
        [CapturedEntity, PendingDisabled](const bool Value)
        {
            auto Poi = FCk_Handle_Poi{};
            if (PendingDisabled->Desired.IsSet()
                || NOT ck_inspector_poi::TryGetPoi(CapturedEntity, Poi)) { return; }
            PendingDisabled->Desired = Value;
            auto Mutable = FCk_Handle{Poi};
            if (Value)
            {
                UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(Mutable, Tag_Poi_DisabledName);
            }
            else if (UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag(
                Mutable, Tag_Poi_DisabledName, {}) == ECk_SucceededFailed::Failed)
            {
                PendingDisabled->Desired.Reset();
            }
        });
    auto Components = TArray<TAttribute<FText>>{};
    for (int32 Axis = 0; Axis < 3; ++Axis) { Components.Add(TAttribute<FText>::CreateLambda([CapturedEntity, Axis]() { return FText::FromString(ck_inspector_poi::GetWorldPositionAxisText(CapturedEntity, Axis)); })); }
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("World Pos:")), Components);
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_Poi::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_PoiAuthored> Authored = SNew(SCkInspector_PoiAuthored)
        .Entity(Entity)
        .CategoryTagsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Category Tags:")))
        .LabelDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Label:")))
        .StateDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("State:")))
        .DisabledDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Disabled:")))
        .WorldPositionDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("World Pos:")));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Poi::Tick(const FCk_Handle&, const float) -> void
{ _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_PoiAuthored>& Instance) { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); }); }

auto FCkInspector_Poi::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_PoiAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_PoiAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
