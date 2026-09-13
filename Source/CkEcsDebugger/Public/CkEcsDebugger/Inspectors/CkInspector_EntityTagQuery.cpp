#include "CkInspector_EntityTagQuery.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEntityTag/Query/CkEntityTagQuery_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTagQuery)

namespace ck_inspector_entity_tag_query
{
    constexpr TCHAR RequirementPrefix[] = TEXT("requirement|");
    constexpr TCHAR MatchPrefix[] = TEXT("match|");

    auto TryGetQuery(const FCk_Handle& InEntity, FCk_Handle_EntityTagQuery& OutQuery) -> bool
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_EntityTagQuery_UE::Has(InEntity)) { return false; }
        auto Mutable = InEntity;
        OutQuery = UCk_Utils_EntityTagQuery_UE::Cast(Mutable);
        return ck::IsValid(OutQuery);
    }

    auto Get_CanRequest(const FCk_Handle& InEntity) -> bool
    {
        auto Query = FCk_Handle_EntityTagQuery{};
        return TryGetQuery(InEntity, Query)
            && ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
    }

    auto Build_DisabledReason(const FCk_Handle& InEntity) -> FString
    {
        auto Query = FCk_Handle_EntityTagQuery{};
        if (NOT TryGetQuery(InEntity, Query)) { return TEXT("Entity Tag Query is unavailable."); }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
    }

    auto Build_Threshold(const FCk_EntityTagQuery_Requirement& InRequirement) -> FString
    {
        if (InRequirement.Get_Mode() == ECk_EntityTagQuery_CountMode::Count)
        { return FString::FromInt(InRequirement.Get_Count()); }
        return InRequirement.Get_Mode() == ECk_EntityTagQuery_CountMode::All ? TEXT("all") : TEXT("1");
    }

    auto Build_RequirementKey(const int32 InRequirementIndex, const FName InTag) -> FString
    { return FString::Printf(TEXT("%s%d|%s"), RequirementPrefix, InRequirementIndex, *InTag.ToString()); }

    auto Build_MatchKey(const int32 InRequirementIndex, const FName InTag, const FCk_Handle& InHandle) -> FString
    { return FString::Printf(TEXT("%s%d|%s|%s"), MatchPrefix, InRequirementIndex, *InTag.ToString(),
        *ck::Format_UE(TEXT("{}"), InHandle.Get_Entity())); }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto BoolField(const bool InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }
}

auto SCkInspector_EntityTagQueryAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _SelectionModel = InArgs._SelectionModel;
    _TagEditScope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _CountEditScope = MakeShared<FCkInspectorEditScope>(_EditGuard);

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_RowRecords() && Build_AuthoredView() && Populate_NativePorts())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_EntityTagQueryAuthored::~SCkInspector_EntityTagQueryAuthored()
{
    Release();
}

auto SCkInspector_EntityTagQueryAuthored::Get_IsAvailable() const -> bool
{
    auto Query = FCk_Handle_EntityTagQuery{};
    return _Active && ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query);
}

auto SCkInspector_EntityTagQueryAuthored::Get_CanRequest() const -> bool
{
    return _Active && ck_inspector_entity_tag_query::Get_CanRequest(_Entity);
}

auto SCkInspector_EntityTagQueryAuthored::Get_IsSatisfiedText() const -> FString
{
    auto Query = FCk_Handle_EntityTagQuery{};
    return _Active && ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)
        ? UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Query) ? TEXT("Yes") : TEXT("No")
        : TEXT("--");
}

auto SCkInspector_EntityTagQueryAuthored::Get_RequirementsLabel() const -> FString
{
    auto Query = FCk_Handle_EntityTagQuery{};
    const int32 Count = _Active && ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)
        ? UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query).Num() : 0;
    return FString::Printf(TEXT("Requirements (%d):"), Count);
}

auto SCkInspector_EntityTagQueryAuthored::Get_RequirementsText() const -> FString
{
    auto Query = FCk_Handle_EntityTagQuery{};
    return _Active && ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)
        ? FString::FromInt(UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query).Num())
        : TEXT("--");
}

auto SCkInspector_EntityTagQueryAuthored::Get_RequestDisabledReason() const -> FString
{
    return _Active ? ck_inspector_entity_tag_query::Build_DisabledReason(_Entity) : FString{};
}

auto SCkInspector_EntityTagQueryAuthored::Get_PendingTagText() const -> FString
{
    return _PendingTag.IsNone() ? FString{} : _PendingTag.ToString();
}

