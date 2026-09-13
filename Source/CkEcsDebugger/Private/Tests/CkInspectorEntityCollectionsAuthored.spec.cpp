#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_EntityCollections.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEntityCollection/CkEntityCollection_Processor.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiStyledButton.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_entity_collections_authored_test
{
    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString().Contains(InText)) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString().Contains(InText)) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; } }
        return false;
    }

    auto FindTaggedButtonForText(
        const TSharedRef<SWidget>& InRoot, const FString& InText, const FName InTag) -> TSharedPtr<SButton>
    {
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButtonForText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText, InTag); Found.IsValid())
            { return Found; }
        }

        if (NOT ContainsText(InRoot, InText)) { return nullptr; }
        TFunction<TSharedPtr<SButton>(const TSharedRef<SWidget>&)> FindTagged;
        FindTagged = [&FindTagged, InTag](const TSharedRef<SWidget>& InWidget) -> TSharedPtr<SButton>
        {
            const FString Type = InWidget->GetTypeAsString();
            if ((Type == TEXT("SButton") || Type == TEXT("SCkUiStyledButton")) && InWidget->GetTag() == InTag)
            { return StaticCastSharedRef<SButton>(InWidget); }
            FChildren* WidgetChildren = InWidget->GetChildren();
            for (int32 ChildIndex = 0; WidgetChildren != nullptr && ChildIndex < WidgetChildren->Num(); ++ChildIndex)
            {
                if (const TSharedPtr<SButton> Found = FindTagged(
                    ConstCastSharedRef<SWidget>(WidgetChildren->GetChildAt(ChildIndex))); Found.IsValid())
                { return Found; }
            }
            return nullptr;
        };
        return FindTagged(InRoot);
    }

    auto AddAuthorityAndWorld(FCk_Handle& InEntity) -> bool
    {
        if (GWorld == nullptr) { return false; }
        InEntity.Add<TWeakObjectPtr<UWorld>>(GWorld);
        UCk_Utils_Net_UE::Add(InEntity, FCk_Net_ConnectionSettings{
            ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});
        return true;
    }

    auto FindRecordKeyByText(const FCkUiCollection& InCollection, const FString& InField, const FString& InText) -> FString
    {
        for (const TSharedPtr<const FCkUiRecord>& Record : InCollection.GetRecords())
        {
            const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
            if (Field != nullptr && Field->Kind == ECkUiFieldKind::Text && Field->Text.ToString() == InText)
            { return Record->GetKey(); }
        }
        return {};
    }

    auto GetRecordBool(const FCkUiCollection& InCollection, const FString& InKey, const FString& InField) -> bool
    {
        const TSharedPtr<const FCkUiRecord> Record = InCollection.FindRecord(InKey);
        const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Bool && Field->Bool;
    }

    auto GetRecordText(const FCkUiCollection& InCollection, const FString& InKey, const FString& InField) -> FString
    {
        const TSharedPtr<const FCkUiRecord> Record = InCollection.FindRecord(InKey);
        const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Text ? Field->Text.ToString() : FString{};
    }

    struct FFixture final
    {
        FCk_Handle OwnerA;
        FCk_Handle OwnerB;
        FCk_Handle_EntityCollection CollectionA;
        FCk_Handle_EntityCollection CollectionB;
        FCk_Handle MemberA1;
        FCk_Handle MemberA2;
        FCk_Handle MemberB;
    };

    auto CreateFixture(ck::FEcsWorld& InWorld, FFixture& OutFixture) -> bool
    {
        const FGameplayTag CollectionLabel = FGameplayTag::RequestGameplayTag(
            FName{TEXT("EntityCollection.Objectives")}, false);
        if (NOT CollectionLabel.IsValid()) { return false; }

        OutFixture.OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        OutFixture.OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (NOT AddAuthorityAndWorld(OutFixture.OwnerA) || NOT AddAuthorityAndWorld(OutFixture.OwnerB)) { return false; }

        OutFixture.CollectionA = UCk_Utils_EntityCollection_UE::Add(
            OutFixture.OwnerA, FCk_Fragment_EntityCollection_ParamsData{CollectionLabel}, ECk_Replication::DoesNotReplicate);
        OutFixture.CollectionB = UCk_Utils_EntityCollection_UE::Add(
            OutFixture.OwnerB, FCk_Fragment_EntityCollection_ParamsData{CollectionLabel}, ECk_Replication::DoesNotReplicate);
        OutFixture.MemberA1 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OutFixture.OwnerA);
        OutFixture.MemberA2 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OutFixture.OwnerA);
        OutFixture.MemberB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OutFixture.OwnerB);
        if (ck::Is_NOT_Valid(OutFixture.CollectionA) || ck::Is_NOT_Valid(OutFixture.CollectionB)
            || ck::Is_NOT_Valid(OutFixture.MemberA1) || ck::Is_NOT_Valid(OutFixture.MemberA2)
            || ck::Is_NOT_Valid(OutFixture.MemberB)) { return false; }

        UCk_Utils_EntityCollection_UE::Request_AddEntities(OutFixture.CollectionA,
            FCk_Request_EntityCollection_AddEntities{TArray<FCk_Handle>{OutFixture.MemberA1, OutFixture.MemberA2}}, {});
        UCk_Utils_EntityCollection_UE::Request_AddEntities(OutFixture.CollectionB,
            FCk_Request_EntityCollection_AddEntities{TArray<FCk_Handle>{OutFixture.MemberB}}, {});
        ck::FProcessor_EntityCollection_HandleRequests{InWorld.Get_Registry()}.Pump();
        return UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(OutFixture.CollectionA) == 2
            && UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(OutFixture.CollectionB) == 1;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorEntityCollectionsAuthored,
    "Ck.UiAuthoring.EcsDebugger.EntityCollectionsInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorEntityCollectionsAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_entity_collections_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto InvalidInspector = FCkInspector_EntityCollections{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    TestFalse(TEXT("default-invalid entity is not inspectable as Entity Collections"), InvalidInspector.CanInspect(FCk_Handle{}));
    if (NOT TestEqual(TEXT("default-invalid entity safely mounts the authored Entity Collections shell"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_EntityCollectionsAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_EntityCollectionsAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid authored shell is unavailable and publishes no collection records"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_CollectionCount() == 0
            && InvalidAuthored->Get_CollectionsCollection().IsValid()
            && InvalidAuthored->Get_CollectionsCollection()->GetRecords().IsEmpty());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored shell releases without retaining an invalid handle"), InvalidAuthored->Is_Inert());

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(TEXT("hermetic Entity Collections fixture creates two independent owners"), CreateFixture(World, Fixture)))
    { return false; }
    const FCk_EntityCollection_Content SourceContentA =
        UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(Fixture.CollectionA);
    const TArray<FCk_Handle>& SourceMembersA = SourceContentA.Get_EntitiesInCollection();
    if (NOT TestEqual(TEXT("source collection exposes both requested members"), SourceMembersA.Num(), 2)
        || NOT TestTrue(TEXT("source collection members retain distinct handle identities"),
            SourceMembersA.Num() == 2 && SourceMembersA[0] != SourceMembersA[1]
                && SourceMembersA.Contains(Fixture.MemberA1) && SourceMembersA.Contains(Fixture.MemberA2)))
    { return false; }

    auto Inspector = FCkInspector_EntityCollections{};
    const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.OwnerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.OwnerB);
    if (NOT TestEqual(TEXT("A mounts the authored Entity Collections inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_EntityCollectionsAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored Entity Collections inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_EntityCollectionsAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_EntityCollectionsAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(RenderedA);
    const TSharedRef<SCkInspector_EntityCollectionsAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> CollectionsA = AuthoredA->Get_CollectionsCollection();
    TSharedPtr<FCkUiCollection> CollectionsB = AuthoredB->Get_CollectionsCollection();
    if (NOT TestTrue(TEXT("each production build owns independent accepted hierarchical collection state"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && CollectionsA.IsValid() && CollectionsB.IsValid() && ViewA != ViewB && CollectionsA != CollectionsB
            && CollectionsA->GetRecords().Num() == 1 && CollectionsB->GetRecords().Num() == 1
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false; }

    const FString CollectionName = TEXT("EntityCollection.Objectives");
    const FString CollectionKeyA = FindRecordKeyByText(*CollectionsA, TEXT("name"), CollectionName);
    const FString CollectionKeyB = FindRecordKeyByText(*CollectionsB, TEXT("name"), CollectionName);
    const TSharedPtr<const FCkUiRecord> CollectionRecordA = CollectionsA->FindRecord(CollectionKeyA);
    const TSharedPtr<const FCkUiCollection> MembersA = CollectionRecordA.IsValid()
        ? CollectionRecordA->FindChildCollection(TEXT("members")) : nullptr;
    const TSharedPtr<const FCkUiRecord> CollectionRecordB = CollectionsB->FindRecord(CollectionKeyB);
    const TSharedPtr<const FCkUiCollection> MembersB = CollectionRecordB.IsValid()
        ? CollectionRecordB->FindChildCollection(TEXT("members")) : nullptr;
    if (NOT TestTrue(TEXT("same-labelled collections have non-empty stable identities"),
            NOT CollectionKeyA.IsEmpty() && NOT CollectionKeyB.IsEmpty())
        || NOT TestNotEqual(TEXT("same-labelled collections have collision-safe stable identities"),
            CollectionKeyA, CollectionKeyB)
        || NOT TestTrue(TEXT("each collection owns its nested member collection"),
            MembersA.IsValid() && MembersB.IsValid()))
    { return false; }
    if (NOT TestEqual(TEXT("first collection projects both nested members"), MembersA->GetRecords().Num(), 2)
        || NOT TestEqual(TEXT("second collection projects its nested member"), MembersB->GetRecords().Num(), 1))
    { return false; }

    const FString MemberA1Text = Fixture.MemberA1.ToString();
    const FString MemberA2Text = Fixture.MemberA2.ToString();
    const FString MemberA1Label = TEXT("  ") + MemberA1Text;
    const FString MemberA1Key = FindRecordKeyByText(*MembersA, TEXT("label"), MemberA1Label);
    const FString MemberA2Key = FindRecordKeyByText(*MembersA, TEXT("label"), TEXT("  ") + MemberA2Text);
    TestTrue(TEXT("authored hierarchy projects exact count and member labels"),
        GetRecordText(*CollectionsA, CollectionKeyA, TEXT("count-text")) == TEXT("2 entities")
            && NOT MemberA1Key.IsEmpty() && NOT MemberA2Key.IsEmpty() && MemberA1Key != MemberA2Key
            && GetRecordBool(*MembersA, MemberA1Key, TEXT("remove-visible")));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.OwnerA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.OwnerB); RowsB = Capture.Get_Rows(); }
    TestTrue(TEXT("native row capture remains the exact comparison authority"),
        RowsA.FindRef(CollectionName) == TEXT("2 entities") && RowsB.FindRef(CollectionName) == TEXT("1 entities")
            && RowsA.FindRef(MemberA1Label) == TEXT("Remove") && RowsA.FindRef(TEXT("  ") + MemberA2Text) == TEXT("Remove"));
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TSharedPtr<SCkInspector_EntityCollectionsAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(Inspector.Build_Inspector(Fixture.OwnerA));
    }
    DiffAuthored->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("authored collection and member diff marks retain the exact native stable-key verdict"),
        Differing.Contains(CollectionName) && Differing.Contains(MemberA1Label)
            && DiffAuthored->Is_CollectionDiffMarked(CollectionKeyA)
            && DiffAuthored->Is_MemberDiffMarked(MemberA1Key)
            && DiffAuthored->Is_MemberDiffMarked(MemberA2Key));

    const TSharedPtr<SButton> HeldSelect = FindTaggedButtonForText(RenderedA, TEXT("2 entities"), TEXT("entity-collections-select"));
    const TSharedPtr<SButton> HeldRemoveA1 = FindTaggedButtonForText(RenderedA, MemberA1Text, TEXT("entity-collections-remove"));
    const TSharedPtr<SButton> HeldRemoveA2 = FindTaggedButtonForText(RenderedA, MemberA2Text, TEXT("entity-collections-remove"));
    if (NOT TestTrue(TEXT("authored hierarchy mounts the physical Select action"), HeldSelect.IsValid())
        || NOT TestTrue(TEXT("authored hierarchy mounts the first physical Remove action"), HeldRemoveA1.IsValid())
        || NOT TestTrue(TEXT("authored hierarchy mounts the second physical Remove action"), HeldRemoveA2.IsValid()))
    { return false; }
    if (NOT TestTrue(TEXT("authored Select and per-member Remove actions are enabled"),
        HeldSelect->IsEnabled() && HeldRemoveA1->IsEnabled() && HeldRemoveA2->IsEnabled()))
    { return false; }
    HeldSelect->SimulateClick();
    TestTrue(TEXT("physical count action selects the exact collection"),
        Selection->Get_SelectedEntities().Num() == 1 && Selection->Get_SelectedEntities()[0] == FCk_Handle{Fixture.CollectionA});

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Entity Collections HTML and CSS resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityCollections.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityCollections.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns the hierarchical count, selection, and removal layout"),
        Markup.Contains(TEXT("bind=\"entity-collections\"")) && Markup.Contains(TEXT("child-bind=\"members\""))
            && Markup.Contains(TEXT("item-action=\"entity-collections-select\""))
            && Markup.Contains(TEXT("item-action=\"entity-collections-remove-member\"")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    const TSharedPtr<const FCkUiRecord> StableCollectionA = CollectionsA->FindRecord(CollectionKeyA);
    TestTrue(TEXT("compatible Entity Collections reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Entity Collections A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload preserves view, parent record, and physical nested action identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && CollectionsA->FindRecord(CollectionKeyA) == StableCollectionA
            && FindTaggedButtonForText(RenderedA, MemberA1Text, TEXT("entity-collections-remove")) == HeldRemoveA1);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing nested members binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("child-bind=\"members\""), TEXT("child-bind=\"missing-members\"")),
        Stylesheet, TEXT("Entity Collections B missing nested collection")).Succeeded);
    TestFalse(TEXT("missing member action binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("item-action=\"entity-collections-remove-member\""),
            TEXT("item-action=\"entity-collections-missing-remove\"")),
        Stylesheet, TEXT("Entity Collections B missing Remove action")).Succeeded);
    TestTrue(TEXT("rejected candidates retain B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TSharedRef<SCkInspector_EntityCollectionsAuthored> Filtered =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, CollectionName));
    const TSharedRef<SCkInspector_EntityCollectionsAuthored> RemoveOnly =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, TEXT("Remove")));
    const FString FilteredKey = FindRecordKeyByText(*Filtered->Get_CollectionsCollection(), TEXT("name"), CollectionName);
    const FString RemoveKey = FindRecordKeyByText(*RemoveOnly->Get_CollectionsCollection(), TEXT("name"), CollectionName);
    TestTrue(TEXT("filter independently matches collection and nested Remove placements"),
        GetRecordBool(*Filtered->Get_CollectionsCollection(), FilteredKey, TEXT("collection-visible"))
            && NOT GetRecordBool(*RemoveOnly->Get_CollectionsCollection(), RemoveKey, TEXT("collection-visible"))
            && GetRecordBool(*RemoveOnly->Get_CollectionsCollection(), RemoveKey, TEXT("record-visible")));
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
    const TSharedRef<SCkInspector_EntityCollectionsAuthored> HiddenEdits =
        StaticCastSharedRef<SCkInspector_EntityCollectionsAuthored>(Inspector.Build_Inspector(Fixture.OwnerA, CollectionName));
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;
    const FString HiddenKey = FindRecordKeyByText(*HiddenEdits->Get_CollectionsCollection(), TEXT("name"), CollectionName);
    const TSharedPtr<const FCkUiRecord> HiddenRecord = HiddenEdits->Get_CollectionsCollection()->FindRecord(HiddenKey);
    const TSharedPtr<const FCkUiCollection> HiddenMembers = HiddenRecord.IsValid()
        ? HiddenRecord->FindChildCollection(TEXT("members")) : nullptr;
    const FString HiddenMemberKey = HiddenMembers.IsValid()
        ? FindRecordKeyByText(*HiddenMembers, TEXT("label"), MemberA1Label) : FString{};
    TestTrue(TEXT("Hidden edit-controls suppresses nested Remove placement without suppressing collection selection"),
        GetRecordBool(*HiddenEdits->Get_CollectionsCollection(), HiddenKey, TEXT("collection-visible"))
            && HiddenMembers.IsValid() && NOT GetRecordBool(*HiddenMembers, HiddenMemberKey, TEXT("remove-visible")));

    HeldRemoveA1->SimulateClick();
    if (NOT TestTrue(TEXT("physical Remove queues exactly one public collection request before the deferred pump"),
        Fixture.CollectionA.Has<ck::FFragment_EntityCollection_Requests>()
            && Fixture.CollectionA.Get<ck::FFragment_EntityCollection_Requests>().Get_Requests().Num() == 1)) { return false; }
    ck::FProcessor_EntityCollection_HandleRequests{World.Get_Registry()}.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    const TSharedPtr<const FCkUiRecord> UpdatedCollectionA = CollectionsA->FindRecord(CollectionKeyA);
    const TSharedPtr<const FCkUiCollection> UpdatedMembersA = UpdatedCollectionA.IsValid()
        ? UpdatedCollectionA->FindChildCollection(TEXT("members")) : nullptr;
    TestTrue(TEXT("deferred pump removes only the selected member and authored hierarchy reconciles live"),
        NOT UCk_Utils_EntityCollection_UE::Get_ContainsEntityInCollection(Fixture.CollectionA, Fixture.MemberA1)
            && UCk_Utils_EntityCollection_UE::Get_ContainsEntityInCollection(Fixture.CollectionA, Fixture.MemberA2)
            && UpdatedMembersA.IsValid() && UpdatedMembersA->GetRecords().Num() == 1
            && GetRecordText(*CollectionsA, CollectionKeyA, TEXT("count-text")) == TEXT("1 entities"));
    HeldRemoveA1->SimulateClick();
    TestFalse(TEXT("held stale member control cannot queue a second request after membership loss"),
        Fixture.CollectionA.Has<ck::FFragment_EntityCollection_Requests>());

    Selection->Set_SelectedEntities({Fixture.MemberB});
    TestTrue(TEXT("owner record teardown leaves the owner and collection entities live"),
        Fixture.OwnerA.Try_Remove<ck::FFragment_RecordOfEntityCollections>()
            && ck::IsValid(Fixture.OwnerA) && ck::IsValid(Fixture.CollectionA) && NOT Inspector.CanInspect(Fixture.OwnerA));
    HeldSelect->SimulateClick();
    HeldRemoveA2->SimulateClick();
    TestTrue(TEXT("held controls fail closed after owner-to-collection composition teardown"),
        Selection->Get_SelectedEntities().Num() == 1 && Selection->Get_SelectedEntities()[0] == Fixture.MemberB
            && NOT Fixture.CollectionA.Has<ck::FFragment_EntityCollection_Requests>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("Entity Collections deactivation releases every retained view and hierarchy"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && Filtered->Is_Inert() && RemoveOnly->Is_Inert()
            && HiddenEdits->Is_Inert() && NOT AuthoredA->Get_View().IsValid()
            && NOT AuthoredB->Get_View().IsValid() && NOT AuthoredA->Get_CollectionsCollection().IsValid());
    HeldSelect->SimulateClick();
    HeldRemoveA2->SimulateClick();
    TestTrue(TEXT("held controls remain inert after inspector release"),
        Selection->Get_SelectedEntities().Num() == 1 && NOT Fixture.CollectionA.Has<ck::FFragment_EntityCollection_Requests>());
    return true;
}

#endif
