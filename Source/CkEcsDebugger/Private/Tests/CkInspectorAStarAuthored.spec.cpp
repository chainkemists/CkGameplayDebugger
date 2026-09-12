#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_AStar.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"

// AStar configuration has no public constructor. The fixture owns synthetic, processor-free states.
#define private public
#include "CkAStar/CkAStar_Fragment.h"
#undef private

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ck_inspector_astar_authored_test
{
    struct FFixture final
    {
        FCk_Handle BothA;
        FCk_Handle BothB;
        FCk_Handle DebugOnly;
        FCk_Handle ParamsOnly;
    };

    auto Add_Params(FCk_Handle& InEntity, const int64 InBudgetUs, const int32 InMaxIterations,
        const float InCostThreshold) -> void
    {
        auto& Params = InEntity.Add<ck::FFragment_AStar_Params>();
        Params._BudgetMicroseconds = InBudgetUs;
        Params._MaxIterationsPerTick = InMaxIterations;
        Params._CostThreshold = InCostThreshold;
    }

    auto Add_Debug(FCk_Handle& InEntity, const int32 InOpenSet, const int32 InClosedSet,
        const int32 InIterations, const int64 InTimeUs, const float InBudgetPercent,
        const ECk_AStarSearchStatus InStatus) -> void
    {
        InEntity.Add<ck::FFragment_AStar_Debug>(
            InOpenSet, InClosedSet, InIterations, InTimeUs, InBudgetPercent, InStatus);
    }

    auto Create_Entity(ck::FEcsWorld& InWorld) -> FCk_Handle
    {
        return UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
    }

    auto CreateFixture(ck::FEcsWorld& InWorld, FFixture& OutFixture) -> bool
    {
        OutFixture.BothA = Create_Entity(InWorld);
        OutFixture.BothB = Create_Entity(InWorld);
        OutFixture.DebugOnly = Create_Entity(InWorld);
        OutFixture.ParamsOnly = Create_Entity(InWorld);
        if (ck::Is_NOT_Valid(OutFixture.BothA) || ck::Is_NOT_Valid(OutFixture.BothB)
            || ck::Is_NOT_Valid(OutFixture.DebugOnly) || ck::Is_NOT_Valid(OutFixture.ParamsOnly))
        { return false; }

        Add_Debug(OutFixture.BothA, 4, 7, 12, 321, 50.0f, ECk_AStarSearchStatus::InProgress);
        Add_Params(OutFixture.BothA, 250, 16, 8.5f);
        Add_Debug(OutFixture.BothB, 9, 11, 30, 654, 100.0f, ECk_AStarSearchStatus::CostThresholdReached);
        Add_Params(OutFixture.BothB, 0, 0, 0.0f);
        Add_Debug(OutFixture.DebugOnly, 1, 2, 3, 4, 80.0f, ECk_AStarSearchStatus::Complete);
        Add_Params(OutFixture.ParamsOnly, 1000, 64, 42.25f);
        return true;
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorAStarAuthored,
    "Ck.UiAuthoring.EcsDebugger.AStarInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorAStarAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_astar_authored_test;

    auto InvalidInspector = FCkInspector_AStar{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored AStar shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_AStarAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_AStarAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_AStarAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid authored AStar shell has no inspectable sections"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_HasSearch()
            && NOT InvalidAuthored->Get_HasParams() && InvalidAuthored->Get_StatusText() == TEXT("--"));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored AStar shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(TEXT("fixture creates both, Debug-only, and Params-only AStar states"),
        CreateFixture(World, Fixture))) { return false; }

    auto Inspector = FCkInspector_AStar{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.BothA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.BothB);
    if (NOT TestEqual(TEXT("first build returns authored AStar widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_AStarAuthored")})
        || NOT TestEqual(TEXT("second build returns authored AStar widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_AStarAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_AStarAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_AStarAuthored>(RenderedA);
    const TSharedRef<SCkInspector_AStarAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_AStarAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted AStar view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && AuthoredA->Get_HasSearch()
            && AuthoredA->Get_HasParams() && AuthoredB->Get_HasSearch() && AuthoredB->Get_HasParams()
            && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    TestTrue(TEXT("authored AStar rows project exact values"),
        AuthoredA->Get_StatusText() == TEXT("In Progress") && AuthoredA->Get_OpenSetText() == TEXT("4")
            && AuthoredA->Get_ClosedSetText() == TEXT("7") && AuthoredA->Get_IterationsText() == TEXT("12")
            && AuthoredA->Get_TimeText() == TEXT("321") && AuthoredA->Get_BudgetUsedText() == TEXT("50.0%")
            && FMath::IsNearlyEqual(AuthoredA->Get_BudgetFraction(), 0.5f)
            && AuthoredA->Get_BudgetText() == TEXT("250") && AuthoredA->Get_MaxIterationsText() == TEXT("16")
            && AuthoredA->Get_CostThresholdText() == TEXT("8.500")
            && AuthoredB->Get_StatusText() == TEXT("Cost Threshold Reached") && AuthoredB->Get_OpenSetText() == TEXT("9")
            && AuthoredB->Get_ClosedSetText() == TEXT("11") && AuthoredB->Get_IterationsText() == TEXT("30")
            && AuthoredB->Get_TimeText() == TEXT("654") && AuthoredB->Get_BudgetUsedText() == TEXT("100.0%")
            && FMath::IsNearlyEqual(AuthoredB->Get_BudgetFraction(), 1.0f)
            && AuthoredB->Get_BudgetText() == TEXT("0 (unbounded)")
            && AuthoredB->Get_MaxIterationsText() == TEXT("0 (unbounded)")
            && AuthoredB->Get_CostThresholdText() == TEXT("0 (disabled)"));
    TestTrue(TEXT("authored AStar status and budget colors retain their exact semantic tones"),
        AuthoredA->Get_StatusForeground() == CkStyle::GetToneColor(ECk_Tone::Info)
            && AuthoredA->Get_StatusBackground() == CkStyle::GetToneDimColor(ECk_Tone::Info)
            && AuthoredB->Get_StatusForeground() == CkStyle::GetToneColor(ECk_Tone::Warn)
            && AuthoredB->Get_StatusBackground() == CkStyle::GetToneDimColor(ECk_Tone::Warn)
            && AuthoredA->Get_BudgetFill() == CkStyle::GetToneColor(ECk_Tone::Info)
            && AuthoredB->Get_BudgetFill() == CkStyle::GetToneColor(ECk_Tone::Err));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.BothA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.BothB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native builder remains multi-selection authority for all nine AStar rows"),
        RowsA.FindRef(TEXT("Status:")) != RowsB.FindRef(TEXT("Status:"))
            && RowsA.FindRef(TEXT("Open Set:")) == TEXT("4") && RowsB.FindRef(TEXT("Open Set:")) == TEXT("9")
            && RowsA.FindRef(TEXT("Closed Set:")) == TEXT("7") && RowsB.FindRef(TEXT("Closed Set:")) == TEXT("11")
            && RowsA.FindRef(TEXT("Iterations (frame):")) == TEXT("12") && RowsB.FindRef(TEXT("Iterations (frame):")) == TEXT("30")
            && RowsA.FindRef(TEXT("Time (frame, us):")) == TEXT("321") && RowsB.FindRef(TEXT("Time (frame, us):")) == TEXT("654")
            && RowsA.FindRef(TEXT("Budget Used:")) == TEXT("50.0%") && RowsB.FindRef(TEXT("Budget Used:")) == TEXT("100.0%")
            && RowsA.FindRef(TEXT("Budget (us):")) == TEXT("250") && RowsB.FindRef(TEXT("Budget (us):")) == TEXT("0 (unbounded)")
            && RowsA.FindRef(TEXT("Max Iterations:")) == TEXT("16") && RowsB.FindRef(TEXT("Max Iterations:")) == TEXT("0 (unbounded)")
            && RowsA.FindRef(TEXT("Cost Threshold:")) == TEXT("8.500") && RowsB.FindRef(TEXT("Cost Threshold:")) == TEXT("0 (disabled)")
            && Differing.Num() == 9 && Differing.Contains(TEXT("Status:")) && Differing.Contains(TEXT("Open Set:"))
            && Differing.Contains(TEXT("Closed Set:")) && Differing.Contains(TEXT("Iterations (frame):"))
            && Differing.Contains(TEXT("Time (frame, us):")) && Differing.Contains(TEXT("Budget Used:"))
            && Differing.Contains(TEXT("Budget (us):")) && Differing.Contains(TEXT("Max Iterations:"))
            && Differing.Contains(TEXT("Cost Threshold:")));

    TSharedPtr<SCkInspector_AStarAuthored> DiffAuthored;
    { const auto DiffScope = FCkInspector_DiffMarkScope{&Differing}; DiffAuthored = StaticCastSharedRef<SCkInspector_AStarAuthored>(Inspector.Build_Inspector(Fixture.BothA)); }
    TestTrue(TEXT("all authored AStar labels receive exact diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_StatusDiffMarked() && DiffAuthored->Is_OpenSetDiffMarked()
        && DiffAuthored->Is_ClosedSetDiffMarked() && DiffAuthored->Is_IterationsDiffMarked()
        && DiffAuthored->Is_TimeDiffMarked() && DiffAuthored->Is_BudgetUsedDiffMarked()
        && DiffAuthored->Is_BudgetDiffMarked() && DiffAuthored->Is_MaxIterationsDiffMarked()
        && DiffAuthored->Is_CostThresholdDiffMarked());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed AStar resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAStar.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAStar.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns both headers, all AStar labels, and astar bindings"),
        Markup.Contains(TEXT(">Search</text>")) && Markup.Contains(TEXT(">Params</text>"))
            && Markup.Contains(TEXT(">Status:</text>")) && Markup.Contains(TEXT(">Open Set:</text>"))
            && Markup.Contains(TEXT(">Closed Set:</text>")) && Markup.Contains(TEXT(">Iterations (frame):</text>"))
            && Markup.Contains(TEXT(">Time (frame, us):</text>")) && Markup.Contains(TEXT(">Budget Used:</text>"))
            && Markup.Contains(TEXT(">Budget (us):</text>")) && Markup.Contains(TEXT(">Max Iterations:</text>"))
            && Markup.Contains(TEXT(">Cost Threshold:</text>")) && Markup.Contains(TEXT("astar-status"))
            && Markup.Contains(TEXT("astar-open-set")) && Markup.Contains(TEXT("astar-closed-set"))
            && Markup.Contains(TEXT("astar-iterations")) && Markup.Contains(TEXT("astar-time"))
            && Markup.Contains(TEXT("astar-budget-used")) && Markup.Contains(TEXT("astar-budget"))
            && Markup.Contains(TEXT("astar-max-iterations")) && Markup.Contains(TEXT("astar-cost-threshold")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by AStar A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("AStar A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing AStar binding is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"value\" bind=\"missing-binding\" /></region></ui>"),
        TEXT(""), TEXT("AStar B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TSharedPtr<SWidget> SearchSection = FindTaggedWidget(ViewA->GetRegion(TEXT("main")), FName{TEXT("astar-search-section")});
    const TSharedPtr<SWidget> ParamsSection = FindTaggedWidget(ViewA->GetRegion(TEXT("main")), FName{TEXT("astar-params-section")});
    if (NOT TestTrue(TEXT("authored AStar exposes both section nodes"), SearchSection.IsValid() && ParamsSection.IsValid())) { return false; }
    TestTrue(TEXT("both sections begin visible"), SearchSection->GetVisibility() == EVisibility::Visible
        && ParamsSection->GetVisibility() == EVisibility::Visible);
    TestTrue(TEXT("Debug removal succeeds for section fail-closed coverage"),
        Fixture.BothA.Try_Remove<ck::FFragment_AStar_Debug>());
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("Debug removal collapses Search and preserves Params"),
        NOT AuthoredA->Get_HasSearch() && AuthoredA->Get_HasParams()
            && SearchSection->GetVisibility() == EVisibility::Collapsed && ParamsSection->GetVisibility() == EVisibility::Visible
            && AuthoredA->Get_StatusText() == TEXT("--") && AuthoredA->Get_BudgetText() == TEXT("250"));
    TestTrue(TEXT("Params removal succeeds for final fail-closed coverage"),
        Fixture.BothA.Try_Remove<ck::FFragment_AStar_Params>());
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("removing both fragments collapses both sections and fails closed"),
        NOT Inspector.CanInspect(Fixture.BothA) && NOT AuthoredA->Get_HasSearch() && NOT AuthoredA->Get_HasParams()
            && SearchSection->GetVisibility() == EVisibility::Collapsed && ParamsSection->GetVisibility() == EVisibility::Collapsed
            && AuthoredA->Get_StatusText() == TEXT("--") && AuthoredA->Get_BudgetText() == TEXT("--"));

    const TSharedRef<SCkInspector_AStarAuthored> DebugOnly =
        StaticCastSharedRef<SCkInspector_AStarAuthored>(Inspector.Build_Inspector(Fixture.DebugOnly));
    const TSharedRef<SCkInspector_AStarAuthored> ParamsOnly =
        StaticCastSharedRef<SCkInspector_AStarAuthored>(Inspector.Build_Inspector(Fixture.ParamsOnly));
    TestTrue(TEXT("Debug-only and Params-only are intentional independently inspectable states"),
        Inspector.CanInspect(Fixture.DebugOnly) && Inspector.CanInspect(Fixture.ParamsOnly)
            && DebugOnly->Get_HasSearch() && NOT DebugOnly->Get_HasParams()
            && NOT ParamsOnly->Get_HasSearch() && ParamsOnly->Get_HasParams()
            && DebugOnly->Get_OpenSetText() == TEXT("1") && ParamsOnly->Get_BudgetText() == TEXT("1000"));
    const TSharedPtr<SWidget> DebugOnlySearchSection = FindTaggedWidget(
        DebugOnly->Get_View()->GetRegion(TEXT("main")), FName{TEXT("astar-search-section")});
    const TSharedPtr<SWidget> DebugOnlyParamsSection = FindTaggedWidget(
        DebugOnly->Get_View()->GetRegion(TEXT("main")), FName{TEXT("astar-params-section")});
    if (NOT TestTrue(TEXT("Debug-only authored AStar exposes both conditional section nodes"),
        DebugOnlySearchSection.IsValid() && DebugOnlyParamsSection.IsValid())) { return false; }
    DebugOnly->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("Debug-only authored AStar shows Search without requiring Params"),
        DebugOnlySearchSection->GetVisibility() == EVisibility::Visible
            && DebugOnlyParamsSection->GetVisibility() == EVisibility::Collapsed);

    TSharedPtr<SCkInspector_AStarAuthored> DestructorAuthored;
    { auto DestructorInspector = MakeUnique<FCkInspector_AStar>(); DestructorAuthored = StaticCastSharedRef<SCkInspector_AStarAuthored>(DestructorInspector->Build_Inspector(Fixture.BothB)); }
    TestTrue(TEXT("inspector destruction makes its authored AStar build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained AStar view inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && DebugOnly->Is_Inert() && ParamsOnly->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build AStar view"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
