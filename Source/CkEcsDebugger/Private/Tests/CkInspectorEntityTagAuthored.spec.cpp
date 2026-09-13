#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_EntityTag.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEntityTag/CkEntityTag_Processor.h"
#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"

#include <variant>

namespace ck_inspector_entity_tag_authored_test
{
    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto FindButtonWithTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SButton> Found = FindButtonWithTag(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto FindButtonBesideText(
        const TSharedRef<SWidget>& InRoot,
        const FString& InContextText,
        const FName InButtonTag) -> TSharedPtr<SButton>
    {
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SButton> Found = FindButtonBesideText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InContextText, InButtonTag);
            if (Found.IsValid()) { return Found; }
        }
        return ContainsText(InRoot, InContextText) ? FindButtonWithTag(InRoot, InButtonTag) : nullptr;
    }

    auto CountType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> int32
    {
        int32 Count = InRoot->GetTypeAsString() == InType ? 1 : 0;
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { Count += CountType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); }
        return Count;
    }

    auto FindWidgetWithTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SWidget> Found = FindWidgetWithTag(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto GetToolTipText(const TSharedRef<SWidget>& InWidget) -> FString
    {
        const TSharedPtr<IToolTip> Tooltip = InWidget->GetToolTip();
        if (NOT Tooltip.IsValid()) { return {}; }
        const TSharedRef<SWidget> Content = Tooltip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString();
    }

    auto GetRecordBool(const FCkUiCollection& InCollection, const FString& InKey, const FString& InField) -> bool
    {
        const TSharedPtr<const FCkUiRecord> Record = InCollection.FindRecord(InKey);
        const FCkUiFieldValue* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
        return Field != nullptr && Field->Kind == ECkUiFieldKind::Bool && Field->Bool;
    }

    auto TickAuthored(const TSharedRef<SCkInspector_EntityTagAuthored>& InAuthored) -> void
    {
        InAuthored->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorEntityTagAuthored,
    "Ck.UiAuthoring.EcsDebugger.EntityTagInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorEntityTagAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_entity_tag_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto InvalidInspector = FCkInspector_EntityTag{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored EntityTag shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagAuthored")}))
    {
        AddError(InvalidInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_EntityTagAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_EntityTagAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid EntityTag getters and collections fail closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_FNameCountText() == TEXT("--")
            && InvalidAuthored->Get_GameplayTagRootCountText() == TEXT("--")
            && InvalidAuthored->Get_FNameTagsCollection()->GetRecords().IsEmpty()
            && InvalidAuthored->Get_GameplayTagRootsCollection()->GetRecords().IsEmpty());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid EntityTag shell releases both record collections"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT InvalidAuthored->Get_View().IsValid()
            && NOT InvalidAuthored->Get_FNameTagsCollection().IsValid()
            && NOT InvalidAuthored->Get_GameplayTagRootsCollection().IsValid());

    const FGameplayTag RootA = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Attack")}, false);
    const FGameplayTag RootB = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Defense")}, false);
    const FName PlainA{TEXT("EntityTag.Authored.PlainA")};
    const FName PlainB{TEXT("EntityTag.Authored.PlainB")};
    const FName PlainOnly{TEXT("EntityTag.Authored.PlainOnly")};
    if (NOT TestTrue(TEXT("fixture reuses distinct registered gameplay tags without native registration"),
        RootA.IsValid() && RootB.IsValid() && RootA != RootB)) { return false; }

    auto World = ck::FEcsWorld{};
    auto RequestProcessor = ck::FProcessor_EntityTag_HandleRequests{World.Get_Registry()};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto FNameOnlyEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto RootOnlyEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture creates four distinct live entities"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && ck::IsValid(FNameOnlyEntity) && ck::IsValid(RootOnlyEntity)
            && EntityA != EntityB && EntityA != FNameOnlyEntity && EntityA != RootOnlyEntity
            && EntityB != FNameOnlyEntity && EntityB != RootOnlyEntity && FNameOnlyEntity != RootOnlyEntity)) { return false; }
    UWorld* const TestWorld = GWorld;
    if (NOT TestNotNull(TEXT("fixture runs inside the automation editor world"), TestWorld)) { return false; }
    EntityA.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    EntityB.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    const auto AuthoritySettings = FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority};
    UCk_Utils_Net_UE::Add(EntityA, AuthoritySettings);
    UCk_Utils_Net_UE::Add(EntityB, AuthoritySettings);
    FNameOnlyEntity.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    RootOnlyEntity.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    UCk_Utils_Net_UE::Add(FNameOnlyEntity, AuthoritySettings);
    UCk_Utils_Net_UE::Add(RootOnlyEntity, AuthoritySettings);
    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(EntityA, RootA);
    UCk_Utils_EntityTag_UE::Add(EntityA, PlainA);
    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(EntityB, RootB);
    UCk_Utils_EntityTag_UE::Add(EntityB, PlainB);
    UCk_Utils_EntityTag_UE::Add(FNameOnlyEntity, PlainOnly);
    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(RootOnlyEntity, RootA);
    RequestProcessor.Pump();
    const TArray<FName> FlattenedA = UCk_Utils_EntityTag_UE::Get_AllTags(EntityA);
    const TArray<FName> FlattenedB = UCk_Utils_EntityTag_UE::Get_AllTags(EntityB);
    const FGameplayTagContainer RootsA = UCk_Utils_EntityTag_UE::Get_AllTagsAsContainer(EntityA);
    const FGameplayTagContainer RootsB = UCk_Utils_EntityTag_UE::Get_AllTagsAsContainer(EntityB);
    if (NOT TestTrue(TEXT("fixture distinguishes flattened FName tags from explicit gameplay roots"),
        FlattenedA.Contains(PlainA) && FlattenedB.Contains(PlainB)
            && FlattenedA.Contains(RootA.GetTagName()) && FlattenedB.Contains(RootB.GetTagName())
            && RootsA.Num() == 1 && RootsB.Num() == 1 && RootsA.HasTagExact(RootA) && RootsB.HasTagExact(RootB)
            && FlattenedA.Num() > RootsA.Num() && FlattenedB.Num() > RootsB.Num())) { return false; }

    {
        auto ParityInspector = FCkInspector_EntityTag{};
        const TSharedRef<SWidget> FNameOnlyRendered = ParityInspector.Build_Inspector(FNameOnlyEntity);
        const TSharedRef<SWidget> RootOnlyRendered = ParityInspector.Build_Inspector(RootOnlyEntity);
        FNameOnlyRendered->SlatePrepass();
        RootOnlyRendered->SlatePrepass();
        const TSharedPtr<SWidget> FNameOnlyRootCount = FindWidgetWithTag(FNameOnlyRendered, TEXT("entity-tag-root-count-row"));
        const TSharedPtr<SWidget> FNameOnlyRoots = FindWidgetWithTag(FNameOnlyRendered, TEXT("entity-tag-roots-row"));
        const TSharedPtr<SWidget> RootOnlyRootCount = FindWidgetWithTag(RootOnlyRendered, TEXT("entity-tag-root-count-row"));
        const TSharedPtr<SWidget> RootOnlyRoots = FindWidgetWithTag(RootOnlyRendered, TEXT("entity-tag-roots-row"));
        auto FNameOnlyRows = TMap<FString, FString>{};
        auto RootOnlyRows = TMap<FString, FString>{};
        { const auto Capture = FCkInspector_RowCaptureScope{}; ParityInspector.Build_Inspector(FNameOnlyEntity); FNameOnlyRows = Capture.Get_Rows(); }
        { const auto Capture = FCkInspector_RowCaptureScope{}; ParityInspector.Build_Inspector(RootOnlyEntity); RootOnlyRows = Capture.Get_Rows(); }
        TestTrue(TEXT("FName-only EntityTag preserves native omission of both gameplay-root rows"),
            FNameOnlyRootCount.IsValid() && FNameOnlyRoots.IsValid()
                && FNameOnlyRootCount->GetVisibility() == EVisibility::Collapsed
                && FNameOnlyRoots->GetVisibility() == EVisibility::Collapsed
                && NOT FNameOnlyRows.Contains(TEXT("GameplayTag Roots:")) && NOT FNameOnlyRows.Contains(TEXT("Roots:")));
        TestTrue(TEXT("root-only EntityTag retains both native gameplay-root rows"),
            RootOnlyRootCount.IsValid() && RootOnlyRoots.IsValid()
                && RootOnlyRootCount->GetVisibility() == EVisibility::Visible
                && RootOnlyRoots->GetVisibility() == EVisibility::Visible
                && RootOnlyRows.Contains(TEXT("GameplayTag Roots:")) && RootOnlyRows.Contains(TEXT("Roots:")));
        ParityInspector.OnDeactivated();
    }

    auto Inspector = FCkInspector_EntityTag{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("A mounts the authored EntityTag inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored EntityTag inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_EntityTagAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_EntityTagAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_EntityTagAuthored>(RenderedA);
    const TSharedRef<SCkInspector_EntityTagAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_EntityTagAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> FNamesA = AuthoredA->Get_FNameTagsCollection();
    TSharedPtr<FCkUiCollection> FNamesB = AuthoredB->Get_FNameTagsCollection();
    TSharedPtr<FCkUiCollection> GameplayRootsA = AuthoredA->Get_GameplayTagRootsCollection();
    TSharedPtr<FCkUiCollection> GameplayRootsB = AuthoredB->Get_GameplayTagRootsCollection();
    if (NOT TestTrue(TEXT("each EntityTag build retains independent view and two collection owners"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && FNamesA.IsValid() && FNamesB.IsValid() && FNamesA != FNamesB
            && GameplayRootsA.IsValid() && GameplayRootsB.IsValid() && GameplayRootsA != GameplayRootsB
            && FNamesA->GetRecords().Num() == FlattenedA.Num() && FNamesB->GetRecords().Num() == FlattenedB.Num()
            && GameplayRootsA->GetRecords().Num() == RootsA.Num() && GameplayRootsB->GetRecords().Num() == RootsB.Num()))
    { return false; }
    TestTrue(TEXT("authored collections preserve flattened FName and explicit root chips separately"),
        FNamesA->FindRecord(FString{TEXT("fname:")} + PlainA.ToString()).IsValid()
            && GameplayRootsA->FindRecord(FString{TEXT("root:")} + RootA.ToString()).IsValid()
            && ContainsText(RenderedA, PlainA.ToString()) && ContainsText(RenderedA, RootA.ToString())
            && ContainsText(RenderedB, PlainB.ToString()) && ContainsText(RenderedB, RootB.ToString()));
    TestTrue(TEXT("both authored native add editor placements physically mount"),
        CountType(RenderedA, TEXT("SEditableTextBox")) >= 2 && CountType(RenderedB, TEXT("SEditableTextBox")) >= 2);

    const TSharedRef<SWidget> FilteredA = Inspector.Build_Inspector(EntityA, PlainA.ToString());
    FilteredA->SlatePrepass();
    const TSharedPtr<SWidget> FilteredFNameTags = FindWidgetWithTag(FilteredA, TEXT("entity-tag-fname-tags-row"));
    const TSharedPtr<SWidget> FilteredRoots = FindWidgetWithTag(FilteredA, TEXT("entity-tag-roots-row"));
    TestTrue(TEXT("EntityTag filter shows the matching flattened FName row and collapses the unmatched root row"),
        ContainsText(FilteredA, PlainA.ToString()) && FilteredFNameTags.IsValid() && FilteredRoots.IsValid()
            && FilteredFNameTags->GetVisibility() == EVisibility::Visible
            && FilteredRoots->GetVisibility() == EVisibility::Collapsed);

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture preserves the exact EntityTag row labels and comparison authority"),
        RowsA.Contains(TEXT("FName Tag Count:")) && RowsA.Contains(TEXT("FName Tags:"))
            && RowsA.Contains(TEXT("GameplayTag Roots:")) && RowsA.Contains(TEXT("Roots:"))
            && NOT Differing.Contains(TEXT("FName Tag Count:")) && Differing.Contains(TEXT("FName Tags:"))
            && NOT Differing.Contains(TEXT("GameplayTag Roots:")) && Differing.Contains(TEXT("Roots:")));
    TSharedPtr<SCkInspector_EntityTagAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_EntityTagAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored EntityTag rows receive the exact native diff marks"),
        DiffAuthored.IsValid() && NOT DiffAuthored->Is_FNameCountDiffMarked() && DiffAuthored->Is_FNameTagsDiffMarked()
            && NOT DiffAuthored->Is_GameplayTagRootCountDiffMarked() && DiffAuthored->Is_GameplayTagRootsDiffMarked());

    const TSharedPtr<SButton> HeldNameRemove = FindButtonBesideText(RenderedA, PlainA.ToString(), TEXT("entity-tag-fname-remove"));
    const TSharedPtr<SButton> HeldRootRemove = FindButtonBesideText(RenderedA, RootA.ToString(), TEXT("entity-tag-root-remove"));
    TestTrue(TEXT("FName record projects an Inline-visible Remove action"),
        GetRecordBool(*FNamesA, FString{TEXT("fname:")} + PlainA.ToString(), TEXT("remove-visible")));
    TestTrue(TEXT("root record projects an Inline-visible Remove Root action"),
        GetRecordBool(*GameplayRootsA, FString{TEXT("root:")} + RootA.ToString(), TEXT("remove-visible")));
    TestTrue(TEXT("mounted EntityTag tree contains authored action buttons"), CountType(RenderedA, TEXT("SButton")) > 0);
    if (NOT TestTrue(TEXT("per-FName Remove action is physical"), HeldNameRemove.IsValid())
        || NOT TestTrue(TEXT("per-root Remove Root action is physical"), HeldRootRemove.IsValid()))
    { return false; }
    if (NOT TestTrue(TEXT("per-FName Remove action is enabled while EntityTag is inspectable"), HeldNameRemove->IsEnabled())
        || NOT TestTrue(TEXT("per-root Remove Root action is enabled while EntityTag is inspectable"), HeldRootRemove->IsEnabled()))
    { return false; }
    HeldNameRemove->SimulateClick();
    HeldRootRemove->SimulateClick();
    if (NOT TestTrue(TEXT("physical remove actions dispatch two public EntityTag requests before pump"),
        EntityA.Has<ck::FFragment_EntityTag_Requests>()
            && EntityA.Get<ck::FFragment_EntityTag_Requests>().Get_Requests().Num() == 2)) { return false; }
    const auto& Requests = EntityA.Get<ck::FFragment_EntityTag_Requests>().Get_Requests();
    TestTrue(TEXT("per-FName Remove dispatches the exact FName request and payload"),
        std::holds_alternative<FCk_Request_EntityTag_TryRemove>(Requests[0])
            && std::get<FCk_Request_EntityTag_TryRemove>(Requests[0]).Get_Tag() == PlainA);
    TestTrue(TEXT("per-root Remove Root dispatches the exact GameplayTag request and payload"),
        std::holds_alternative<FCk_Request_EntityTag_TryRemoveGameplayTag>(Requests[1])
            && std::get<FCk_Request_EntityTag_TryRemoveGameplayTag>(Requests[1]).Get_Tag() == RootA);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed EntityTag resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTag.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorEntityTag.ui.css"))))) { return false; }
    TestTrue(TEXT("EntityTag markup owns all authored labels, collections, actions, and both native ports"),
        Markup.Contains(TEXT(">FName Tag Count:</text>")) && Markup.Contains(TEXT(">FName Tags:</text>"))
            && Markup.Contains(TEXT(">GameplayTag Roots:</text>")) && Markup.Contains(TEXT(">Roots:</text>"))
            && Markup.Contains(TEXT("entity-tag-remove-fname")) && Markup.Contains(TEXT("entity-tag-remove-root"))
            && Markup.Contains(TEXT("entity-tag-add-name-port")) && Markup.Contains(TEXT("entity-tag-add-gameplay-tag-port")));
    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible EntityTag reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("EntityTag A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload retains A view and action identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore
            && FindButtonBesideText(RenderedA, PlainA.ToString(), TEXT("entity-tag-fname-remove")) == HeldNameRemove);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing required EntityTag native port is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("bind=\"entity-tag-add-name-port\""), TEXT("bind=\"entity-tag-missing-port\"")),
        Stylesheet, TEXT("EntityTag B missing port candidate")).Succeeded);
    TestFalse(TEXT("missing required EntityTag action is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("item-action=\"entity-tag-remove-fname\""), TEXT("item-action=\"entity-tag-missing-action\"")),
        Stylesheet, TEXT("EntityTag B missing action candidate")).Succeeded);
    TestTrue(TEXT("rejected EntityTag reloads retain B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    RequestProcessor.Pump();
    TickAuthored(AuthoredA);
    HeldNameRemove->SlatePrepass();
    HeldRootRemove->SlatePrepass();
    TestTrue(TEXT("pump and tick reconcile all records after removing every tag while EntityA remains live"),
        ck::IsValid(EntityA) && NOT Inspector.CanInspect(EntityA) && NOT AuthoredA->Get_IsAvailable()
            && AuthoredA->Get_FNameCountText() == TEXT("--") && AuthoredA->Get_GameplayTagRootCountText() == TEXT("--")
            && FNamesA->GetRecords().IsEmpty() && GameplayRootsA->GetRecords().IsEmpty()
            && NOT HeldNameRemove->IsEnabled() && NOT HeldRootRemove->IsEnabled());
    TestTrue(TEXT("stale EntityTag remove controls explain their disabled state"),
        GetToolTipText(HeldNameRemove.ToSharedRef()).Contains(TEXT("Entity Tag is unavailable"))
            && GetToolTipText(HeldRootRemove.ToSharedRef()).Contains(TEXT("Entity Tag is unavailable")));
    HeldNameRemove->SimulateClick();
    HeldRootRemove->SimulateClick();
    TestFalse(TEXT("held controls cannot recreate or enqueue tags after EntityTag loss"),
        EntityA.Has<ck::FFragment_EntityTag_Current>() || EntityA.Has<ck::FFragment_EntityTag_Requests>());

    {
        auto StyleInspector = FCkInspector_EntityTag{};
        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
        const TSharedRef<SWidget> Hidden = StyleInspector.Build_Inspector(EntityB);
        TestTrue(TEXT("Hidden edit-control style removes EntityTag verbs and both native editors"),
            CountType(Hidden, TEXT("SButton")) == 0 && CountType(Hidden, TEXT("SEditableTextBox")) == 0);
        StyleInspector.OnDeactivated();

        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::OnHover;
        const TSharedRef<SWidget> OnHover = StyleInspector.Build_Inspector(EntityB);
        OnHover->SlatePrepass();
        const TSharedPtr<SButton> HoverRemove = FindButtonBesideText(OnHover, PlainB.ToString(), TEXT("entity-tag-fname-remove"));
        if (TestTrue(TEXT("On-Hover style retains the EntityTag remove action"), HoverRemove.IsValid()))
        {
            TestEqual(TEXT("On-Hover EntityTag action starts hidden until row hover"), HoverRemove->GetVisibility(), EVisibility::Hidden);
        }
        StyleInspector.OnDeactivated();
        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;
    }

    TSharedPtr<SCkInspector_EntityTagAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_EntityTag>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_EntityTagAuthored>(DestructorInspector->Build_Inspector(EntityB));
    }
    TestTrue(TEXT("EntityTag inspector destruction makes retained authored work inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases views and both collections from every retained EntityTag build"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid() && NOT DiffAuthored->Get_View().IsValid()
            && NOT AuthoredA->Get_FNameTagsCollection().IsValid() && NOT AuthoredB->Get_FNameTagsCollection().IsValid()
            && NOT AuthoredA->Get_GameplayTagRootsCollection().IsValid() && NOT AuthoredB->Get_GameplayTagRootsCollection().IsValid());
    TestTrue(TEXT("deactivation detaches both retained native EntityTag editor ports"),
        CountType(RenderedA, TEXT("SEditableTextBox")) == 0 && CountType(RenderedB, TEXT("SEditableTextBox")) == 0);
    return true;
}

#endif
