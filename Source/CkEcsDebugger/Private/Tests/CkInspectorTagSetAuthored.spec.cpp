#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_TagSet.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkTagSet/CkTagSet_Fragment.h"
#include "CkTagSet/CkTagSet_Utils.h"

#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"

#include <initializer_list>
#include <variant>

namespace ck_inspector_tagset_authored_test
{
    auto MakeTags(std::initializer_list<FGameplayTag> InTags) -> FGameplayTagContainer
    {
        auto Result = FGameplayTagContainer{};
        for (const FGameplayTag Tag : InTags) { Result.AddTag(Tag); }
        return Result;
    }

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

    auto GetToolTipText(const TSharedRef<SWidget>& InWidget) -> FString
    {
        const TSharedPtr<IToolTip> Tooltip = InWidget->GetToolTip();
        if (NOT Tooltip.IsValid()) { return {}; }
        const TSharedRef<SWidget> Content = Tooltip->GetContentWidget();
        Content->SlatePrepass();
        return Content->GetAccessibleText().ToString();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorTagSetAuthored,
    "Ck.UiAuthoring.EcsDebugger.TagSetInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorTagSetAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_tagset_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    const FGameplayTag TagAlpha = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Attack")}, false);
    const FGameplayTag TagBeta = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Defense")}, false);
    const FGameplayTag TagGamma = FGameplayTag::RequestGameplayTag(FName{TEXT("CueGym.Concurrency.Multiple")}, false);
    if (NOT TestTrue(TEXT("fixture reuses three distinct runtime-owned gameplay tags"),
        TagAlpha.IsValid() && TagBeta.IsValid() && TagGamma.IsValid()
            && TagAlpha != TagBeta && TagAlpha != TagGamma && TagBeta != TagGamma)) { return false; }

    auto World = ck::FEcsWorld{};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestNotNull(TEXT("fixture runs inside the automation editor world"), TestWorld)) { return false; }
    EntityA.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    EntityB.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    const auto AuthoritySettings = FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority};
    UCk_Utils_Net_UE::Add(EntityA, AuthoritySettings);
    UCk_Utils_Net_UE::Add(EntityB, AuthoritySettings);
    auto TagSetA = UCk_Utils_TagSet_UE::Add(EntityA, MakeTags({TagAlpha, TagBeta}), ECk_Replication::DoesNotReplicate);
    auto TagSetB = UCk_Utils_TagSet_UE::Add(EntityB, MakeTags({TagGamma}), ECk_Replication::DoesNotReplicate);
    if (NOT TestTrue(TEXT("fixture creates distinct non-replicated Tag Sets"),
        ck::IsValid(TagSetA) && ck::IsValid(TagSetB) && EntityA != EntityB)) { return false; }

    auto Inspector = FCkInspector_TagSet{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first build returns the authored Tag Set widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_TagSetAuthored")})
        || NOT TestEqual(TEXT("second build returns the authored Tag Set widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_TagSetAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const TSharedRef<SCkInspector_TagSetAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_TagSetAuthored>(RenderedA);
    const TSharedRef<SCkInspector_TagSetAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_TagSetAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> CollectionA = AuthoredA->Get_TagsCollection();
    TSharedPtr<FCkUiCollection> CollectionB = AuthoredB->Get_TagsCollection();
    if (NOT TestTrue(TEXT("each production build owns an independent authored view and tag collection"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && CollectionA.IsValid() && CollectionB.IsValid() && ViewA != ViewB && CollectionA != CollectionB
            && CollectionA->GetRecords().Num() == 2 && CollectionB->GetRecords().Num() == 1)) { return false; }
    TestEqual(TEXT("authored count A matches its Tag Set"), AuthoredA->Get_CountText(), FString{TEXT("2")});
    TestEqual(TEXT("authored count B matches its Tag Set"), AuthoredB->Get_CountText(), FString{TEXT("1")});
    TestTrue(TEXT("authored layout mounts the existing gameplay-tag entry control through its native port"),
        CountType(RenderedA, TEXT("SEditableTextBox")) >= 1 && CountType(RenderedB, TEXT("SEditableTextBox")) >= 1);
    TestTrue(TEXT("authored chip repeat binds the exact Tag Set records"),
        ContainsText(RenderedA, TagAlpha.ToString()) && ContainsText(RenderedA, TagBeta.ToString())
            && ContainsText(RenderedB, TagGamma.ToString()));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains the multi-select value and diff authority"),
        RowsA.FindRef(TEXT("Count:")) == TEXT("2 tags") && RowsB.FindRef(TEXT("Count:")) == TEXT("1 tags")
            && Differing.Contains(TEXT("Count:")) && Differing.Contains(TEXT("Tags:")));

    TSharedPtr<SCkInspector_TagSetAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_TagSetAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored stable rows receive exact native diff marks"),
        DiffAuthored.IsValid() && DiffAuthored->Is_CountDiffMarked() && DiffAuthored->Is_TagsDiffMarked());

    const TSharedPtr<SButton> HeldRemove = FindButtonBesideText(RenderedA, TagBeta.ToString(), TEXT("tag-set-remove"));
    if (NOT TestTrue(TEXT("authored repeat mounts a physical remove action"), HeldRemove.IsValid() && HeldRemove->IsEnabled()))
    { return false; }
    HeldRemove->SimulateClick();
    if (NOT TestTrue(TEXT("physical authored remove routes one authority-gated public request"),
        EntityA.Has<ck::FFragment_TagSet_Requests>()
            && EntityA.Get<ck::FFragment_TagSet_Requests>().Get_Requests().Num() == 1)) { return false; }
    const auto& Request = EntityA.Get<ck::FFragment_TagSet_Requests>().Get_Requests()[0];
    TestTrue(TEXT("routed request removes the stable-key tag selected by the repeat"),
        std::holds_alternative<FCk_Request_TagSet_RemoveTags>(Request)
            && std::get<FCk_Request_TagSet_RemoveTags>(Request).Get_TagsToRemove().Num() == 1
            && std::get<FCk_Request_TagSet_RemoveTags>(Request).Get_TagsToRemove().HasTagExact(TagBeta));
    EntityA.Try_Remove<ck::FFragment_TagSet_Requests>();

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Tag Set resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTagSet.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorTagSet.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns stable count, chip, remove, and native add placement"),
        Markup.Contains(TEXT("tag-set-count-label")) && Markup.Contains(TEXT(">Count:</text>"))
            && Markup.Contains(TEXT("tag-set-tags-label")) && Markup.Contains(TEXT(">Tags:</text>"))
            && Markup.Contains(TEXT("bind=\"tag-set-add-tag-port\""))
            && Markup.Contains(TEXT("bind=\"tag-set-tags\"")) && Markup.Contains(TEXT("<debug-inspector-action"))
            && Markup.Contains(TEXT("item-action=\"tag-set-remove\"")));

    {
        auto StyleInspector = FCkInspector_TagSet{};
        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
        const TSharedRef<SWidget> Hidden = StyleInspector.Build_Inspector(EntityB);
        TestTrue(TEXT("Hidden edit-control style removes authored Tag Set verbs and native editors"),
            CountType(Hidden, TEXT("SButton")) == 0 && CountType(Hidden, TEXT("SEditableTextBox")) == 0);
        StyleInspector.OnDeactivated();

        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::OnHover;
        const TSharedRef<SWidget> OnHover = StyleInspector.Build_Inspector(EntityB);
        OnHover->SlatePrepass();
        const TSharedPtr<SButton> HoverRemove = FindButtonBesideText(OnHover, TagGamma.ToString(), TEXT("tag-set-remove"));
        if (TestTrue(TEXT("On-Hover edit-control style retains the authored action"), HoverRemove.IsValid()))
        {
            TestEqual(TEXT("On-Hover authored action starts hidden until row hover"),
                HoverRemove->GetVisibility(), EVisibility::Hidden);
        }
        TestTrue(TEXT("On-Hover edit-control style retains the read-only action label"), ContainsText(OnHover, TEXT("Remove")));
        StyleInspector.OnDeactivated();
        StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;
    }

    TSharedPtr<SCkInspector_TagSetAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_TagSet>();
        const TSharedRef<SWidget> DestructorRendered = DestructorInspector->Build_Inspector(EntityB);
        if (NOT TestEqual(TEXT("destructor fixture mounts the authored Tag Set widget"),
            DestructorRendered->GetTypeAsString(), FString{TEXT("SCkInspector_TagSetAuthored")}))
        {
            AddError(DestructorInspector->Get_LastAuthoredLoadError());
            return false;
        }
        DestructorAuthored = StaticCastSharedRef<SCkInspector_TagSetAuthored>(DestructorRendered);
    }
    TestTrue(TEXT("inspector destruction makes its retained authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());
    DestructorAuthored.Reset();

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Tag Set A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedPtr<SButton> ReloadedRemove = FindButtonBesideText(
        RenderedA, TagBeta.ToString(), TEXT("tag-set-remove"));
    TestTrue(TEXT("compatible reload retains the physical authored remove action"),
        ReloadedRemove.IsValid() && ReloadedRemove == HeldRemove);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing native add port is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("<native id=\"tag-set-add-tag-native\" bind=\"tag-set-add-tag-port\" visible=\"tag-set-available\" class=\"tag-set-native-row\" />"), TEXT("")),
        Stylesheet, TEXT("Tag Set B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    TestTrue(TEXT("fixture removes the Tag Set fragment while the entity remains live"),
        EntityA.Try_Remove<ck::FFragment_TagSet>() && ck::IsValid(EntityA) && NOT Inspector.CanInspect(EntityA));
    HeldRemove->SlatePrepass();
    TestEqual(TEXT("mounted authored count fails closed after fragment removal"), AuthoredA->Get_CountText(), FString{TEXT("--")});
    TestFalse(TEXT("stale authored action is disabled"), HeldRemove->IsEnabled());
    TestTrue(TEXT("stale authored action explains its disabled state"),
        GetToolTipText(HeldRemove.ToSharedRef()).Contains(TEXT("Tag Set is unavailable")));
    HeldRemove->SimulateClick();
    TestFalse(TEXT("held authored action cannot enqueue against a stale Tag Set"), EntityA.Has<ck::FFragment_TagSet_Requests>());
    auto StaleRows = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); StaleRows = Capture.Get_Rows(); }
    TestTrue(TEXT("native row capture also fails closed after fragment removal"), StaleRows.IsEmpty());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained Tag Set build inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    TestTrue(TEXT("deactivation releases each authored build's view and collection ownership"),
        NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid() && NOT DiffAuthored->Get_View().IsValid()
            && NOT AuthoredA->Get_TagsCollection().IsValid() && NOT AuthoredB->Get_TagsCollection().IsValid()
            && NOT DiffAuthored->Get_TagsCollection().IsValid());
    ViewA.Reset(); ViewB.Reset(); CollectionA.Reset(); CollectionB.Reset(); DiffAuthored.Reset();
    TestTrue(TEXT("deactivation detaches every native Tag Set editor port"),
        CountType(RenderedA, TEXT("SEditableTextBox")) == 0 && CountType(RenderedB, TEXT("SEditableTextBox")) == 0);
    return true;
}

#endif