auto SCkInspector_EntityTagQueryAuthored::Get_PendingKindText() const -> FString
{
    static const TCHAR* Labels[] = {TEXT("Single"), TEXT("Of (count)"), TEXT("All")};
    return Labels[FMath::Clamp(_PendingKind, 0, 2)];
}

auto SCkInspector_EntityTagQueryAuthored::Commit_Tag(const FName InValue) -> void
{
    if (_Active) { _PendingTag = FName{*InValue.ToString().TrimStartAndEnd()}; }
}

auto SCkInspector_EntityTagQueryAuthored::Commit_Kind(const int32 InValue) -> void
{
    if (_Active) { _PendingKind = FMath::Clamp(InValue, 0, 2); }
}

auto SCkInspector_EntityTagQueryAuthored::Commit_Count(const int32 InValue) -> void
{
    if (_Active) { _PendingCount = FMath::Max(InValue, 1); }
}

auto SCkInspector_EntityTagQueryAuthored::Build_RowRecords() -> bool
{
    if (NOT _RowsCollection.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate(
            {
                {TEXT("requirement-visible"), ECkUiFieldKind::Bool},
                {TEXT("ensure-visible"), ECkUiFieldKind::Bool},
                {TEXT("match-visible"), ECkUiFieldKind::Bool},
                {TEXT("summary"), ECkUiFieldKind::Text},
                {TEXT("ensure"), ECkUiFieldKind::Text},
                {TEXT("match-label"), ECkUiFieldKind::Text},
                {TEXT("match-id"), ECkUiFieldKind::Text},
                {TEXT("match-name"), ECkUiFieldKind::Text},
                {TEXT("remove-label"), ECkUiFieldKind::Text},
                {TEXT("remove-tooltip"), ECkUiFieldKind::Text},
            },
            _RowsCollection);
        if (NOT CreateResult.Succeeded || NOT _RowsCollection.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    auto Records = TArray<FCkUiRecordData>{};
    auto RequirementsByKey = TMap<FString, FName>{};
    auto MatchesByKey = TMap<FString, FCk_Handle>{};
    auto Query = FCk_Handle_EntityTagQuery{};
    if (ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query))
    {
        const TArray<FCk_EntityTagQuery_Requirement> Requirements = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
        const TArray<FCk_EntityTagQuery_Result> Results = UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(Query);
        for (int32 Index = 0; Index < Requirements.Num(); ++Index)
        {
            const FCk_EntityTagQuery_Requirement& Requirement = Requirements[Index];
            const FName RequirementTag = Requirement.Get_Tag();
            const TArray<FCk_Handle> Matches = Index < Results.Num() ? Results[Index].Get_Handles() : TArray<FCk_Handle>{};

            auto RequirementRecord = FCkUiRecordData{};
            RequirementRecord.Key = ck_inspector_entity_tag_query::Build_RequirementKey(Index, RequirementTag);
            RequirementsByKey.Add(RequirementRecord.Key, RequirementTag);
            RequirementRecord.Fields.Add(TEXT("requirement-visible"), ck_inspector_entity_tag_query::BoolField(true));
            RequirementRecord.Fields.Add(TEXT("ensure-visible"), ck_inspector_entity_tag_query::BoolField(false));
            RequirementRecord.Fields.Add(TEXT("match-visible"), ck_inspector_entity_tag_query::BoolField(false));
            RequirementRecord.Fields.Add(TEXT("summary"), ck_inspector_entity_tag_query::TextField(FString::Printf(
                TEXT("%s [%s, %d/%s]"), *RequirementTag.ToString(), *UEnum::GetValueAsString(Requirement.Get_Mode()),
                Matches.Num(), *ck_inspector_entity_tag_query::Build_Threshold(Requirement))));
            RequirementRecord.Fields.Add(TEXT("ensure"), ck_inspector_entity_tag_query::TextField(FString{}));
            RequirementRecord.Fields.Add(TEXT("match-label"), ck_inspector_entity_tag_query::TextField(FString{}));
            RequirementRecord.Fields.Add(TEXT("match-id"), ck_inspector_entity_tag_query::TextField(FString{}));
            RequirementRecord.Fields.Add(TEXT("match-name"), ck_inspector_entity_tag_query::TextField(FString{}));
            RequirementRecord.Fields.Add(TEXT("remove-label"), ck_inspector_entity_tag_query::TextField(TEXT("Remove")));
            RequirementRecord.Fields.Add(TEXT("remove-tooltip"), ck_inspector_entity_tag_query::TextField(
                ck::Format_UE(TEXT("Request_RemoveRequirement({})"), RequirementTag.ToString())));
            Records.Add(MoveTemp(RequirementRecord));

            if (Requirement.Get_MaxAllowedEnsure() > FCk_EntityTagQuery_Requirement::NoEnsure)
            {
                auto EnsureRecord = FCkUiRecordData{};
                EnsureRecord.Key = FString::Printf(TEXT("ensure|%d|%s"), Index, *RequirementTag.ToString());
                EnsureRecord.Fields.Add(TEXT("requirement-visible"), ck_inspector_entity_tag_query::BoolField(false));
                EnsureRecord.Fields.Add(TEXT("ensure-visible"), ck_inspector_entity_tag_query::BoolField(true));
                EnsureRecord.Fields.Add(TEXT("match-visible"), ck_inspector_entity_tag_query::BoolField(false));
                EnsureRecord.Fields.Add(TEXT("summary"), ck_inspector_entity_tag_query::TextField(FString{}));
                EnsureRecord.Fields.Add(TEXT("ensure"), ck_inspector_entity_tag_query::TextField(
                    FString::FromInt(Requirement.Get_MaxAllowedEnsure())));
                EnsureRecord.Fields.Add(TEXT("match-label"), ck_inspector_entity_tag_query::TextField(FString{}));
                EnsureRecord.Fields.Add(TEXT("match-id"), ck_inspector_entity_tag_query::TextField(FString{}));
                EnsureRecord.Fields.Add(TEXT("match-name"), ck_inspector_entity_tag_query::TextField(FString{}));
                EnsureRecord.Fields.Add(TEXT("remove-label"), ck_inspector_entity_tag_query::TextField(FString{}));
                EnsureRecord.Fields.Add(TEXT("remove-tooltip"), ck_inspector_entity_tag_query::TextField(FString{}));
                Records.Add(MoveTemp(EnsureRecord));
            }

            for (int32 MatchIndex = 0; MatchIndex < Matches.Num(); ++MatchIndex)
            {
                const FCk_Handle& Match = Matches[MatchIndex];
                if (ck::Is_NOT_Valid(Match)) { continue; }
                auto MatchRecord = FCkUiRecordData{};
                MatchRecord.Key = ck_inspector_entity_tag_query::Build_MatchKey(Index, RequirementTag, Match);
                MatchesByKey.Add(MatchRecord.Key, Match);
                MatchRecord.Fields.Add(TEXT("requirement-visible"), ck_inspector_entity_tag_query::BoolField(false));
                MatchRecord.Fields.Add(TEXT("ensure-visible"), ck_inspector_entity_tag_query::BoolField(false));
                MatchRecord.Fields.Add(TEXT("match-visible"), ck_inspector_entity_tag_query::BoolField(true));
                MatchRecord.Fields.Add(TEXT("summary"), ck_inspector_entity_tag_query::TextField(FString{}));
                MatchRecord.Fields.Add(TEXT("ensure"), ck_inspector_entity_tag_query::TextField(FString{}));
                MatchRecord.Fields.Add(TEXT("match-label"), ck_inspector_entity_tag_query::TextField(
                    MatchIndex == 0 ? TEXT("Matches:") : FString{}));
                MatchRecord.Fields.Add(TEXT("match-id"), ck_inspector_entity_tag_query::TextField(
                    ck::Format_UE(TEXT("{}"), Match.Get_Entity())));
                MatchRecord.Fields.Add(TEXT("match-name"), ck_inspector_entity_tag_query::TextField(
                    UCk_Utils_Handle_UE::Get_DebugName(Match).ToString()));
                MatchRecord.Fields.Add(TEXT("remove-label"), ck_inspector_entity_tag_query::TextField(FString{}));
                MatchRecord.Fields.Add(TEXT("remove-tooltip"), ck_inspector_entity_tag_query::TextField(FString{}));
                Records.Add(MoveTemp(MatchRecord));
            }
        }
    }

    const FCkUiLoadResult RecordsResult = _RowsCollection->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    _RequirementsByKey = MoveTemp(RequirementsByKey);
    _MatchesByKey = MoveTemp(MatchesByKey);
    return true;
}

auto SCkInspector_EntityTagQueryAuthored::Build_AuthoredView() -> bool
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

    const TSharedRef<SBox> TagPort = SNew(SBox);
    const TSharedRef<SBox> KindPort = SNew(SBox);
    const TSharedRef<SBox> CountPort = SNew(SBox);
    _NativePorts = {TagPort, KindPort, CountPort};
    auto NativeBindings = FCkUiView::FNativeBindings{};
    NativeBindings.Add(TEXT("entity-tag-query-tag-port"), TagPort);
    NativeBindings.Add(TEXT("entity-tag-query-kind-port"), KindPort);
    NativeBindings.Add(TEXT("entity-tag-query-count-port"), CountPort);

    const TWeakPtr<SCkInspector_EntityTagQueryAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Collections.Add(TEXT("entity-tag-query-rows"), _RowsCollection);
    Data.Text.Add(TEXT("entity-tag-query-satisfied"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_IsSatisfiedText()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("entity-tag-query-requirements-label"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_RequirementsLabel()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("entity-tag-query-requirements"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_RequirementsText()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("entity-tag-query-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("entity-tag-query-add-label"), FText::FromString(TEXT("Add")));
    Data.Text.Add(TEXT("entity-tag-query-add-tooltip"), FText::FromString(
        TEXT("Request_AddRequirement with the staged tag, kind, and count.")));
    Data.Color.Add(TEXT("entity-tag-query-status-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        if (NOT Widget.IsValid() || NOT Widget->Get_IsAvailable()) { return CkStyle::GetToneColor(ECk_Tone::Neutral); }
        return CkStyle::GetToneColor(Widget->Get_IsSatisfiedText() == TEXT("Yes") ? ECk_Tone::Ok : ECk_Tone::Err);
    }));
    Data.Color.Add(TEXT("entity-tag-query-status-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        if (NOT Widget.IsValid() || NOT Widget->Get_IsAvailable()) { return CkStyle::GetToneDimColor(ECk_Tone::Neutral); }
        return CkStyle::GetToneDimColor(Widget->Get_IsSatisfiedText() == TEXT("Yes") ? ECk_Tone::Ok : ECk_Tone::Err);
    }));
    Data.Visibility.Add(TEXT("entity-tag-query-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("entity-tag-query-request-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.ItemActions.Add(TEXT("entity-tag-query-remove"), FCkUiOnItemAction::CreateLambda(
        [WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert())
            { Widget->Request_Remove(InKey); }
        }));
    Data.ItemActions.Add(TEXT("entity-tag-query-navigate"), FCkUiOnItemAction::CreateLambda(
        [WeakWidget](const FString& InKey)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert())
            { Widget->Navigate_Match(InKey); }
        }));

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("entity-tag-query-add"), FSimpleDelegate::CreateLambda([WeakWidget]()
    {
        if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert())
        { Widget->Request_Add(); }
    }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTagQuery.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTagQuery.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_EntityTagQueryAuthored::Present_EditControl(
    const TSharedRef<SWidget> InInput,
    const TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>
{
    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    { return SNew(STextBlock).Text(InReadOnlyText); }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection)) { return InInput; }

    const TSharedRef<SBox> HoverHost = SNew(SBox);
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    { const auto Host = WeakHoverHost.Pin(); return Host.IsValid() && Host->IsHovered(); };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock).Text(InReadOnlyText);
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    InInput->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay) + SOverlay::Slot()[ReadOnly] + SOverlay::Slot()[InInput]);
    return HoverHost;
}

