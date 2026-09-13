#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_EntityTagQuery.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEntityTag/CkEntityTag_Processor.h"
#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkEntityTag/Query/CkEntityTagQuery_Fragment.h"
#include "CkEntityTag/Query/CkEntityTagQuery_Processor.h"
#include "CkEntityTag/Query/CkEntityTagQuery_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"

namespace ck_inspector_entity_tag_query_authored_test
{
    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto FindTaggedButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if ((InRoot->GetTypeAsString() == TEXT("SButton") || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton"))
            && InRoot->GetTag() == InTag) { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindTaggedButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto CollectEntityRefs(const TSharedRef<SWidget>& InRoot, TArray<TSharedRef<SCkDebug_EntityRef>>& OutEntityRefs) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_EntityRef"))
        { OutEntityRefs.Add(StaticCastSharedRef<SCkDebug_EntityRef>(InRoot)); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            CollectEntityRefs(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutEntityRefs);
        }
    }

    auto CreateQuery(const FCk_Handle& InOwner) -> FCk_Handle
    {
        auto MutableOwner = InOwner;
        return UCk_Utils_EntityTagQuery_UE::Add(MutableOwner);
    }

    auto PumpQuery(ck::FEcsWorld& InWorld) -> void
    {
        ck::FProcessor_EntityTag_HandleRequests{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_EntityTagQuery_HandleRequests{InWorld.Get_Registry()}.Pump();
        ck::FProcessor_EntityTagQuery_Evaluate{InWorld.Get_Registry()}.Pump();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorEntityTagQueryAuthored,
    "Ck.UiAuthoring.EcsDebugger.EntityTagQueryInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorEntityTagQueryAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_entity_tag_query_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture has transient ownership and an automation world"),
        ck::IsValid(Owner) && GWorld != nullptr)) { return false; }
    Owner.Add<TWeakObjectPtr<UWorld>>(GWorld);
    FCk_Handle QueryEntityA = CreateQuery(Owner);
    FCk_Handle QueryEntityB = CreateQuery(Owner);
    FCk_Handle QueryEntityC = CreateQuery(Owner);
    FCk_Handle MatchA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    FCk_Handle MatchB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    if (NOT TestTrue(TEXT("fixture creates three queries and two match candidates"),
        ck::IsValid(QueryEntityA) && ck::IsValid(QueryEntityB) && ck::IsValid(QueryEntityC)
            && ck::IsValid(MatchA) && ck::IsValid(MatchB))) { return false; }
    UCk_Utils_EntityTag_UE::Add(MatchA, TEXT("Test.Query"));
    UCk_Utils_EntityTag_UE::Add(MatchB, TEXT("Test.Query"));
    auto QueryA = UCk_Utils_EntityTagQuery_UE::Cast(QueryEntityA);
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(QueryA,
        FCk_Request_EntityTagQuery_AddRequirement{
            UCk_Utils_EntityTagQuery_UE::Make_Requirement_Of(TEXT("Test.Query"), 2)}, {});
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(QueryA,
        FCk_Request_EntityTagQuery_AddRequirement{
            UCk_Utils_EntityTagQuery_UE::Make_Requirement_Single_WithEnsure(TEXT("Test.Missing"), 3)}, {});
    auto QueryC = UCk_Utils_EntityTagQuery_UE::Cast(QueryEntityC);
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(QueryC,
        FCk_Request_EntityTagQuery_AddRequirement{
            UCk_Utils_EntityTagQuery_UE::Make_Requirement_Single(TEXT("Test.Duplicate"))}, {});
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(QueryC,
        FCk_Request_EntityTagQuery_AddRequirement{
            UCk_Utils_EntityTagQuery_UE::Make_Requirement_Of(TEXT("Test.Duplicate"), 2)}, {});
    PumpQuery(World);
    TestTrue(TEXT("fixture evaluates one satisfied match set and one ensured missing set"),
        UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(QueryA).Num() == 2
            && UCk_Utils_EntityTagQuery_UE::Get_CurrentResults(QueryA)[0].Get_Handles().Num() == 2
            && NOT UCk_Utils_EntityTagQuery_UE::Get_IsSatisfied(QueryA));

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    const TSharedRef<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    auto Inspector = FCkInspector_EntityTagQuery{};
    Inspector.Set_EditGuard(EditGuard);
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(QueryEntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(QueryEntityB);
    const TSharedRef<SWidget> RenderedC = Inspector.Build_Inspector(QueryEntityC);
    if (NOT TestEqual(TEXT("first query mounts authored shell"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagQueryAuthored")})
        || NOT TestEqual(TEXT("second query mounts an independent authored shell"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagQueryAuthored")})
        || NOT TestEqual(TEXT("duplicate-tag query mounts without record-key collisions"), RenderedC->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagQueryAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_EntityTagQueryAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_EntityTagQueryAuthored>(RenderedA);
    const TSharedRef<SCkInspector_EntityTagQueryAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_EntityTagQueryAuthored>(RenderedB);
    const TSharedRef<SCkInspector_EntityTagQueryAuthored> AuthoredC = StaticCastSharedRef<SCkInspector_EntityTagQueryAuthored>(RenderedC);
    const TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    const TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("independent views project distinct query state"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && AuthoredA->Get_RequirementsText() == TEXT("2") && AuthoredB->Get_RequirementsText() == TEXT("0")
            && AuthoredA->Get_IsSatisfiedText() == TEXT("No"));
    TestTrue(TEXT("duplicate same-tag requirements retain distinct authored records"),
        AuthoredC->Is_Mounted() && AuthoredC->Get_RequirementsText() == TEXT("2")
            && AuthoredC->Get_RowsCollection().IsValid() && AuthoredC->Get_RowsCollection()->GetRecords().Num() == 2);

    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    const TSharedPtr<SButton> AddButton = FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-add"));
    const TSharedPtr<SButton> RemoveButton = FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-remove"));
    auto EntityRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), EntityRefs);
    TestTrue(TEXT("markup mounts canonical leaves, authored actions, and real matched entity references"),
        FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-tag-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-kind-input")).IsValid()
            && FindTaggedWidget(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-count-input")).IsValid()
            && AddButton.IsValid() && RemoveButton.IsValid() && EntityRefs.Num() > 0);

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(QueryEntityA); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(QueryEntityB); RowsB = Capture.Get_Rows(); }
    TestTrue(TEXT("native capture remains complete fallback and diff authority"),
        RowsA.Num() == 11 && RowsB.Num() == 6
            && RowsA.Contains(TEXT("Is Satisfied:")) && RowsA.Contains(TEXT("Requirements (2):"))
            && RowsA.Contains(TEXT("    Matches:")) && RowsA.Contains(TEXT("    Ensure <=:"))
            && RowsA.Contains(TEXT("Tag:")) && RowsA.Contains(TEXT("Kind:")) && RowsA.Contains(TEXT("Count (Of):")));

    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{120.0f, 20.0f}, FSlateLayoutTransform{});
    const FPointerEvent LeftClick{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::LeftMouseButton},
        EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    TSharedPtr<SCkDebug_EntityRef> MatchRef;
    int32 NavigableEntityRefCount = 0;
    for (const TSharedRef<SCkDebug_EntityRef>& EntityRef : EntityRefs)
    {
        if (EntityRef->OnCursorQuery(Geometry, LeftClick).IsEventHandled())
        {
            ++NavigableEntityRefCount;
            if (NOT MatchRef.IsValid()) { MatchRef = EntityRef; }
        }
    }
    TestEqual(TEXT("only the two visible matched entity references advertise navigation"), NavigableEntityRefCount, 2);
    if (NOT TestTrue(TEXT("a visible matched entity reference is available for physical routing"), MatchRef.IsValid()))
    { return false; }
    const FCursorReply MatchCursor = MatchRef->OnCursorQuery(Geometry, LeftClick);
    const FReply MatchClick = MatchRef->OnMouseButtonDown(Geometry, LeftClick);
    TestTrue(TEXT("physical matched entity reference advertises authored navigation"),
        MatchCursor.IsEventHandled() && MatchCursor.GetCursorType() == EMouseCursor::Hand);
    TestTrue(TEXT("physical matched entity reference handles its authored navigation click"), MatchClick.IsEventHandled());
    TestEqual(TEXT("physical match action routes to the inspector selection model"),
        Selection->Get_SelectedEntities().Num(), 1);

    AuthoredA->Commit_Tag(TEXT("Test.Added"));
    AuthoredA->Commit_Kind(1);
    AuthoredA->Commit_Count(2);
    AuthoredB->Commit_Tag(TEXT("Test.OtherView"));
    AddButton->SimulateClick();
    TestTrue(TEXT("physical Add action dispatches exactly one staged request for its own view"),
        QueryEntityA.Has<ck::FFragment_EntityTagQuery_Requests>()
            && QueryEntityA.Get<ck::FFragment_EntityTagQuery_Requests>().Get_Requests().Num() == 1
            && NOT QueryEntityB.Has<ck::FFragment_EntityTagQuery_Requests>());
    PumpQuery(World);
    TestTrue(TEXT("staged requirement preserves tag, count mode, count, and view isolation"),
        UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(QueryA).Num() == 3
            && UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(QueryA)[2].Get_Tag() == TEXT("Test.Added")
            && UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(QueryA)[2].Get_Mode() == ECk_EntityTagQuery_CountMode::Count
            && UCk_Utils_EntityTagQuery_UE::Get_AllRequirements(QueryA)[2].Get_Count() == 2
            && AuthoredB->Get_PendingTagText() == TEXT("Test.OtherView"));

    RemoveButton->SimulateClick();
    TestTrue(TEXT("physical stable-key Remove action dispatches the original requirement"),
        QueryEntityA.Has<ck::FFragment_EntityTagQuery_Requests>()
            && QueryEntityA.Get<ck::FFragment_EntityTagQuery_Requests>().Get_Requests().Num() == 1
            && std::holds_alternative<FCk_Request_EntityTagQuery_RemoveRequirement>(
                QueryEntityA.Get<ck::FFragment_EntityTagQuery_Requests>().Get_Requests()[0]));
    PumpQuery(World);

    FCk_Handle HeldMatch = Selection->Get_PrimarySelection();
    UCk_Utils_EntityTagQuery_UE::Request_AddRequirement(QueryA,
        FCk_Request_EntityTagQuery_AddRequirement{
            UCk_Utils_EntityTagQuery_UE::Make_Requirement_Single(TEXT("Test.Rekey"))}, {});
    UCk_Utils_EntityTag_UE::Add(HeldMatch, TEXT("Test.Rekey"));
    UCk_Utils_EntityTag_UE::Request_TryRemove(HeldMatch, TEXT("Test.Query"), {});
    PumpQuery(World);
    Selection->Clear_Selection();
    MatchRef->OnMouseButtonDown(Geometry, LeftClick);
    TestTrue(TEXT("held match link rejects a handle that survives only under a different requirement"),
        Selection->Get_SelectedEntities().IsEmpty());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("Entity Tag Query authored resources are installed"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorEntityTagQuery.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorEntityTagQuery.ui.css"))))) { return false; }
    TestTrue(TEXT("HTML owns the full fixed and repeated layout rather than a whole-panel native wrapper"),
        Markup.Contains(TEXT(">Is Satisfied:</text>")) && Markup.Contains(TEXT(">Add Requirement</text>"))
            && Markup.Contains(TEXT("debug-entity-ref")) && Markup.Contains(TEXT("item-action=\"entity-tag-query-remove\""))
            && Markup.Contains(TEXT("entity-tag-query-kind-port")) && NOT Markup.Contains(TEXT("entity-tag-query-native-body")));

    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload retains physical action identity"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Entity Tag Query compatible candidate")).Succeeded
            && FindTaggedButton(ViewA->GetRegion(TEXT("main")), TEXT("entity-tag-query-add")) == AddButton);
    TestTrue(TEXT("missing native binding reload is atomically rejected"), NOT ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"broken\" bind=\"missing-query-port\" /></region></ui>"),
        TEXT(""), TEXT("Entity Tag Query rejected candidate")).Succeeded && ViewB->GetRevision() == RevisionBBefore);

    TestTrue(TEXT("query composition removal succeeds"), QueryEntityA.Try_Remove<ck::FFragment_EntityTagQuery_Current>());
    const int32 RequestCountBefore = QueryEntityA.Has<ck::FFragment_EntityTagQuery_Requests>()
        ? QueryEntityA.Get<ck::FFragment_EntityTagQuery_Requests>().Get_Requests().Num() : 0;
    AddButton->SimulateClick();
    RemoveButton->SimulateClick();
    TestTrue(TEXT("mounted and held controls fail closed after query composition loss"),
        NOT AuthoredA->Get_IsAvailable() && NOT AuthoredA->Get_CanRequest() && AuthoredA->Get_IsSatisfiedText() == TEXT("--")
            && (NOT QueryEntityA.Has<ck::FFragment_EntityTagQuery_Requests>()
                || QueryEntityA.Get<ck::FFragment_EntityTagQuery_Requests>().Get_Requests().Num() == RequestCountBefore));

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every retained view, native leaf, and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && AuthoredC->Is_Inert() && NOT AuthoredC->Is_Mounted() && NOT EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
