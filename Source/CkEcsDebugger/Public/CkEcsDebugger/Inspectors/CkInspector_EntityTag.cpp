#include "CkInspector_EntityTag.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTag)

namespace ck_inspector_entity_tag
{
    constexpr TCHAR AddNamePort[] = TEXT("entity-tag-add-name-port");
    constexpr TCHAR AddGameplayTagPort[] = TEXT("entity-tag-add-gameplay-tag-port");
    constexpr TCHAR FNameKeyPrefix[] = TEXT("fname:");
    constexpr TCHAR GameplayTagRootKeyPrefix[] = TEXT("root:");

    auto IsInspectable(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && UCk_Utils_EntityTag_UE::Has_AnyTag(InEntity);
    }

    auto GetFNameTags(const FCk_Handle& InEntity) -> TArray<FName>
    {
        return IsInspectable(InEntity) ? UCk_Utils_EntityTag_UE::Get_AllTags(InEntity) : TArray<FName>{};
    }

    auto GetGameplayTagRoots(const FCk_Handle& InEntity) -> FGameplayTagContainer
    {
        return IsInspectable(InEntity) ? UCk_Utils_EntityTag_UE::Get_AllTagsAsContainer(InEntity) : FGameplayTagContainer{};
    }

    auto BuildDisabledReason(const FCk_Handle& InEntity) -> FText
    {
        return IsInspectable(InEntity) ? FText::GetEmpty() : FText::FromString(TEXT("Entity Tag is unavailable."));
    }

    auto RequestAddName(const FCk_Handle& InEntity, const FName InTag) -> void
    {
        if (NOT IsInspectable(InEntity) || InTag.IsNone()) { return; }
        auto MutableEntity = InEntity;
        UCk_Utils_EntityTag_UE::Add(MutableEntity, InTag);
    }

    auto RequestAddGameplayTag(const FCk_Handle& InEntity, const FGameplayTag InTag) -> void
    {
        if (NOT IsInspectable(InEntity) || NOT InTag.IsValid()) { return; }
        auto MutableEntity = InEntity;
        UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(MutableEntity, InTag);
    }

    auto RequestRemoveName(const FCk_Handle& InEntity, const FName InTag) -> void
    {
        if (NOT IsInspectable(InEntity) || InTag.IsNone()) { return; }
        auto MutableEntity = InEntity;
        UCk_Utils_EntityTag_UE::Request_TryRemove(MutableEntity, InTag, {});
    }