auto SCkInspector_EntityTagQueryAuthored::Build_TagValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_EntityTagQueryAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SEditableTextBox> Entry = SNew(SEditableTextBox)
        .Tag(TEXT("entity-tag-query-tag-input"))
        .Text_Lambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_PendingTagText()) : FText::GetEmpty();
        })
        .OnTextChanged_Lambda([WeakWidget](const FText& InText)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Is_Inert())
            {
                Widget->_PendingTag = FName{*InText.ToString()};
                if (Widget->_TagEditScope.IsValid()) { Widget->_TagEditScope->Set_Active(true); }
            }
        })
        .OnTextCommitted_Lambda([WeakWidget](const FText& InText, ETextCommit::Type)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            {
                Widget->Commit_Tag(FName{*InText.ToString()});
                if (Widget->_TagEditScope.IsValid()) { Widget->_TagEditScope->Set_Active(false); }
            }
        });
    Entry->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Entry->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
    }));
    return Present_EditControl(Entry, TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_PendingTagText()) : FText::GetEmpty();
    }));
}

auto SCkInspector_EntityTagQueryAuthored::Build_KindValue() -> TSharedRef<SWidget>
{
    _KindOptions = {MakeShared<FString>(TEXT("Single")), MakeShared<FString>(TEXT("Of (count)")), MakeShared<FString>(TEXT("All"))};
    const TWeakPtr<SCkInspector_EntityTagQueryAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Combo = SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_KindOptions)
        .OnGenerateWidget_Lambda([](const TSharedPtr<FString> InItem)
        { return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : FString{})); })
        .OnSelectionChanged_Lambda([WeakWidget](const TSharedPtr<FString> InItem, const ESelectInfo::Type InSelectInfo)
        {
            if (InSelectInfo == ESelectInfo::Direct || NOT InItem.IsValid()) { return; }
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Commit_Kind(Widget->_KindOptions.IndexOfByKey(InItem)); }
        })
        [SNew(STextBlock).Text_Lambda([WeakWidget]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_PendingKindText()) : FText::GetEmpty();
        })];
    const TSharedRef<SBox> Input = SNew(SBox)
        .Tag(TEXT("entity-tag-query-kind-input"))
        .IsEnabled_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })
        [Combo];
    return Present_EditControl(Input, TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_PendingKindText()) : FText::GetEmpty();
    }));
}

