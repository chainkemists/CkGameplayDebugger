#include "../../CkOptimizationDebugger_Module.h"
#include "CkOptimizationDebugger/Window/SCkOptimizationDebuggerWindow.h"
#include "CkOptimizationDebugger/Window/SCkPerfLabPage.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
struct FCkOptimizationDebuggerLifecycleTestAccess
{
    static auto Window(const FCkOptimizationDebuggerModule& InModule) -> TSharedPtr<SCkOptimizationDebuggerWindow> { return InModule._DebuggerWindow; }
    static auto Tab(const FCkOptimizationDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
    static auto PreExit(FCkOptimizationDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
    static auto Spawn(FCkOptimizationDebuggerModule& InModule) -> TSharedRef<SDockTab>
    { return InModule.OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{InModule._DebuggerTabName}}); }
    static auto StartScan(SCkOptimizationDebuggerWindow& InWindow) -> void { InWindow.DoStart_ProjectScan(); }
    static auto IsScanRunning(const SCkOptimizationDebuggerWindow& InWindow) -> bool { return InWindow._ProjectScanState.IsRunning; }
    static auto HasScanTimer(const SCkOptimizationDebuggerWindow& InWindow) -> bool { return InWindow._ProjectScanTimer.IsValid(); }
    static auto PerfLab(const SCkOptimizationDebuggerWindow& InWindow) -> TSharedPtr<SCkPerfLabPage> { return InWindow._PerfLabPage; }
    static auto IsReleased(const SCkOptimizationDebuggerWindow& InWindow) -> bool { return InWindow._PresentationReleased; }
    static auto IsPerfLabReleased(const SCkPerfLabPage& InPage) -> bool { return InPage._PresentationReleased; }
    static auto HasPerfLabTimers(const SCkPerfLabPage& InPage) -> bool
    { return InPage._PollTimer.IsValid() || InPage._HeatmapTimer.IsValid(); }
    static auto View(const SCkOptimizationDebuggerWindow& InWindow) -> TSharedPtr<FCkUiView> { return InWindow._AuthoredShellView; }
    static auto DashboardView(const SCkOptimizationDebuggerWindow& InWindow) -> TSharedPtr<FCkUiView> { return InWindow._AuthoredDashboardView; }
    static auto DashboardPort(const SCkOptimizationDebuggerWindow& InWindow) -> TSharedPtr<SVerticalBox> { return InWindow._DashboardBox; }
    static auto DashboardFailure(const SCkOptimizationDebuggerWindow& InWindow) -> const FString& { return InWindow._AuthoredDashboardLoadFailure; }
    static auto ExerciseMemoryMultiSelectionAndContextMenu(SCkOptimizationDebuggerWindow& InWindow) -> bool
    {
        if (!InWindow._MemoryList.IsValid()) { return false; }
        const auto First = MakeShared<FCkOptimizationDebugger_MemoryRow>();
        First->DisplayName = TEXT("First retained memory row");
        First->AssetPath = TEXT("/Game/Tests/First.First");
        const auto Second = MakeShared<FCkOptimizationDebugger_MemoryRow>();
        Second->DisplayName = TEXT("Second retained memory row");
        Second->AssetPath = TEXT("/Game/Tests/Second.Second");
        InWindow._MemoryItems = {First, Second};
        InWindow._MemoryList->RequestListRefresh();
        InWindow._MemoryList->ClearSelection();
        InWindow._MemoryList->SetItemSelection(First, true, ESelectInfo::Direct);
        InWindow._MemoryList->SetItemSelection(Second, true, ESelectInfo::Direct);
        return InWindow._MemoryList->GetSelectedItems().Num() == 2
            && InWindow.DoOnMemoryContextMenu().IsValid();
    }
    static auto Failure(const SCkOptimizationDebuggerWindow& InWindow) -> const FString& { return InWindow._AuthoredShellLoadFailure; }
    static auto Ports(const SCkOptimizationDebuggerWindow& InWindow) -> TArray<TSharedPtr<SWidget>> { return InWindow._PagePorts; }
    static auto Poll(SCkOptimizationDebuggerWindow& InWindow) -> void
    { InWindow.Poll_AuthoredShell(InWindow._NextAuthoredShellPollSeconds); }
    static auto HasAuthoredParents(const SCkOptimizationDebuggerWindow& InWindow) -> bool
    {
        for (const TSharedPtr<SWidget>& Port : InWindow._PagePorts)
        { if (!Port.IsValid() || !Port->GetParentWidget().IsValid()) { return false; } }
        return !InWindow._UsingNativeShellFallback;
    }
    static auto HasFallbackParents(const SCkOptimizationDebuggerWindow& InWindow) -> bool
    {
        if (!InWindow._UsingNativeShellFallback || InWindow._FallbackPageHosts.Num() != InWindow._PagePorts.Num()) { return false; }
        for (int32 Index = 0; Index < InWindow._PagePorts.Num(); ++Index)
        {
            if (!InWindow._PagePorts[Index].IsValid() || !InWindow._FallbackPageHosts[Index].IsValid()
                || InWindow._PagePorts[Index]->GetParentWidget().Get() != InWindow._FallbackPageHosts[Index].Get()) { return false; }
        }
        return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOptimizationDebuggerLifecycle,
    "Ck.OptimizationDebugger.Lifecycle.ReleasePresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebuggerLifecycle::RunTest(const FString&)
{
    FCkOptimizationDebuggerModule* Module = FModuleManager::LoadModulePtr<FCkOptimizationDebuggerModule>(TEXT("CkOptimizationDebugger"));
    if (NOT TestNotNull(TEXT("Optimization debugger module loads"), Module)) { return false; }
    if (NOT TestTrue(TEXT("Optimization lifecycle fixture requires Slate"), FSlateApplication::IsInitialized())) { return false; }

    const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();
    const FName HostTabId{TEXT("LevelEditor")};
    const FName FixtureTabId{TEXT("CkOptimizationLifecycleFixture")};
    if (NOT TestTrue(TEXT("Optimization lifecycle fixture has a stable Level Editor docking host"),
        TabManager->FindExistingLiveTab(FTabId{HostTabId}).IsValid())) { return false; }

    const auto DrainSlate = []()
    {
        FSlateApplication::Get().PumpMessages();
        FSlateApplication::Get().Tick();
    };
    ON_SCOPE_EXIT
    {
        if (const auto FixtureTab = TabManager->FindExistingLiveTab(FTabId{FixtureTabId}); FixtureTab.IsValid())
        { FixtureTab->RequestCloseTab(); }
        DrainSlate();
    };

    // A user layout may restore the production tab before automation starts. Detach any such presentation without
    // closing its top-level docking area, then exercise the production spawn/close callbacks in an isolated document tab.
    FCkOptimizationDebuggerLifecycleTestAccess::PreExit(*Module);
    const TSharedRef<SDockTab> SpawnedTab = FCkOptimizationDebuggerLifecycleTestAccess::Spawn(*Module);
    TabManager->InsertNewDocumentTab(
        HostTabId, FixtureTabId, FTabManager::FLiveTabSearch{HostTabId}, SpawnedTab);
    const TSharedPtr<SCkOptimizationDebuggerWindow> Window = FCkOptimizationDebuggerLifecycleTestAccess::Window(*Module);
    const TSharedPtr<SDockTab> Tab = FCkOptimizationDebuggerLifecycleTestAccess::Tab(*Module);
    if (NOT TestTrue(TEXT("real module creates window and tab"), Window.IsValid() && Tab.IsValid())) { return false; }
    // Exercise the cancellation path before the authored-shell checks: this proves close owns the
    // Optimization scan timer, while the held PerfLab page below proves its independent timers are released too.
    const TSharedPtr<SCkPerfLabPage> HeldPerfLab = FCkOptimizationDebuggerLifecycleTestAccess::PerfLab(*Window);
    FCkOptimizationDebuggerLifecycleTestAccess::StartScan(*Window);
    TestTrue(TEXT("lifecycle fixture starts the real project scan before close"),
        FCkOptimizationDebuggerLifecycleTestAccess::IsScanRunning(*Window)
        && FCkOptimizationDebuggerLifecycleTestAccess::HasScanTimer(*Window));
    const TSharedPtr<FCkUiView> View = FCkOptimizationDebuggerLifecycleTestAccess::View(*Window);
    if (NOT TestTrue(TEXT("real module mounts the installed authored Optimization shell"),
        View.IsValid() && View->GetLastResult().Succeeded && FCkOptimizationDebuggerLifecycleTestAccess::HasAuthoredParents(*Window)))
    { AddError(FCkOptimizationDebuggerLifecycleTestAccess::Failure(*Window)); return false; }
    const TSharedPtr<FCkUiView> DashboardView = FCkOptimizationDebuggerLifecycleTestAccess::DashboardView(*Window);
    const TSharedPtr<SVerticalBox> DashboardPort = FCkOptimizationDebuggerLifecycleTestAccess::DashboardPort(*Window);
    if (NOT TestTrue(TEXT("installed Dashboard authored presentation mounts its collection view and specialist port"),
        DashboardView.IsValid() && DashboardPort.IsValid()))
    {
        AddError(FCkOptimizationDebuggerLifecycleTestAccess::DashboardFailure(*Window));
        return false;
    }
    if (NOT TestTrue(TEXT("installed Dashboard authored presentation is accepted and mounted"),
        DashboardView->GetLastResult().Succeeded && DashboardPort->GetParentWidget().IsValid()))
    {
        AddError(FString::Join(DashboardView->GetLastResult().Errors, TEXT("\n")));
        return false;
    }
    TestTrue(TEXT("native Memory table retains multi-selection and its context menu"),
        FCkOptimizationDebuggerLifecycleTestAccess::ExerciseMemoryMultiSelectionAndContextMenu(*Window));
    const TArray<TSharedPtr<SWidget>> ProductionPorts = FCkOptimizationDebuggerLifecycleTestAccess::Ports(*Window);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const int64 Revision = View->GetRevision();
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Css;
    if (NOT TestTrue(TEXT("installed Optimization shell assets are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/OptimizationDebugger.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/OptimizationDebugger.ui.css"))))) { return false; }
    FString DashboardMarkup, DashboardCss;
    if (!TestTrue(TEXT("installed Dashboard authored resources are readable"),
        FFileHelper::LoadFileToString(DashboardMarkup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/OptimizationDebuggerDashboard.ui.html")))
        && FFileHelper::LoadFileToString(DashboardCss, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/OptimizationDebuggerDashboard.ui.css"))))) { return false; }
    const TSharedRef<SWidget> DashboardMain = DashboardView->GetRegion(TEXT("main"));
    const int64 DashboardRevision = DashboardView->GetRevision();
    const FCkUiLoadResult DashboardCompatible = DashboardView->TryReload(DashboardMarkup, DashboardCss, TEXT("Optimization Dashboard compatible candidate"));
    const FCkUiLoadResult DashboardMissingCollection = DashboardView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><repeat id=\"bad\" bind=\"missing-dashboard-records\"><text id=\"entry\" bind-field=\"label\"/></repeat></region></ui>"),
        TEXT(""), TEXT("Optimization Dashboard missing collection candidate"));
    TestTrue(TEXT("Dashboard compatible reload retains its explicit specialist port and rejects a missing collection atomically"),
        DashboardCompatible.Succeeded && DashboardView->GetRevision() > DashboardRevision
        && DashboardView->GetRegion(TEXT("main")) == DashboardMain && DashboardPort->GetParentWidget().IsValid()
        && !DashboardMissingCollection.Succeeded && DashboardView->GetRegion(TEXT("main")) == DashboardMain);
    const FCkUiLoadResult Compatible = View->TryReload(Markup, Css, TEXT("Optimization compatible shell candidate"));
    TestTrue(TEXT("compatible reload preserves seven native page controls and active mount"), Compatible.Succeeded
        && View->GetRevision() > Revision && View->GetRegion(TEXT("main")) == Main
        && FCkOptimizationDebuggerLifecycleTestAccess::Ports(*Window) == ProductionPorts
        && FCkOptimizationDebuggerLifecycleTestAccess::HasAuthoredParents(*Window));
    const int64 RejectedRevision = View->GetRevision();
    const FCkUiLoadResult MissingPort = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing-optimization-port\"/></region></ui>"), TEXT(""), TEXT("Optimization missing port candidate"));
    const FCkUiLoadResult MissingField = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><tabs id=\"bad\" value-bind=\"missing-optimization-field\"><tab id=\"dashboard\" key=\"Dashboard\" label=\"Dashboard\"><native id=\"page\" bind=\"optimization-dashboard-page\"/></tab></tabs></region></ui>"), TEXT(""), TEXT("Optimization missing field candidate"));
    const FCkUiLoadResult MissingAction = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><tabs id=\"bad\" value-bind=\"optimization-page\" changed=\"missing-optimization-action\"><tab id=\"dashboard\" key=\"Dashboard\" label=\"Dashboard\"><native id=\"page\" bind=\"optimization-dashboard-page\"/></tab></tabs></region></ui>"), TEXT(""), TEXT("Optimization missing action candidate"));
    TestTrue(TEXT("malformed port, field and action reloads reject atomically"), !MissingPort.Succeeded && !MissingField.Succeeded && !MissingAction.Succeeded
        && View->GetRevision() == RejectedRevision && View->GetRegion(TEXT("main")) == Main
        && FCkOptimizationDebuggerLifecycleTestAccess::Ports(*Window) == ProductionPorts
        && FCkOptimizationDebuggerLifecycleTestAccess::HasAuthoredParents(*Window));

    if (NOT TestTrue(TEXT("Optimization authored recovery requires Slate"), FSlateApplication::IsInitialized())) { return false; }
    const FString RecoveryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OptimizationAuthoredRecovery"));
    IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RecoveryDirectory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true); };
    const FString RecoveryMarkup = FPaths::Combine(RecoveryDirectory, TEXT("OptimizationDebugger.ui.html"));
    const FString RecoveryCss = FPaths::Combine(RecoveryDirectory, TEXT("OptimizationDebugger.ui.css"));
    if (NOT TestTrue(TEXT("malformed Optimization recovery fixture writes"),
        FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\">"), *RecoveryMarkup)
        && FFileHelper::SaveStringToFile(Css, *RecoveryCss))) { return false; }
    const TSharedRef<SCkOptimizationDebuggerWindow> RecoveryWindow = SNew(SCkOptimizationDebuggerWindow)
        .TestResourceDirectory(RecoveryDirectory);
    const TSharedRef<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D{1280.0f, 900.0f})[RecoveryWindow];
    ON_SCOPE_EXIT { FSlateApplication::Get().DestroyWindowImmediately(RecoveryHost); };
    FSlateApplication::Get().AddWindow(RecoveryHost, true);
    FSlateApplication::Get().PumpMessages(); FSlateApplication::Get().Tick(); FSlateApplication::Get().Tick();
    const TArray<TSharedPtr<SWidget>> RecoveryPorts = FCkOptimizationDebuggerLifecycleTestAccess::Ports(*RecoveryWindow);
    TestTrue(TEXT("malformed startup retains every native page in fallback"), FCkOptimizationDebuggerLifecycleTestAccess::HasFallbackParents(*RecoveryWindow));
    if (NOT TestTrue(TEXT("same Optimization resource paths are repaired"),
        FFileHelper::SaveStringToFile(Markup, *RecoveryMarkup) && FFileHelper::SaveStringToFile(Css, *RecoveryCss))) { return false; }
    FCkOptimizationDebuggerLifecycleTestAccess::Poll(*RecoveryWindow);
    TestTrue(TEXT("same-path recovery remounts seven identical native page ports"),
        FCkOptimizationDebuggerLifecycleTestAccess::View(*RecoveryWindow).IsValid()
        && FCkOptimizationDebuggerLifecycleTestAccess::View(*RecoveryWindow)->GetLastResult().Succeeded
        && FCkOptimizationDebuggerLifecycleTestAccess::Ports(*RecoveryWindow) == RecoveryPorts
        && FCkOptimizationDebuggerLifecycleTestAccess::HasAuthoredParents(*RecoveryWindow));
    Module->CloseDebugger();
    TestTrue(TEXT("ordinary close detaches tab and releases held page"), !FCkOptimizationDebuggerLifecycleTestAccess::Window(*Module).IsValid()
        && Tab->GetContent() == SNullWidget::NullWidget && HeldPerfLab.IsValid()
        && FCkOptimizationDebuggerLifecycleTestAccess::IsReleased(*Window)
        && !FCkOptimizationDebuggerLifecycleTestAccess::IsScanRunning(*Window)
        && !FCkOptimizationDebuggerLifecycleTestAccess::HasScanTimer(*Window)
        && FCkOptimizationDebuggerLifecycleTestAccess::IsPerfLabReleased(*HeldPerfLab)
        && !FCkOptimizationDebuggerLifecycleTestAccess::HasPerfLabTimers(*HeldPerfLab));
    Window->Release_Presentation();
    TestTrue(TEXT("retained window release is idempotent"), Tab->GetContent() == SNullWidget::NullWidget);
    DrainSlate();
    const TSharedRef<SDockTab> RespawnedTab = FCkOptimizationDebuggerLifecycleTestAccess::Spawn(*Module);
    TabManager->InsertNewDocumentTab(
        HostTabId, FixtureTabId, FTabManager::FLiveTabSearch{HostTabId}, RespawnedTab);
    const TSharedPtr<SDockTab> Reopened = FCkOptimizationDebuggerLifecycleTestAccess::Tab(*Module);
    const TSharedPtr<SCkOptimizationDebuggerWindow> ReopenedWindow = FCkOptimizationDebuggerLifecycleTestAccess::Window(*Module);
    TestTrue(TEXT("reopen remounts a fresh authored shell after the close release"), ReopenedWindow.IsValid()
        && FCkOptimizationDebuggerLifecycleTestAccess::View(*ReopenedWindow).IsValid()
        && FCkOptimizationDebuggerLifecycleTestAccess::View(*ReopenedWindow)->GetLastResult().Succeeded
        && FCkOptimizationDebuggerLifecycleTestAccess::HasAuthoredParents(*ReopenedWindow));
    FCkOptimizationDebuggerLifecycleTestAccess::PreExit(*Module);
    TestTrue(TEXT("pre-exit detaches reopened tab"), !FCkOptimizationDebuggerLifecycleTestAccess::Window(*Module).IsValid()
        && Reopened.IsValid() && Reopened->GetContent() == SNullWidget::NullWidget);
    return true;
}
#endif