    auto RequestRemoveGameplayTagRoot(const FCk_Handle& InEntity, const FGameplayTag InTag) -> void
    {
        if (NOT IsInspectable(InEntity) || NOT InTag.IsValid()) { return; }
        auto MutableEntity = InEntity;
        UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag(MutableEntity, InTag, {});
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    auto AreRecordKeysUnchanged(const TSharedPtr<FCkUiCollection>& InCollection, const TArray<FCkUiRecordData>& InRecords) -> bool
    {
        if (NOT InCollection.IsValid()) { return false; }
        const TArray<TSharedPtr<const FCkUiRecord>>& Current = InCollection->GetRecords();
        if (Current.Num() != InRecords.Num()) { return false; }
        for (int32 Index = 0; Index < Current.Num(); ++Index)
        {
            if (NOT Current[Index].IsValid() || Current[Index]->GetKey() != InRecords[Index].Key) { return false; }
        }
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_EntityTagAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _AddNameWidget = InArgs._AddNameWidget;
    _AddGameplayTagWidget = InArgs._AddGameplayTagWidget;
    _FNameCountDiffMarked = InArgs._FNameCountDiffMarked;
    _FNameTagsDiffMarked = InArgs._FNameTagsDiffMarked;
    _GameplayTagRootCountDiffMarked = InArgs._GameplayTagRootCountDiffMarked;
    _GameplayTagRootsDiffMarked = InArgs._GameplayTagRootsDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_Records() && Build_AuthoredView() && Populate_NativePorts())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_EntityTagAuthored::~SCkInspector_EntityTagAuthored()
{
    Release();
}

auto SCkInspector_EntityTagAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_entity_tag::IsInspectable(_Entity);
}

auto SCkInspector_EntityTagAuthored::Get_FNameCountText() const -> FString
{
    return Get_IsAvailable() ? FCkInspectorWidgetBuilder::Format_Count(ck_inspector_entity_tag::GetFNameTags(_Entity).Num()).ToString() : TEXT("--");
}

auto SCkInspector_EntityTagAuthored::Get_GameplayTagRootCountText() const -> FString
{
    return Get_IsAvailable()
        ? FString::FromInt(ck_inspector_entity_tag::GetGameplayTagRoots(_Entity).Num())
        : TEXT("--");
}

auto SCkInspector_EntityTagAuthored::Build_Records() -> bool
{
    const FCkUiLoadResult FNameResult = FCkUiCollection::TryCreate(
        {
            FCkUiFieldSchema{TEXT("tag"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("tag-color"), ECkUiFieldKind::Color},
            FCkUiFieldSchema{TEXT("remove-label"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("remove-visible"), ECkUiFieldKind::Bool},
            FCkUiFieldSchema{TEXT("remove-tooltip"), ECkUiFieldKind::Text},
        }, _FNameTagsCollection);
    const FCkUiLoadResult RootResult = FCkUiCollection::TryCreate(
        {
            FCkUiFieldSchema{TEXT("tag"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("tag-color"), ECkUiFieldKind::Color},
            FCkUiFieldSchema{TEXT("remove-label"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("remove-visible"), ECkUiFieldKind::Bool},
            FCkUiFieldSchema{TEXT("remove-tooltip"), ECkUiFieldKind::Text},
        }, _GameplayTagRootsCollection);
    if (NOT FNameResult.Succeeded || NOT RootResult.Succeeded || NOT _FNameTagsCollection.IsValid() || NOT _GameplayTagRootsCollection.IsValid())
    {
        auto Errors = FNameResult.Errors;
        Errors.Append(RootResult.Errors);
        if (NOT _FNameTagsCollection.IsValid()) { Errors.Add(TEXT("Entity Tag FName collection is unavailable.")); }
        if (NOT _GameplayTagRootsCollection.IsValid()) { Errors.Add(TEXT("Entity Tag GameplayTag root collection is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    return Refresh_Records();
}

auto SCkInspector_EntityTagAuthored::Refresh_Records() -> bool
{
    if (NOT _FNameTagsCollection.IsValid() || NOT _GameplayTagRootsCollection.IsValid()) { return false; }

    const TArray<FName> FNameTags = ck_inspector_entity_tag::GetFNameTags(_Entity);
    TArray<FGameplayTag> GameplayTagRoots;
    for (const FGameplayTag& Root : ck_inspector_entity_tag::GetGameplayTagRoots(_Entity)) { GameplayTagRoots.Add(Root); }

    auto FNameStrings = TArray<FString>{};
    FNameStrings.Reserve(FNameTags.Num());
    for (const FName EntityTag : FNameTags) { FNameStrings.Add(EntityTag.ToString()); }
    auto RootStrings = TArray<FString>{};
    RootStrings.Reserve(GameplayTagRoots.Num());
    for (const FGameplayTag Root : GameplayTagRoots) { RootStrings.Add(Root.ToString()); }

    _FNameCountRowVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("FName Tag Count:"), Get_FNameCountText());
    _FNameTagsRowVisible = NOT FNameStrings.IsEmpty() && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("FName Tags:"), FString::Join(FNameStrings, TEXT(" ")));
    _GameplayTagRootCountRowVisible = NOT RootStrings.IsEmpty()
        && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("GameplayTag Roots:"), Get_GameplayTagRootCountText());
    _GameplayTagRootsRowVisible = NOT RootStrings.IsEmpty() && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("Roots:"), FString::Join(RootStrings, TEXT(" ")));

    const bool EditControlsVisible = ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection());
    auto FNameRecords = TArray<FCkUiRecordData>{};
    FNameRecords.Reserve(FNameStrings.Num());
    for (const FString& TagString : FNameStrings)
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(TagString))
        { _FNameDiffMarkedKeys.Add(TagString); }
        auto Record = FCkUiRecordData{};
        Record.Key = FString{ck_inspector_entity_tag::FNameKeyPrefix} + TagString;
        Record.Fields.Add(TEXT("tag"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TagString)});
        Record.Fields.Add(TEXT("tag-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = ck_inspector_entity_tag::DiffColor(_FNameDiffMarkedKeys.Contains(TagString))});
        Record.Fields.Add(TEXT("remove-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Remove"))});
        Record.Fields.Add(TEXT("remove-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = EditControlsVisible && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TagString, TEXT("Remove"))});
        Record.Fields.Add(TEXT("remove-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(ck::Format_UE(TEXT("UCk_Utils_EntityTag_UE::Request_TryRemove({})"), TagString))});
        FNameRecords.Add(MoveTemp(Record));
    }

    auto RootRecords = TArray<FCkUiRecordData>{};
    RootRecords.Reserve(RootStrings.Num());
    for (const FString& RootString : RootStrings)
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(RootString))
        { _GameplayTagRootDiffMarkedKeys.Add(RootString); }
        auto Record = FCkUiRecordData{};
        Record.Key = FString{ck_inspector_entity_tag::GameplayTagRootKeyPrefix} + RootString;
        Record.Fields.Add(TEXT("tag"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(RootString)});
        Record.Fields.Add(TEXT("tag-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = ck_inspector_entity_tag::DiffColor(_GameplayTagRootDiffMarkedKeys.Contains(RootString))});
        Record.Fields.Add(TEXT("remove-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Remove Root"))});
        Record.Fields.Add(TEXT("remove-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = EditControlsVisible && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, RootString, TEXT("Remove Root"))});
        Record.Fields.Add(TEXT("remove-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(ck::Format_UE(TEXT("UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag({})"), RootString))});
        RootRecords.Add(MoveTemp(Record));
    }

    if (NOT ck_inspector_entity_tag::AreRecordKeysUnchanged(_FNameTagsCollection, FNameRecords))
    {
        const FCkUiLoadResult Result = _FNameTagsCollection->TrySetRecords(MoveTemp(FNameRecords));
        if (NOT Result.Succeeded) { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    }
    if (NOT ck_inspector_entity_tag::AreRecordKeysUnchanged(_GameplayTagRootsCollection, RootRecords))
    {
        const FCkUiLoadResult Result = _GameplayTagRootsCollection->TrySetRecords(MoveTemp(RootRecords));
        if (NOT Result.Succeeded) { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    }
    return true;
}

auto SCkInspector_EntityTagAuthored::Build_AuthoredView() -> bool
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

    const TSharedRef<SBox> AddNamePort = SNew(SBox);
    const TSharedRef<SBox> AddGameplayTagPort = SNew(SBox);
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(ck_inspector_entity_tag::AddNamePort, AddNamePort);
    NativeBindings.Add(ck_inspector_entity_tag::AddGameplayTagPort, AddGameplayTagPort);
    const TWeakPtr<SCkInspector_EntityTagAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, WeakWidget](const FString& Name, FString (SCkInspector_EntityTagAuthored::* Getter)() const)
    { Data.Text.Add(Name, TAttribute<FText>::CreateLambda([WeakWidget, Getter]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); })); };
    BindText(TEXT("entity-tag-fname-count"), &SCkInspector_EntityTagAuthored::Get_FNameCountText);
    BindText(TEXT("entity-tag-root-count"), &SCkInspector_EntityTagAuthored::Get_GameplayTagRootCountText);
    Data.Text.Add(TEXT("entity-tag-request-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_entity_tag::BuildDisabledReason(Widget->_Entity) : FText::GetEmpty(); }));
    const auto BindColor = [&Data, WeakWidget](const FString& Name, const bool SCkInspector_EntityTagAuthored::* Member)
    { Data.Color.Add(Name, TAttribute<FLinearColor>::CreateLambda([WeakWidget, Member]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_entity_tag::DiffColor(Widget.Get()->*Member) : FLinearColor::Transparent; })); };
    BindColor(TEXT("entity-tag-fname-count-diff-color"), &SCkInspector_EntityTagAuthored::_FNameCountDiffMarked);
    BindColor(TEXT("entity-tag-fname-tags-diff-color"), &SCkInspector_EntityTagAuthored::_FNameTagsDiffMarked);
    BindColor(TEXT("entity-tag-root-count-diff-color"), &SCkInspector_EntityTagAuthored::_GameplayTagRootCountDiffMarked);
    BindColor(TEXT("entity-tag-roots-diff-color"), &SCkInspector_EntityTagAuthored::_GameplayTagRootsDiffMarked);
    Data.Color.Add(TEXT("entity-tag-count-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() ? CkStyle::GetToneColor(ECk_Tone::Info) : CkStyle::GetToneColor(ECk_Tone::Neutral); }));
    Data.Color.Add(TEXT("entity-tag-count-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() ? CkStyle::GetToneDimColor(ECk_Tone::Info) : CkStyle::GetToneDimColor(ECk_Tone::Neutral); }));
    const auto BindVisible = [&Data, WeakWidget](const FString& Name, const bool SCkInspector_EntityTagAuthored::* Member)
    { Data.Visibility.Add(Name, TAttribute<bool>::CreateLambda([WeakWidget, Member]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && Widget.Get()->*Member; })); };
    BindVisible(TEXT("entity-tag-fname-count-visible"), &SCkInspector_EntityTagAuthored::_FNameCountRowVisible);
    BindVisible(TEXT("entity-tag-fname-tags-visible"), &SCkInspector_EntityTagAuthored::_FNameTagsRowVisible);
    BindVisible(TEXT("entity-tag-root-count-visible"), &SCkInspector_EntityTagAuthored::_GameplayTagRootCountRowVisible);
    BindVisible(TEXT("entity-tag-roots-visible"), &SCkInspector_EntityTagAuthored::_GameplayTagRootsRowVisible);
    Data.Visibility.Add(TEXT("entity-tag-available"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("entity-tag-request-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]() { const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    Data.Collections.Add(TEXT("entity-tag-fname-tags"), _FNameTagsCollection);
    Data.Collections.Add(TEXT("entity-tag-gameplay-tag-roots"), _GameplayTagRootsCollection);
    Data.ItemActions.Add(TEXT("entity-tag-remove-fname"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key) { if (const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert()) { Widget->Request_RemoveName(Key); } }));
    Data.ItemActions.Add(TEXT("entity-tag-remove-root"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key) { if (const TSharedPtr<SCkInspector_EntityTagAuthored> Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert()) { Widget->Request_RemoveGameplayTagRoot(Key); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTag.ui.html")), FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTag.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate;
    _AddNamePort = AddNamePort;
    _AddGameplayTagPort = AddGameplayTagPort;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_EntityTagAuthored::Populate_NativePorts() -> bool
{
    if (NOT _Active || NOT _AddNamePort.IsValid() || NOT _AddGameplayTagPort.IsValid() || NOT _AddNameWidget.IsValid() || NOT _AddGameplayTagWidget.IsValid()) { return false; }
    _AddNamePort->SetContent(_AddNameWidget.ToSharedRef());
    _AddGameplayTagPort->SetContent(_AddGameplayTagWidget.ToSharedRef());
    return true;
}

auto SCkInspector_EntityTagAuthored::Detach_NativePorts() -> void
{
    if (_AddNamePort.IsValid()) { _AddNamePort->SetContent(SNullWidget::NullWidget); }
    if (_AddGameplayTagPort.IsValid()) { _AddGameplayTagPort->SetContent(SNullWidget::NullWidget); }
}

auto SCkInspector_EntityTagAuthored::Request_RemoveName(const FString& InStableKey) -> void
{
    FString TagString = InStableKey;
    if (NOT TagString.RemoveFromStart(ck_inspector_entity_tag::FNameKeyPrefix)) { return; }
    ck_inspector_entity_tag::RequestRemoveName(_Entity, FName{*TagString});
}

auto SCkInspector_EntityTagAuthored::Request_RemoveGameplayTagRoot(const FString& InStableKey) -> void
{
    FString RootString = InStableKey;
    if (NOT RootString.RemoveFromStart(ck_inspector_entity_tag::GameplayTagRootKeyPrefix)) { return; }
    constexpr bool ErrorIfNotFound = false;
    ck_inspector_entity_tag::RequestRemoveGameplayTagRoot(_Entity, FGameplayTag::RequestGameplayTag(FName{*RootString}, ErrorIfNotFound));
}

auto SCkInspector_EntityTagAuthored::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT Refresh_Records()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_EntityTagAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    Detach_NativePorts();
    _Entity = {};
    _AddNameWidget.Reset();
    _AddGameplayTagWidget.Reset();
    _AddNamePort.Reset();
    _AddGameplayTagPort.Reset();
    _FNameTagsCollection.Reset();
    _GameplayTagRootsCollection.Reset();
    _FNameDiffMarkedKeys.Reset();
    _GameplayTagRootDiffMarkedKeys.Reset();
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_EntityTag::~FCkInspector_EntityTag() { OnDeactivated(); }
auto FCkInspector_EntityTag::Get_ComponentName() const -> FText { return FText::FromString(TEXT("Entity Tag")); }
auto FCkInspector_EntityTag::CanInspect(const FCk_Handle& Entity) const -> bool { return ck_inspector_entity_tag::IsInspectable(Entity); }

auto FCkInspector_EntityTag::Build_NativeBody(const FCk_Handle& Entity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    if (NOT ck_inspector_entity_tag::IsInspectable(Entity)) { return Builder.Build(Entity, InFilter); }
    const FCk_Handle CapturedEntity = Entity;
    const TArray<FName> FNameTags = ck_inspector_entity_tag::GetFNameTags(Entity);
    const FGameplayTagContainer GameplayTagRoots = ck_inspector_entity_tag::GetGameplayTagRoots(Entity);
    Builder.AddCountBadgeRow(FText::FromString(TEXT("FName Tag Count:")), FNameTags.Num(), ECk_Tone::Info, FText::FromString(TEXT("tags")));
    auto FNameChips = TArray<FCkInspector_Chip>{};
    for (const FName Tag : FNameTags) { FNameChips.Add(FCkInspector_Chip{FText::FromName(Tag), ECk_Tone::Neutral}); }
    if (NOT FNameChips.IsEmpty()) { Builder.AddChipsRow(FText::FromString(TEXT("FName Tags:")), FNameChips); }
    if (NOT GameplayTagRoots.IsEmpty())
    {
        Builder.AddRow(FText::FromString(TEXT("GameplayTag Roots:")), [Count = GameplayTagRoots.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(Count)); }, CkStyle::Value_Numeric());
        auto RootChips = TArray<FCkInspector_Chip>{};
        for (const FGameplayTag Root : GameplayTagRoots) { RootChips.Add(FCkInspector_Chip{FText::FromName(Root.GetTagName()), ECk_Tone::Neutral}); }
        Builder.AddChipsRow(FText::FromString(TEXT("Roots:")), RootChips);
    }
    Builder.AddNameEntryRow(FText::FromString(TEXT("Add Tag (FName):")), TAttribute<FText>{}, [CapturedEntity](const FName Tag) { ck_inspector_entity_tag::RequestAddName(CapturedEntity, Tag); });
    Builder.AddTagEntryRow(FText::FromString(TEXT("Add Tag (GameplayTag):")), TAttribute<FText>{}, [CapturedEntity](const FGameplayTag Tag) { ck_inspector_entity_tag::RequestAddGameplayTag(CapturedEntity, Tag); });
    for (const FName Tag : FNameTags)
    { Builder.AddActionRow(FText::FromName(Tag), {FCkInspector_Action{FText::FromString(TEXT("Remove")), FText::FromString(ck::Format_UE(TEXT("UCk_Utils_EntityTag_UE::Request_TryRemove({})"), Tag.ToString())), [CapturedEntity, Tag]() { ck_inspector_entity_tag::RequestRemoveName(CapturedEntity, Tag); }, ECk_DebugRequest_Requirement::LocalOk}}); }
    for (const FGameplayTag Root : GameplayTagRoots)
    { Builder.AddActionRow(FText::FromName(Root.GetTagName()), {FCkInspector_Action{FText::FromString(TEXT("Remove Root")), FText::FromString(ck::Format_UE(TEXT("UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag({})"), Root.ToString())), [CapturedEntity, Root]() { ck_inspector_entity_tag::RequestRemoveGameplayTagRoot(CapturedEntity, Root); }, ECk_DebugRequest_Requirement::LocalOk}}); }
    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_EntityTag::Build_NativeAddNameRow(const FCk_Handle& Entity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddNameEntryRow(FText::FromString(TEXT("Add Tag (FName):")), TAttribute<FText>{}, [CapturedEntity](const FName Tag) { ck_inspector_entity_tag::RequestAddName(CapturedEntity, Tag); });
    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_EntityTag::Build_NativeAddGameplayTagRow(const FCk_Handle& Entity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddTagEntryRow(FText::FromString(TEXT("Add Tag (GameplayTag):")), TAttribute<FText>{}, [CapturedEntity](const FGameplayTag Tag) { ck_inspector_entity_tag::RequestAddGameplayTag(CapturedEntity, Tag); });
    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_EntityTag::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> { return Build_Inspector(Entity, FString{}); }

auto FCkInspector_EntityTag::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    const TSharedRef<SCkInspector_EntityTagAuthored> Authored = SNew(SCkInspector_EntityTagAuthored)
        .Entity(Entity).Filter(InFilter)
        .AddNameWidget(Build_NativeAddNameRow(Entity, InFilter))
        .AddGameplayTagWidget(Build_NativeAddGameplayTagRow(Entity, InFilter))
        .FNameCountDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("FName Tag Count:")))
        .FNameTagsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("FName Tags:")))
        .GameplayTagRootCountDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("GameplayTag Roots:")))
        .GameplayTagRootsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Roots:")));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_EntityTag::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_EntityTagAuthored>& Instance) { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}

auto FCkInspector_EntityTag::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_EntityTagAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_EntityTagAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