auto SCkInspector_EntityTagQueryAuthored::Build_CountValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_EntityTagQueryAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
        .Tag(TEXT("entity-tag-query-count-input"))
        .Value_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_PendingCount() : 1; })
        .Kind(ECkDebug_NumericKind::Integer)
        .MinValue(1.0)
        .Width(88.0f)
        .OnValueCommitted_Lambda([WeakWidget](const double InValue)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Count(FMath::RoundToInt(InValue)); } })
        .OnEditStateChanged_Lambda([WeakWidget](const bool bEditing)
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && Widget->_CountEditScope.IsValid())
            { Widget->_CountEditScope->Set_Active(bEditing); }
        });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
    }));
    return Present_EditControl(Editor, TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::AsNumber(Widget->Get_PendingCount()) : FText::GetEmpty();
    }));
}

auto SCkInspector_EntityTagQueryAuthored::Populate_NativePorts() -> bool
{
    if (NOT _Active || _NativePorts.Num() != 3)
    {
        _LoadError = TEXT("Entity Tag Query authored native ports are unavailable.");
        return false;
    }
    _NativePorts[0]->SetContent(Build_TagValue());
    _NativePorts[1]->SetContent(Build_KindValue());
    _NativePorts[2]->SetContent(Build_CountValue());
    return true;
}

