#include "CkInspector_EntityCollections.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityCollections)

namespace ck_inspector_entity_collections
{
    auto TryGetCollections(const FCk_Handle& InEntity) -> TArray<FCk_Handle_EntityCollection>
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_EntityCollection_UE::Has_Any(InEntity)) { return {}; }

        auto Collections = TArray<FCk_Handle_EntityCollection>{};
        auto MutableEntity = InEntity;
        UCk_Utils_EntityCollection_UE::ForEach_EntityCollection(MutableEntity,
            [&Collections](const FCk_Handle_EntityCollection InCollection)
            {
                if (ck::IsValid(InCollection) && UCk_Utils_EntityCollection_UE::Has(FCk_Handle{InCollection}))
                { Collections.Add(InCollection); }
            });
        return Collections;
    }

    auto GetCollectionName(const FCk_Handle_EntityCollection& InCollection) -> FString
    {
        if (ck::Is_NOT_Valid(InCollection)) { return TEXT("Unnamed"); }
        const FGameplayTag Label = UCk_Utils_GameplayLabel_UE::Get_Label(InCollection);
        return Label.IsValid() ? Label.ToString() : TEXT("Unnamed");
    }

    auto GetHandleIdentityKey(const FCk_Handle& InHandle) -> FString
    {
        if (ck::Is_NOT_Valid(InHandle)) { return {}; }
        const FCk_Entity& Entity = InHandle.Get_Entity();
        const FCk_RegistryHandle Registry = InHandle.Get_RegistryView().Get_RegistryHandle();
        return FString::Printf(TEXT("registry|%d|%d/entity|%u|%u"),
            Registry.SlotIndex,
            Registry.Generation,
            static_cast<uint32>(Entity.Get_EntityNumber()),
            static_cast<uint32>(Entity.Get_VersionNumber()));
    }

    auto GetCollectionKey(const FCk_Handle_EntityCollection& InCollection) -> FString
    {
        const FString Identity = GetHandleIdentityKey(FCk_Handle{InCollection});
        return Identity.IsEmpty() ? FString{} : TEXT("collection/") + Identity;
    }

    auto GetMemberLabel(const FCk_Handle& InMember) -> FString
    {
        return ck::IsValid(InMember) ? ck::Format_UE(TEXT("  {}"), InMember.ToString()) : FString{};
    }

    auto GetMemberKey(const FCk_Handle_EntityCollection& InCollection, const FCk_Handle& InMember) -> FString
    {
        const FString CollectionKey = GetCollectionKey(InCollection);
        const FString MemberIdentity = GetHandleIdentityKey(InMember);
        return NOT CollectionKey.IsEmpty() && NOT MemberIdentity.IsEmpty()
            ? FString::Printf(TEXT("%s/member/%s"), *CollectionKey, *MemberIdentity)
            : FString{};
    }

    auto FindCollectionByKey(const FCk_Handle& InEntity, const FString& InStableKey,
        FCk_Handle_EntityCollection& OutCollection) -> bool
    {
        if (InStableKey.IsEmpty()) { return false; }
        for (const FCk_Handle_EntityCollection& Collection : TryGetCollections(InEntity))
        {
            if (GetCollectionKey(Collection) == InStableKey)
            {
                OutCollection = Collection;
                return true;
            }
        }
        return false;
    }

    auto FindMemberByKey(const FCk_Handle& InEntity, const FString& InStableKey,
        FCk_Handle_EntityCollection& OutCollection, FCk_Handle& OutMember) -> bool
    {
        if (InStableKey.IsEmpty()) { return false; }
        for (const FCk_Handle_EntityCollection& Collection : TryGetCollections(InEntity))
        {
            const FCk_EntityCollection_Content Content = UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(Collection);
            for (const FCk_Handle& Member : Content.Get_EntitiesInCollection())
            {
                if (ck::IsValid(Member) && GetMemberKey(Collection, Member) == InStableKey)
                {
                    OutCollection = Collection;
                    OutMember = Member;
                    return true;
                }
            }
        }
        return false;
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkInspector_EntityCollectionsAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _SelectionModel = InArgs._SelectionModel;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_CollectionRecords() && Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }

    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_EntityCollectionsAuthored::~SCkInspector_EntityCollectionsAuthored()
{
    Release();
}

