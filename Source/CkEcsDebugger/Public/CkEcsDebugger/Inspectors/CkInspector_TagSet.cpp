#include "CkInspector_TagSet.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkTagSet/CkTagSet_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_TagSet)

namespace ck_inspector_tagset
{
    constexpr TCHAR AddTagPort[] = TEXT("tag-set-add-tag-port");

    auto TryGetTagSet(const FCk_Handle& InEntity, FCk_Handle_TagSet& OutTagSet) -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_TagSet_UE::Has(InEntity)) { return false; }
        auto Mutable = InEntity;
        OutTagSet = UCk_Utils_TagSet_UE::Cast(Mutable);
        return ck::IsValid(OutTagSet);
    }

    auto Get_CanRequest(const FCk_Handle& InEntity) -> bool
    {
        auto TagSet = FCk_Handle_TagSet{};
        return TryGetTagSet(InEntity, TagSet)
            && ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
    }

    auto Build_RequestDisabledReason(const FCk_Handle& InEntity) -> FText
    {
        auto TagSet = FCk_Handle_TagSet{};
        if (NOT TryGetTagSet(InEntity, TagSet)) { return FText::FromString(TEXT("Tag Set is unavailable.")); }
        const FCk_DebugRequest_GateVerdict Verdict = ck::DebugRequestGate::Evaluate(
            InEntity, ECk_DebugRequest_Requirement::AuthorityOnly);
        return Verdict.IsEnabled ? FText::GetEmpty() : Verdict.Reason;
    }

    auto Build_CountText(const FCk_Handle& InEntity) -> FString
    {
        auto TagSet = FCk_Handle_TagSet{};
        return TryGetTagSet(InEntity, TagSet)
            ? FCkInspectorWidgetBuilder::Format_Count(UCk_Utils_TagSet_UE::Get_NumTags(TagSet)).ToString()
            : TEXT("--");
    }

    auto Build_CountValue(const FCk_Handle& InEntity) -> int32
    {
        auto TagSet = FCk_Handle_TagSet{};
        return TryGetTagSet(InEntity, TagSet) ? UCk_Utils_TagSet_UE::Get_NumTags(TagSet) : 0;
    }

    auto Get_Tags(const FCk_Handle& InEntity) -> FGameplayTagContainer
    {
        auto TagSet = FCk_Handle_TagSet{};
        return TryGetTagSet(InEntity, TagSet) ? UCk_Utils_TagSet_UE::Get_Tags(TagSet) : FGameplayTagContainer{};
    }

    auto Request_AddTag(const FCk_Handle& InEntity, FGameplayTag InTag) -> void
    {
        auto TagSet = FCk_Handle_TagSet{};
        if (NOT InTag.IsValid() || NOT Get_CanRequest(InEntity) || NOT TryGetTagSet(InEntity, TagSet)) { return; }
        UCk_Utils_TagSet_UE::Request_AddTag(TagSet, InTag, {});
    }

    auto Request_RemoveTag(const FCk_Handle& InEntity, FGameplayTag InTag) -> void
    {
        auto TagSet = FCk_Handle_TagSet{};
        if (NOT InTag.IsValid() || NOT Get_CanRequest(InEntity) || NOT TryGetTagSet(InEntity, TagSet)) { return; }
        UCk_Utils_TagSet_UE::Request_RemoveTag(TagSet, InTag, {});
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_TagSetAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _AddTagWidget = InArgs._AddTagWidget;
    _CountDiffMarked = InArgs._CountDiffMarked;
    _TagsDiffMarked = InArgs._TagsDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_TagRecords() && Build_AuthoredView() && Populate_NativePort())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_TagSetAuthored::~SCkInspector_TagSetAuthored()
{
    Release();
}

auto SCkInspector_TagSetAuthored::Get_CountText() const -> FString
{
    return _Active ? ck_inspector_tagset::Build_CountText(_Entity) : FString{};
}

auto SCkInspector_TagSetAuthored::Get_IsAvailable() const -> bool
{
    auto TagSet = FCk_Handle_TagSet{};
    return _Active && ck_inspector_tagset::TryGetTagSet(_Entity, TagSet);
}

auto SCkInspector_TagSetAuthored::Get_CanRequest() const -> bool
{
    return _Active && ck_inspector_tagset::Get_CanRequest(_Entity);
}