auto SCkInspector_EntityTagQueryAuthored::Request_Add() -> void
{
    if (NOT Get_CanRequest() || _PendingTag.IsNone()) { return; }
    auto Query = FCk_Handle_EntityTagQuery{};
    if (NOT ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)) { return; }
    const FCk_EntityTagQuery_Requirement Requirement = [this]()
    {
        if (_PendingKind == 1) { return UCk_Utils_EntityTagQuery_UE::Make_Requirement_Of(_PendingTag, _PendingCount); }
        if (_PendingKind == 2) { return UCk_Utils_EntityTagQuery_UE::Make_Requirement_All(_PendingTag); }
        return UCk_Utils_EntityTagQuery_UE::Make_Requirement_Single(_PendingTag);
    }();
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(
        Query, FCk_Request_EntityTagQuery_AddRequirement{Requirement}, {});
}

auto SCkInspector_EntityTagQueryAuthored::Request_Remove(const FString& InStableKey) -> void
{
    if (NOT Get_CanRequest()) { return; }
    const FName* RequirementTag = _RequirementsByKey.Find(InStableKey);
    auto Query = FCk_Handle_EntityTagQuery{};
    if (RequirementTag == nullptr || RequirementTag->IsNone()
        || NOT ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)) { return; }
    const TArray<FCk_EntityTagQuery_Requirement> Requirements = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
    bool IsCurrentKey = false;
    for (int32 Index = 0; Index < Requirements.Num(); ++Index)
    {
        if (ck_inspector_entity_tag_query::Build_RequirementKey(Index, Requirements[Index].Get_Tag()) == InStableKey)
        { IsCurrentKey = true; break; }
    }
    if (NOT IsCurrentKey) { return; }
    UCk_Utils_EntityTagQuery_UE::Request_RemoveRequirement(
        Query, FCk_Request_EntityTagQuery_RemoveRequirement{*RequirementTag}, {});
}