auto SCkInspector_EntityCollectionsAuthored::Get_IsAvailable() const -> bool
{
    return _Active && NOT ck_inspector_entity_collections::TryGetCollections(_Entity).IsEmpty();
}

auto SCkInspector_EntityCollectionsAuthored::Get_CollectionCount() const -> int32
{
    return _Active ? ck_inspector_entity_collections::TryGetCollections(_Entity).Num() : 0;
}

auto SCkInspector_EntityCollectionsAuthored::Build_CollectionRecords() -> bool
{
    if (NOT _CollectionsCollection.IsValid())
    {
        const FCkUiCollectionSchema Schema{
            .Fields = {
                {TEXT("name"), ECkUiFieldKind::Text},
                {TEXT("count-text"), ECkUiFieldKind::Text},
                {TEXT("diff-color"), ECkUiFieldKind::Color},
                {TEXT("collection-visible"), ECkUiFieldKind::Bool},
                {TEXT("record-visible"), ECkUiFieldKind::Bool},
                {TEXT("select-tooltip"), ECkUiFieldKind::Text},
            },
            .Children = {{TEXT("members"), {
                {TEXT("label"), ECkUiFieldKind::Text},
                {TEXT("diff-color"), ECkUiFieldKind::Color},
                {TEXT("remove-label"), ECkUiFieldKind::Text},
                {TEXT("remove-visible"), ECkUiFieldKind::Bool},
                {TEXT("remove-tooltip"), ECkUiFieldKind::Text},
            }}}
        };
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreateHierarchical(Schema, _CollectionsCollection);
        if (NOT CreateResult.Succeeded || NOT _CollectionsCollection.IsValid())
        {
            _LoadError = FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    const bool bShowEditControls = ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection());
    const bool bCaptureDiffMarks = NOT _DiffMarksCaptured;
    auto RetainedCollectionDiffs = TSet<FString>{};
    auto RetainedMemberDiffs = TSet<FString>{};
    auto Records = TArray<FCkUiRecordData>{};
    for (const FCk_Handle_EntityCollection& Collection : ck_inspector_entity_collections::TryGetCollections(_Entity))
    {
        const FString CollectionKey = ck_inspector_entity_collections::GetCollectionKey(Collection);
        if (CollectionKey.IsEmpty()) { continue; }

        const FString Name = ck_inspector_entity_collections::GetCollectionName(Collection);
        const int32 Count = UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(Collection);
        const FString CountText = FString::Printf(TEXT("%d entities"), Count);
        const bool bCollectionDiff = bCaptureDiffMarks
            ? FCkInspector_DiffMarkScope::Is_LabelMarked(Name)
            : _CollectionDiffMarkedKeys.Contains(CollectionKey);
        if (bCollectionDiff) { RetainedCollectionDiffs.Add(CollectionKey); }
        const bool bCollectionVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, Name, CountText);

        auto Members = TArray<FCkUiRecordData>{};
        auto bAnyMemberVisible = false;
        const FCk_EntityCollection_Content Content = UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(Collection);
        for (const FCk_Handle& Member : Content.Get_EntitiesInCollection())
        {
            if (ck::Is_NOT_Valid(Member)) { continue; }
            const FString MemberKey = ck_inspector_entity_collections::GetMemberKey(Collection, Member);
            const FString MemberLabel = ck_inspector_entity_collections::GetMemberLabel(Member);
            if (MemberKey.IsEmpty() || MemberLabel.IsEmpty()) { continue; }

            const bool bMemberDiff = bCaptureDiffMarks
                ? FCkInspector_DiffMarkScope::Is_LabelMarked(MemberLabel)
                : _MemberDiffMarkedKeys.Contains(MemberKey);
            if (bMemberDiff) { RetainedMemberDiffs.Add(MemberKey); }
            const bool bRemoveVisible = bShowEditControls
                && FCkInspectorWidgetBuilder::Matches_Filter(_Filter, MemberLabel, TEXT("Remove"));
            bAnyMemberVisible |= bRemoveVisible;

            auto MemberRecord = FCkUiRecordData{};
            MemberRecord.Key = MemberKey;
            MemberRecord.Fields.Add(TEXT("label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(MemberLabel)});
            MemberRecord.Fields.Add(TEXT("diff-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
                .Color = ck_inspector_entity_collections::DiffColor(bMemberDiff)});
            MemberRecord.Fields.Add(TEXT("remove-label"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Remove"))});
            MemberRecord.Fields.Add(TEXT("remove-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bRemoveVisible});
            MemberRecord.Fields.Add(TEXT("remove-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
                .Text = FText::FromString(TEXT("Request_RemoveEntities with just this entity — the collection itself and the entity both survive."))});
            Members.Add(MoveTemp(MemberRecord));
        }

        auto Record = FCkUiRecordData{};
        Record.Key = CollectionKey;
        Record.Fields.Add(TEXT("name"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Name)});
        Record.Fields.Add(TEXT("count-text"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(CountText)});
        Record.Fields.Add(TEXT("diff-color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = ck_inspector_entity_collections::DiffColor(bCollectionDiff)});
        Record.Fields.Add(TEXT("collection-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bCollectionVisible});
        Record.Fields.Add(TEXT("record-visible"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool,
            .Bool = bCollectionVisible || bAnyMemberVisible});
        Record.Fields.Add(TEXT("select-tooltip"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(TEXT("Select collection in the ECS inspector"))});
        Record.Children.Add(TEXT("members"), MoveTemp(Members));
        Records.Add(MoveTemp(Record));
    }

    const FCkUiLoadResult RecordsResult = _CollectionsCollection->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    _CollectionDiffMarkedKeys = MoveTemp(RetainedCollectionDiffs);
    _MemberDiffMarkedKeys = MoveTemp(RetainedMemberDiffs);
    _DiffMarksCaptured = true;
    return true;
}

auto SCkInspector_EntityCollectionsAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_EntityCollectionsAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("entity-collections-available"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_EntityCollectionsAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Text.Add(TEXT("entity-collections-disabled-reason"), FText::FromString(TEXT("Entity collection membership is unavailable.")));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_EntityCollectionsAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.Collections.Add(TEXT("entity-collections"), _CollectionsCollection);
    Data.ItemActions.Add(TEXT("entity-collections-select"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InStableKey)
    {
        if (const TSharedPtr<SCkInspector_EntityCollectionsAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Request_SelectCollection(InStableKey); }
    }));
    Data.ItemActions.Add(TEXT("entity-collections-remove-member"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& InStableKey)
    {
        if (const TSharedPtr<SCkInspector_EntityCollectionsAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Request_RemoveMember(InStableKey); }
    }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityCollections.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityCollections.ui.css")));
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

auto SCkInspector_EntityCollectionsAuthored::Request_SelectCollection(const FString& InStableKey) -> void
{
    auto Collection = FCk_Handle_EntityCollection{};
    if (NOT _Active || NOT ck_inspector_entity_collections::FindCollectionByKey(_Entity, InStableKey, Collection)) { return; }
    if (const TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel = _SelectionModel.Pin(); SelectionModel.IsValid())
    { SelectionModel->Set_SelectedEntities({FCk_Handle{Collection}}); }
}

auto SCkInspector_EntityCollectionsAuthored::Request_RemoveMember(const FString& InStableKey) -> void
{
    auto Collection = FCk_Handle_EntityCollection{};
    auto Member = FCk_Handle{};
    if (NOT _Active || NOT ck_inspector_entity_collections::FindMemberByKey(_Entity, InStableKey, Collection, Member)) { return; }

    UCk_Utils_EntityCollection_UE::Request_RemoveEntities(
        Collection, FCk_Request_EntityCollection_RemoveEntities{TArray<FCk_Handle>{Member}}, {});
}

auto SCkInspector_EntityCollectionsAuthored::Tick(
    const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }

    if (NOT Build_CollectionRecords()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_EntityCollectionsAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = FCk_Handle{};
    _SelectionModel.Reset();
    _CollectionsCollection.Reset();
    _CollectionDiffMarkedKeys.Reset();
    _MemberDiffMarkedKeys.Reset();
    _View.Reset();
    _Mounted = false;
}

// --------------------------------------------------------------------------------------------------------------------

FCkInspector_EntityCollections::~FCkInspector_EntityCollections()
{
    OnDeactivated();
}

auto FCkInspector_EntityCollections::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Collections"));
}

auto FCkInspector_EntityCollections::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return NOT ck_inspector_entity_collections::TryGetCollections(Entity).IsEmpty();
}

auto FCkInspector_EntityCollections::Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    const TWeakPtr<FCkDebuggerModel_EntitySelection> WeakSelectionModel = SelectionModel;

    for (const FCk_Handle_EntityCollection& Collection : ck_inspector_entity_collections::TryGetCollections(InEntity))
    {
        const FString CollectionName = ck_inspector_entity_collections::GetCollectionName(Collection);
        const FString CollectionKey = ck_inspector_entity_collections::GetCollectionKey(Collection);
        const FCk_Handle CapturedEntity = InEntity;

        Builder.AddCountBadgeRow(
            FText::FromString(CollectionName),
            TAttribute<int32>::CreateLambda([CapturedEntity, CollectionKey]()
            {
                auto Current = FCk_Handle_EntityCollection{};
                return ck_inspector_entity_collections::FindCollectionByKey(CapturedEntity, CollectionKey, Current)
                    ? UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(Current) : 0;
            }),
            ECk_Tone::Info,
            FText::FromString(TEXT("entities")),
            [WeakSelectionModel, CapturedEntity, CollectionKey]()
            {
                auto Current = FCk_Handle_EntityCollection{};
                const TSharedPtr<FCkDebuggerModel_EntitySelection> CurrentSelectionModel = WeakSelectionModel.Pin();
                if (CurrentSelectionModel.IsValid()
                    && ck_inspector_entity_collections::FindCollectionByKey(CapturedEntity, CollectionKey, Current))
                { CurrentSelectionModel->Set_SelectedEntities({FCk_Handle{Current}}); }
            });

        const FCk_EntityCollection_Content Content = UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(Collection);
        for (const FCk_Handle& Member : Content.Get_EntitiesInCollection())
        {
            if (ck::Is_NOT_Valid(Member)) { continue; }
            const FString MemberLabel = ck_inspector_entity_collections::GetMemberLabel(Member);
            const FString MemberKey = ck_inspector_entity_collections::GetMemberKey(Collection, Member);
            Builder.AddActionRow(
                FText::FromString(MemberLabel),
                {FCkInspector_Action{
                    FText::FromString(TEXT("Remove")),
                    FText::FromString(TEXT("Request_RemoveEntities with just this entity — the collection itself and the entity both survive.")),
                    [CapturedEntity, MemberKey]()
                    {
                        auto CurrentCollection = FCk_Handle_EntityCollection{};
                        auto CurrentMember = FCk_Handle{};
                        if (NOT ck_inspector_entity_collections::FindMemberByKey(
                            CapturedEntity, MemberKey, CurrentCollection, CurrentMember)) { return; }
                        UCk_Utils_EntityCollection_UE::Request_RemoveEntities(CurrentCollection,
                            FCk_Request_EntityCollection_RemoveEntities{TArray<FCk_Handle>{CurrentMember}}, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk}});
        }
    }

    return Builder.Build(InEntity, InFilter);
}

auto FCkInspector_EntityCollections::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString{});
}

auto FCkInspector_EntityCollections::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_EntityCollectionsAuthored> Authored = SNew(SCkInspector_EntityCollectionsAuthored)
        .Entity(Entity)
        .Filter(InFilter)
        .SelectionModel(SelectionModel);
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_EntityCollections::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_EntityCollectionsAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_EntityCollections::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_EntityCollectionsAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_EntityCollectionsAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