auto SCkInspector_TagSetAuthored::Build_TagRecords() -> bool
{
    const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate(
        {
            FCkUiFieldSchema{TEXT("tag"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("tag-color"), ECkUiFieldKind::Color},
            FCkUiFieldSchema{TEXT("remove-label"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("remove-visible"), ECkUiFieldKind::Bool},
            FCkUiFieldSchema{TEXT("remove-tooltip"), ECkUiFieldKind::Text},
        },
        _TagsCollection);
    if (NOT CreateResult.Succeeded || NOT _TagsCollection.IsValid())
    {
        _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
        return false;
    }

    const FGameplayTagContainer Tags = ck_inspector_tagset::Get_Tags(_Entity);
    auto TagStrings = TArray<FString>{};
    TagStrings.Reserve(Tags.Num());
    for (const FGameplayTag& GameplayTag : Tags) { TagStrings.Add(GameplayTag.ToString()); }
    _TagsRowVisible = NOT TagStrings.IsEmpty()
        && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("Tags:"), FString::Join(TagStrings, TEXT(" ")));
    _CountRowVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("Count:"), ck_inspector_tagset::Build_CountText(_Entity));

    auto Records = TArray<FCkUiRecordData>{};
    Records.Reserve(TagStrings.Num());
    for (const FString& TagString : TagStrings)
    {
        auto Record = FCkUiRecordData{};
        Record.Key = TagString;
        Record.Fields.Add(TEXT("tag"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TagString)});
        Record.Fields.Add(TEXT("tag-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = ck_inspector_tagset::DiffColor(FCkInspector_DiffMarkScope::Is_LabelMarked(TagString))});
        Record.Fields.Add(TEXT("remove-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(TEXT("Remove"))});
        Record.Fields.Add(TEXT("remove-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool,
            .Bool = ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection())
                && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TagString, TEXT("Remove"))});
        Record.Fields.Add(TEXT("remove-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(FString::Printf(TEXT("UCk_Utils_TagSet_UE::Request_RemoveTag(%s)"), *TagString))});
        Records.Add(MoveTemp(Record));
    }

    const FCkUiLoadResult RecordsResult = _TagsCollection->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    return true;
}

auto SCkInspector_TagSetAuthored::Build_AuthoredView() -> bool
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

    const TSharedRef<SBox> AddTagPort = SNew(SBox);
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(ck_inspector_tagset::AddTagPort, AddTagPort);

    const TWeakPtr<SCkInspector_TagSetAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("tag-set-count"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString(Widget->Get_CountText()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("tag-set-request-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_tagset::Build_RequestDisabledReason(Widget->_Entity) : FText::GetEmpty();
    }));
    Data.Color.Add(TEXT("tag-set-count-diff-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_tagset::DiffColor(Widget->_CountDiffMarked) : FLinearColor::Transparent;
    }));
    Data.Color.Add(TEXT("tag-set-tags-diff-color"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_tagset::DiffColor(Widget->_TagsDiffMarked) : FLinearColor::Transparent;
    }));
    Data.Color.Add(TEXT("tag-set-count-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable() ? CkStyle::GetToneColor(ECk_Tone::Info) : CkStyle::GetToneColor(ECk_Tone::Neutral);
    }));
    Data.Color.Add(TEXT("tag-set-count-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable() ? CkStyle::GetToneDimColor(ECk_Tone::Info) : CkStyle::GetToneDimColor(ECk_Tone::Neutral);
    }));
    Data.Visibility.Add(TEXT("tag-set-count-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() && Widget->_CountRowVisible;
    }));
    Data.Visibility.Add(TEXT("tag-set-tags-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable() && Widget->_TagsRowVisible;
    }));
    Data.Visibility.Add(TEXT("tag-set-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("tag-set-authority-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    });
    Data.Collections.Add(TEXT("tag-set-tags"), _TagsCollection);
    Data.ItemActions.Add(TEXT("tag-set-remove"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InStableKey)
    {
        const TSharedPtr<SCkInspector_TagSetAuthored> Widget = WeakWidget.Pin();
        if (Widget.IsValid() && NOT Widget->Is_Inert()) { Widget->Request_RemoveTag(InStableKey); }
    }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTagSet.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTagSet.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _AddTagPort = AddTagPort;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_TagSetAuthored::Populate_NativePort() -> bool
{
    if (NOT _Active || NOT _AddTagPort.IsValid() || NOT _AddTagWidget.IsValid()) { return false; }
    _AddTagPort->SetContent(_AddTagWidget.ToSharedRef());
    return true;
}

auto SCkInspector_TagSetAuthored::Detach_NativePort() -> void
{
    if (_AddTagPort.IsValid()) { _AddTagPort->SetContent(SNullWidget::NullWidget); }
}

auto SCkInspector_TagSetAuthored::Request_RemoveTag(const FString& InStableKey) -> void
{
    constexpr bool ErrorIfNotFound = false;
    const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName{*InStableKey}, ErrorIfNotFound);
    if (_Active) { ck_inspector_tagset::Request_RemoveTag(_Entity, GameplayTag); }
}