auto SCkInspector_EntityTagQueryAuthored::Navigate_Match(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable() || NOT InStableKey.StartsWith(ck_inspector_entity_tag_query::MatchPrefix)) { return; }
    const FCk_Handle* RoutedMatch = _MatchesByKey.Find(InStableKey);
    if (RoutedMatch == nullptr || ck::Is_NOT_Valid(*RoutedMatch)) { return; }
    auto Query = FCk_Handle_EntityTagQuery{};
    if (NOT ck_inspector_entity_tag_query::TryGetQuery(_Entity, Query)) { return; }
    const TArray<FCk_EntityTagQuery_Requirement> Requirements = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
    const TArray<FCk_EntityTagQuery_Result> Results = UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(Query);
    for (int32 Index = 0; Index < Requirements.Num() && Index < Results.Num(); ++Index)
    {
        for (const FCk_Handle& Match : Results[Index].Get_Handles())
        {
            if (Match != *RoutedMatch
                || ck_inspector_entity_tag_query::Build_MatchKey(Index, Requirements[Index].Get_Tag(), Match) != InStableKey)
            { continue; }
            if (_SelectionModel.IsValid()) { _SelectionModel->Set_SelectedEntities({Match}); }
            else { ck::DebugNav::Goto_Entity(Match); }
            return;
        }
    }
}

auto SCkInspector_EntityTagQueryAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT Build_RowRecords()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_EntityTagQueryAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_EntityTagQueryAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    if (_TagEditScope.IsValid()) { _TagEditScope->Set_Active(false); }
    if (_CountEditScope.IsValid()) { _CountEditScope->Set_Active(false); }
    Detach_NativePorts();
    _Entity = {};
    _EditGuard.Reset();
    _TagEditScope.Reset();
    _CountEditScope.Reset();
    _SelectionModel.Reset();
    _RowsCollection.Reset();
    _View.Reset();
    _RequirementsByKey.Reset();
    _MatchesByKey.Reset();
    _KindOptions.Reset();
    _Mounted = false;
}

FCkInspector_EntityTagQuery::~FCkInspector_EntityTagQuery()
{
    OnDeactivated();
}

auto FCkInspector_EntityTagQuery::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Tag Query"));
}

auto FCkInspector_EntityTagQuery::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Query = FCk_Handle_EntityTagQuery{};
    return ck_inspector_entity_tag_query::TryGetQuery(Entity, Query);
}