auto SCkInspector_TagSetAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_TagSetAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    Detach_NativePort();
    _Entity = FCk_Handle{};
    _AddTagWidget.Reset();
    _AddTagPort.Reset();
    _TagsCollection.Reset();
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_TagSet::~FCkInspector_TagSet()
{
    OnDeactivated();
}

auto FCkInspector_TagSet::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Tag Set"));
}

auto FCkInspector_TagSet::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_TagSet_UE::Has(Entity);
}

auto FCkInspector_TagSet::Build_NativeBody(const FCk_Handle& Entity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    auto TagSet = FCk_Handle_TagSet{};
    if (NOT ck_inspector_tagset::TryGetTagSet(Entity, TagSet)) { return Builder.Build(Entity, InFilter); }

    const FCk_Handle CapturedEntity = Entity;
    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Count:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]() { return ck_inspector_tagset::Build_CountValue(CapturedEntity); }),
        ECk_Tone::Info,
        FText::FromString(TEXT("tags")));

    const FGameplayTagContainer Tags = UCk_Utils_TagSet_UE::Get_Tags(TagSet);
    auto Chips = TArray<FCkInspector_Chip>{};
    Chips.Reserve(Tags.Num());
    for (const FGameplayTag& Tag : Tags)
    { Chips.Add(FCkInspector_Chip{FText::FromString(Tag.ToString()), ECk_Tone::Neutral}); }
    if (NOT Chips.IsEmpty()) { Builder.AddChipsRow(FText::FromString(TEXT("Tags:")), Chips); }

    Builder.AddTagEntryRow(
        FText::FromString(TEXT("Add Tag:")),
        TAttribute<FText>{},
        [CapturedEntity](FGameplayTag InTag) { ck_inspector_tagset::Request_AddTag(CapturedEntity, InTag); },
        ECk_DebugRequest_Requirement::AuthorityOnly);

    for (const FGameplayTag& Tag : Tags)
    {
        const FString TagString = Tag.ToString();
        Builder.AddActionRow(
            FText::FromString(TagString),
            {FCkInspector_Action{
                FText::FromString(TEXT("Remove")),
                FText::FromString(FString::Printf(TEXT("UCk_Utils_TagSet_UE::Request_RemoveTag(%s)"), *TagString)),
                [CapturedEntity, Tag]() { ck_inspector_tagset::Request_RemoveTag(CapturedEntity, Tag); },
                ECk_DebugRequest_Requirement::AuthorityOnly}});
    }
    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_TagSet::Build_NativeAddTagRow(const FCk_Handle& Entity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    const FCk_Handle CapturedEntity = Entity;
    Builder.AddTagEntryRow(
        FText::FromString(TEXT("Add Tag:")),
        TAttribute<FText>{},
        [CapturedEntity](FGameplayTag InTag) { ck_inspector_tagset::Request_AddTag(CapturedEntity, InTag); },
        ECk_DebugRequest_Requirement::AuthorityOnly);
    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_TagSet::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString{});
}

auto FCkInspector_TagSet::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_TagSetAuthored> Authored = SNew(SCkInspector_TagSetAuthored)
        .Entity(Entity)
        .Filter(InFilter)
        .AddTagWidget(Build_NativeAddTagRow(Entity, InFilter))
        .CountDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Count:")))
        .TagsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Tags:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_TagSet::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_TagSetAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_TagSet::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_TagSetAuthored>& WeakInstance : _AuthoredInstances)
    { if (const TSharedPtr<SCkInspector_TagSetAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