auto FCkInspector_EntityTagQuery::Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    auto Query = FCk_Handle_EntityTagQuery{};
    if (NOT ck_inspector_entity_tag_query::TryGetQuery(InEntity, Query)) { return Builder.Build(InEntity); }

    const FCk_Handle CapturedEntity = InEntity;
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Is Satisfied:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            auto Current = FCk_Handle_EntityTagQuery{};
            if (NOT ck_inspector_entity_tag_query::TryGetQuery(CapturedEntity, Current)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Current) ? TEXT("Yes") : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            auto Current = FCk_Handle_EntityTagQuery{};
            if (NOT ck_inspector_entity_tag_query::TryGetQuery(CapturedEntity, Current)) { return ECk_Tone::Neutral; }
            return UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(Current) ? ECk_Tone::Ok : ECk_Tone::Err;
        }));

    const TArray<FCk_EntityTagQuery_Requirement> Requirements = UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(Query);
    const TArray<FCk_EntityTagQuery_Result> Results = UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(Query);
    Builder.AddRow(
        FText::FromString(FString::Printf(TEXT("Requirements (%d):"), Requirements.Num())),
        [Count = Requirements.Num()](const FCk_Handle&) { return FText::AsNumber(Count); },
        CkStyle::Value_Numeric());

    for (int32 Index = 0; Index < Requirements.Num(); ++Index)
    {
        const FCk_EntityTagQuery_Requirement& Requirement = Requirements[Index];
        const FName Tag = Requirement.Get_Tag();
        const TArray<FCk_Handle> Matches = Index < Results.Num() ? Results[Index].Get_Handles() : TArray<FCk_Handle>{};
        Builder.AddRow(
            FText::FromString(FString::Printf(TEXT("  %s [%s, %d/%s]"), *Tag.ToString(),
                *UEnum::GetValueAsString(Requirement.Get_Mode()), Matches.Num(),
                *ck_inspector_entity_tag_query::Build_Threshold(Requirement))),
            [](const FCk_Handle&) { return FText::GetEmpty(); },
            CkStyle::TextDim());
        Builder.AddActionRow(
            FText::FromString(TEXT("    ")),
            {FCkInspector_Action{
                FText::FromString(TEXT("Remove")),
                FText::FromString(ck::Format_UE(TEXT("Request_RemoveRequirement({})"), Tag.ToString())),
                [CapturedEntity, Tag]()
                {
                    auto Current = FCk_Handle_EntityTagQuery{};
                    if (ck_inspector_entity_tag_query::TryGetQuery(CapturedEntity, Current))
                    { UCk_Utils_EntityTagQuery_UE::Request_RemoveRequirement(
                        Current, FCk_Request_EntityTagQuery_RemoveRequirement{Tag}, {}); }
                }}});
        if (Requirement.Get_MaxAllowedEnsure() > FCk_EntityTagQuery_Requirement::NoEnsure)
        {
            Builder.AddRow(
                FText::FromString(TEXT("    Ensure <=:")),
                [Maximum = Requirement.Get_MaxAllowedEnsure()](const FCk_Handle&) { return FText::AsNumber(Maximum); },
                CkStyle::Value_Numeric());
        }
        if (NOT Matches.IsEmpty())
        { Builder.AddWidgetRow(FText::FromString(TEXT("    Matches:")), FCkInspectorWidgetBuilder::MakeBadgeBox(Matches)); }
    }

    Builder.AddHeader(FText::FromString(TEXT("Add Requirement")));
    const auto PendingTag = MakeShared<FName>(NAME_None);
    const auto PendingKind = MakeShared<int32>(0);
    const auto PendingCount = MakeShared<int32>(1);
    Builder.AddNameEntryRow(
        FText::FromString(TEXT("Tag:")),
        TAttribute<FText>::CreateLambda([PendingTag]() { return FText::FromName(*PendingTag); }),
        [PendingTag](const FName InValue) { *PendingTag = InValue; });
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Kind:")),
        {FText::FromString(TEXT("Single")), FText::FromString(TEXT("Of (count)")), FText::FromString(TEXT("All"))},
        TAttribute<int32>::CreateLambda([PendingKind]() { return *PendingKind; }),
        [PendingKind](const int32 InValue) { *PendingKind = InValue; });
    Builder.AddIntegerRow(
        FText::FromString(TEXT("Count (Of):")),
        TAttribute<int32>::CreateLambda([PendingCount]() { return *PendingCount; }),
        [PendingCount](const int32 InValue) { *PendingCount = InValue; },
        1);
    Builder.AddActionRow(
        FText::FromString(TEXT(" ")),
        {FCkInspector_Action{
            FText::FromString(TEXT("Add")),
            FText::FromString(TEXT("Request_AddRequirement with the staged tag, kind, and count.")),
            [CapturedEntity, PendingTag, PendingKind, PendingCount]()
            {
                auto Current = FCk_Handle_EntityTagQuery{};
                if (PendingTag->IsNone() || NOT ck_inspector_entity_tag_query::TryGetQuery(CapturedEntity, Current)) { return; }
                const FCk_EntityTagQuery_Requirement Requirement = *PendingKind == 1
                    ? UCk_Utils_EntityTagQuery_UE::Make_Requirement_Of(*PendingTag, *PendingCount)
                    : *PendingKind == 2
                        ? UCk_Utils_EntityTagQuery_UE::Make_Requirement_All(*PendingTag)
                        : UCk_Utils_EntityTagQuery_UE::Make_Requirement_Single(*PendingTag);
                UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(
                    Current, FCk_Request_EntityTagQuery_AddRequirement{Requirement}, {});
            }}});
    return Builder.Build(InEntity);
}

auto FCkInspector_EntityTagQuery::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString{});
}

auto FCkInspector_EntityTagQuery::Build_Inspector(const FCk_Handle& Entity, const FString&) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_EntityTagQueryAuthored> Authored = SNew(SCkInspector_EntityTagQueryAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .SelectionModel(Get_SelectionModel());
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_EntityTagQuery::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_EntityTagQueryAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_EntityTagQuery::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_EntityTagQueryAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_EntityTagQueryAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
