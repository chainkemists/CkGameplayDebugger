#include "Window/SCkDebuggerLauncher.h"
#include "Window/SCkDebuggerSuiteWindow.h"
#include "../../CkDebuggerLauncher_Module.h"

#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkDebuggerLauncherAuthoredTestAccess
{
    static auto GetSearch(const TSharedPtr<SCkDebuggerLauncher>& InLauncher) -> TSharedPtr<SCkDebug_SearchBar>
    { return InLauncher.IsValid() ? InLauncher->_SearchBar : nullptr; }
    static auto GetResults(const TSharedPtr<SCkDebuggerLauncher>& InLauncher) -> TSharedPtr<SScrollBox>
    { return InLauncher.IsValid() ? InLauncher->_ResultsScroll : nullptr; }
    static auto Poll(const TSharedPtr<SCkDebuggerLauncher>& InLauncher) -> void
    { if (InLauncher.IsValid()) { InLauncher->Poll_AuthoredShell(InLauncher->_NextAuthoredShellPollSeconds); } }
};

struct FCkDebuggerLauncherModuleAuthoredTestAccess
{
    static auto GetWindow(FCkDebuggerLauncherModule& InModule) -> TSharedPtr<SCkDebuggerLauncher> { return InModule._LauncherWindow; }
    static auto GetTab(FCkDebuggerLauncherModule& InModule) -> TSharedPtr<SDockTab> { return InModule._LauncherTab; }
    static auto GetSuiteWindow(FCkDebuggerLauncherModule& InModule) -> TSharedPtr<SCkDebuggerSuiteWindow> { return InModule._SuiteWindow; }
    static auto GetSuiteTab(FCkDebuggerLauncherModule& InModule) -> TSharedPtr<SDockTab> { return InModule._SuiteTab; }
    static auto PreExit(FCkDebuggerLauncherModule& InModule) -> void { InModule.HandleEnginePreExit(); }
};

struct FCkDebuggerSuiteAuthoredTestAccess
{
    static auto GetRail(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite) -> TSharedPtr<SCkDebuggerLauncher>
    { return InSuite.IsValid() ? InSuite->_Rail : nullptr; }

    static auto GetContentHost(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite) -> TSharedPtr<SBox>
    { return InSuite.IsValid() ? InSuite->_ContentHost : nullptr; }

    static auto Select(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite, FName InTabId) -> void
    {
        if (InSuite.IsValid())
        { InSuite->Handle_ToolSelected(InTabId); }
    }

    static auto IsEmbedded(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite, FName InTabId) -> bool
    { return InSuite.IsValid() && InSuite->_EmbeddedTools.Contains(InTabId); }

    static auto GetSelected(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite) -> FName
    { return InSuite.IsValid() ? InSuite->_SelectedToolId : NAME_None; }
    static auto Poll(const TSharedPtr<SCkDebuggerSuiteWindow>& InSuite) -> void
    { if (InSuite.IsValid()) { InSuite->Poll_AuthoredShell(InSuite->_NextAuthoredShellPollSeconds); } }
};

namespace ck_debugger_launcher_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }
    auto FindTagged(const TSharedRef<SWidget>& Root, const FName Tag) -> TSharedPtr<SWidget>
    {
        if (Root->GetTag() == Tag) { return Root; }
        const FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (const TSharedPtr<SWidget> Found = FindTagged(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Tag)) { return Found; } }
        return {};
    }
    auto ReadInstalledPair(const TCHAR* InBaseName, FString& OutMarkup, FString& OutCss) -> bool
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (!Plugin.IsValid()) { return false; }
        const FString Ui = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        const FString BaseName{InBaseName};
        return FFileHelper::LoadFileToString(OutMarkup, *FPaths::Combine(Ui, BaseName + TEXT(".ui.html")))
            && FFileHelper::LoadFileToString(OutCss, *FPaths::Combine(Ui, BaseName + TEXT(".ui.css")));
    }

    auto ReadInstalled(FString& OutMarkup, FString& OutCss) -> bool
    { return ReadInstalledPair(TEXT("DebuggerLauncherShell"), OutMarkup, OutCss); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerLauncherAuthoredShell, "Ck.DebuggerLauncher.Authored.ShellRecoveryAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebuggerLauncherAuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_debugger_launcher_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Launcher authored-shell test requires Slate.")); return false; }
    FString Markup, Css;
    if (!TestTrue(TEXT("Installed launcher shell resources are readable"), ReadInstalled(Markup, Css))) { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };
    TSharedPtr<SCkDebuggerLauncher> Launcher = SNew(SCkDebuggerLauncher);
    Host = SNew(SWindow).ClientSize(FVector2D{300, 600}).CreateTitleBar(false).HasCloseButton(false)[Launcher.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true); Tick(Slate);
    const TSharedPtr<FCkUiView> View = Launcher->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Installed launcher shell admits one retained authored view"),
        View.IsValid() && View->GetLastResult().Succeeded && !Launcher->IsUsingNativeShellFallback()))
    {
        AddError(Launcher->Get_AuthoredShellLoadFailure());
        return false;
    }
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    for (const FName Id : {FName(TEXT("debugger-launcher-shell")), FName(TEXT("debugger-launcher-suite")), FName(TEXT("debugger-launcher-filter")), FName(TEXT("debugger-launcher-results"))})
    { TestTrue(*FString::Printf(TEXT("Authored launcher topology owns '%s'"), *Id.ToString()), FindTagged(Main, Id).IsValid()); }
    const TSharedPtr<SCkDebug_SearchBar> HeldSearch = FCkDebuggerLauncherAuthoredTestAccess::GetSearch(Launcher);
    const TSharedPtr<SScrollBox> HeldResults = FCkDebuggerLauncherAuthoredTestAccess::GetResults(Launcher);
    const int64 Revision = View->GetRevision();
    TestTrue(TEXT("Compatible launcher reload succeeds"), View->TryReload(Markup, Css, TEXT("launcher compatible candidate")).Succeeded);
    TestTrue(TEXT("Compatible reload preserves native mechanics identities"), View->GetRevision() > Revision
        && HeldSearch == FCkDebuggerLauncherAuthoredTestAccess::GetSearch(Launcher) && HeldResults == FCkDebuggerLauncherAuthoredTestAccess::GetResults(Launcher));
    const TSharedRef<SWidget> MainBeforeReject = View->GetRegion(TEXT("main"));
    TestFalse(TEXT("Missing port rejects atomically"), View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing\"/></region></ui>"), TEXT(""), TEXT("launcher missing port")).Succeeded);
    TestFalse(TEXT("Missing action rejects atomically"), View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><button id=\"bad\" action=\"missing\">bad</button></region></ui>"), TEXT(""), TEXT("launcher missing action")).Succeeded);
    TestTrue(TEXT("Rejected candidates retain the committed tree"), View->GetRegion(TEXT("main")) == MainBeforeReject);

    const FString Temp = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/DebuggerLauncherRecovery"));
    IFileManager::Get().MakeDirectory(*Temp, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Temp, false, true); };
    FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing\"/></region></ui>"), *FPaths::Combine(Temp, TEXT("DebuggerLauncherShell.ui.html")));
    FFileHelper::SaveStringToFile(TEXT(""), *FPaths::Combine(Temp, TEXT("DebuggerLauncherShell.ui.css")));
    TSharedPtr<SCkDebuggerLauncher> Recovery = SNew(SCkDebuggerLauncher).ResourceDirectoryOverride(Temp);
    TSharedPtr<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D{300, 600})[Recovery.ToSharedRef()];
    Slate.AddWindow(RecoveryHost.ToSharedRef(), true); Tick(Slate);
    const TSharedPtr<SCkDebug_SearchBar> RecoverySearch = FCkDebuggerLauncherAuthoredTestAccess::GetSearch(Recovery);
    const TSharedPtr<SScrollBox> RecoveryResults = FCkDebuggerLauncherAuthoredTestAccess::GetResults(Recovery);
    TestTrue(TEXT("Malformed same-path startup retains native fallback and candidate"), Recovery->IsUsingNativeShellFallback() && Recovery->Get_AuthoredShellView().IsValid());
    FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Temp, TEXT("DebuggerLauncherShell.ui.html")));
    FFileHelper::SaveStringToFile(Css, *FPaths::Combine(Temp, TEXT("DebuggerLauncherShell.ui.css")));
    FCkDebuggerLauncherAuthoredTestAccess::Poll(Recovery);
    TestTrue(TEXT("Same-path repair atomically admits authored shell"), !Recovery->IsUsingNativeShellFallback());
    TestTrue(TEXT("Same-path repair preserves exact native port identities and a live parent"), RecoverySearch == FCkDebuggerLauncherAuthoredTestAccess::GetSearch(Recovery)
        && RecoveryResults == FCkDebuggerLauncherAuthoredTestAccess::GetResults(Recovery) && RecoverySearch->GetParentWidget().IsValid() && RecoveryResults->GetParentWidget().IsValid());
    Slate.DestroyWindowImmediately(RecoveryHost.ToSharedRef());
    RecoveryHost.Reset(); Recovery.Reset();
    Launcher->Release_Presentation(); Launcher->Release_Presentation();
    TestFalse(TEXT("Idempotent owner release drops production view"), Launcher->Get_AuthoredShellView().IsValid());
    TestTrue(TEXT("Held view remains safe and dispatch-gated after release"), View->TryReload(Markup, Css, TEXT("launcher held inert reload")).Succeeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerLauncherAuthoredModuleLifecycle, "Ck.DebuggerLauncher.Authored.ModuleCloseReopenPreExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebuggerLauncherAuthoredModuleLifecycle::RunTest(const FString&) -> bool
{
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Launcher lifecycle test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FCkDebuggerLauncherModule& Module = FCkDebuggerLauncherModule::Get();
    Module.CloseLauncher(); Module.OpenLauncher();
    ck_debugger_launcher_authored_tests::Tick(Slate);
    TSharedPtr<SCkDebuggerLauncher> First = FCkDebuggerLauncherModuleAuthoredTestAccess::GetWindow(Module);
    TSharedPtr<SDockTab> HeldTab = FCkDebuggerLauncherModuleAuthoredTestAccess::GetTab(Module);
    TSharedPtr<FCkUiView> HeldView = First.IsValid() ? First->Get_AuthoredShellView() : nullptr;
    if (!TestTrue(TEXT("Loaded launcher module opens a real production tab"), Module.IsLauncherOpen() && HeldTab.IsValid() && HeldView.IsValid())) { return false; }
    Module.CloseLauncher(); ck_debugger_launcher_authored_tests::Tick(Slate);
    TestFalse(TEXT("Ordinary close releases module launcher state"), Module.IsLauncherOpen());
    TestTrue(TEXT("Ordinary close empties held tab content"), HeldTab->GetContent() == SNullWidget::NullWidget);
    Module.OpenLauncher(); ck_debugger_launcher_authored_tests::Tick(Slate);
    if (NOT TestTrue(TEXT("Launcher reopens after ordinary close"), Module.IsLauncherOpen())) { return false; }
    HeldTab = FCkDebuggerLauncherModuleAuthoredTestAccess::GetTab(Module);
    if (NOT TestTrue(TEXT("Launcher reopen retains a tab for pre-exit validation"), HeldTab.IsValid())) { return false; }
    FCkDebuggerLauncherModuleAuthoredTestAccess::PreExit(Module); ck_debugger_launcher_authored_tests::Tick(Slate);
    TestFalse(TEXT("Private pre-exit releases module launcher state"), Module.IsLauncherOpen());
    TestTrue(TEXT("Private pre-exit empties held tab content"), HeldTab->GetContent() == SNullWidget::NullWidget);
    FString Markup, Css;
    if (TestTrue(TEXT("Held-view inert reload resources are readable"), ck_debugger_launcher_authored_tests::ReadInstalled(Markup, Css)))
    { TestTrue(TEXT("Held view remains safe after close and pre-exit"), HeldView->TryReload(Markup, Css, TEXT("launcher held lifecycle reload")).Succeeded); }
    HeldView.Reset(); HeldTab.Reset(); First.Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerSuiteAuthoredShell, "Ck.DebuggerLauncher.Authored.SuiteShellRecoveryAndOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebuggerSuiteAuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_debugger_launcher_authored_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Suite authored-shell test requires Slate."));
        return false;
    }

    FString Markup;
    FString Css;
    if (NOT TestTrue(TEXT("Installed suite shell resources are readable"),
        ReadInstalledPair(TEXT("DebuggerSuiteShell"), Markup, Css)))
    { return false; }

    constexpr auto TestTabId = TEXT("CkDebuggerSuiteAuthoredTestTool");
    int32 EmbeddedCloseCount = 0;
    auto Descriptor = FCkDebuggerToolDescriptor{
        TEXT("CkDebuggerLauncher"),
        TestTabId,
        FText::FromString(TEXT("Suite Test Tool")),
        FText::FromString(TEXT("Suite authored ownership fixture")),
        ECk_Icon::Diagnostics,
        ECkDebuggerToolCategory::Tools,
        TNumericLimits<int32>::Max()};
    Descriptor.Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([&EmbeddedCloseCount]()
    {
        return SNew(SDockTab)
            .OnTabClosed_Lambda([&EmbeddedCloseCount](TSharedRef<SDockTab>) { ++EmbeddedCloseCount; })
            [SNew(STextBlock).Text(FText::FromString(TEXT("Retained suite tool content")))];
    }));

    FCkDebuggerToolRegistry& ToolRegistry = FCkDebuggerToolRegistry::Get();
    const uint64 RegistrationId = ToolRegistry.Register(MoveTemp(Descriptor));
    if (NOT TestTrue(TEXT("Suite fixture registers a complete launchable descriptor"), RegistrationId != 0))
    { return false; }
    ON_SCOPE_EXIT { ToolRegistry.Unregister(TestTabId, RegistrationId); };

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT
    {
        if (Host.IsValid())
        { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
    };

    TSharedPtr<SCkDebuggerSuiteWindow> Suite = SNew(SCkDebuggerSuiteWindow);
    Host = SNew(SWindow)
        .ClientSize(FVector2D{900.0f, 600.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Suite.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    const TSharedPtr<FCkUiView> View = Suite->Get_AuthoredShellView();
    if (NOT TestTrue(TEXT("Installed suite shell admits one retained authored view"),
        View.IsValid() && View->GetLastResult().Succeeded && NOT Suite->IsUsingNativeShellFallback()))
    {
        AddError(Suite->Get_AuthoredShellLoadFailure());
        return false;
    }

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    for (const FName Id : {
        FName{TEXT("debugger-suite-shell")},
        FName{TEXT("debugger-suite-rail")},
        FName{TEXT("debugger-suite-content")},
        FName{TEXT("debugger-suite-header")},
        FName{TEXT("debugger-suite-title")},
        FName{TEXT("debugger-suite-popout")},
        FName{TEXT("debugger-suite-tool-content")}})
    {
        TestTrue(
            *FString::Printf(TEXT("Authored suite topology owns '%s'"), *Id.ToString()),
            FindTagged(Main, Id).IsValid());
    }

    const TSharedPtr<SCkDebuggerLauncher> HeldRail = FCkDebuggerSuiteAuthoredTestAccess::GetRail(Suite);
    const TSharedPtr<SBox> HeldContentHost = FCkDebuggerSuiteAuthoredTestAccess::GetContentHost(Suite);
    TestTrue(TEXT("Suite native mechanics ports have exactly one authored parent"),
        HeldRail.IsValid() && HeldRail->GetParentWidget().IsValid()
        && HeldContentHost.IsValid() && HeldContentHost->GetParentWidget().IsValid());

    FCkDebuggerSuiteAuthoredTestAccess::Select(Suite, TestTabId);
    TestTrue(TEXT("Production suite selection retains the factory-built dock tab"),
        FCkDebuggerSuiteAuthoredTestAccess::IsEmbedded(Suite, TestTabId));

    const int64 Revision = View->GetRevision();
    TestTrue(TEXT("Compatible suite reload succeeds"),
        View->TryReload(Markup, Css, TEXT("suite compatible candidate")).Succeeded);
    TestTrue(TEXT("Compatible suite reload preserves exact rail and content-host identities"),
        View->GetRevision() > Revision
        && HeldRail == FCkDebuggerSuiteAuthoredTestAccess::GetRail(Suite)
        && HeldContentHost == FCkDebuggerSuiteAuthoredTestAccess::GetContentHost(Suite));

    const TSharedRef<SWidget> MainBeforeReject = View->GetRegion(TEXT("main"));
    TestFalse(TEXT("Suite candidate with a missing native port rejects atomically"),
        View->TryReload(
            TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing\"/></region></ui>"),
            TEXT(""),
            TEXT("suite missing port")).Succeeded);
    TestFalse(TEXT("Suite candidate with a missing action rejects atomically"),
        View->TryReload(
            TEXT("<ui version=\"1\"><region name=\"main\"><button id=\"bad\" action=\"missing\">bad</button></region></ui>"),
            TEXT(""),
            TEXT("suite missing action")).Succeeded);
    TestTrue(TEXT("Rejected suite candidates retain the committed tree"),
        View->GetRegion(TEXT("main")) == MainBeforeReject);

    const FString Temp = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/DebuggerSuiteRecovery"));
    IFileManager::Get().MakeDirectory(*Temp, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Temp, false, true); };
    FFileHelper::SaveStringToFile(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"missing\"/></region></ui>"),
        *FPaths::Combine(Temp, TEXT("DebuggerSuiteShell.ui.html")));
    FFileHelper::SaveStringToFile(TEXT(""), *FPaths::Combine(Temp, TEXT("DebuggerSuiteShell.ui.css")));

    TSharedPtr<SCkDebuggerSuiteWindow> Recovery = SNew(SCkDebuggerSuiteWindow).ResourceDirectoryOverride(Temp);
    TSharedPtr<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D{900.0f, 600.0f})[Recovery.ToSharedRef()];
    Slate.AddWindow(RecoveryHost.ToSharedRef(), true);
    Tick(Slate);
    const TSharedPtr<SCkDebuggerLauncher> RecoveryRail = FCkDebuggerSuiteAuthoredTestAccess::GetRail(Recovery);
    const TSharedPtr<SBox> RecoveryContent = FCkDebuggerSuiteAuthoredTestAccess::GetContentHost(Recovery);
    TestTrue(TEXT("Malformed same-path suite startup retains native fallback and candidate"),
        Recovery->IsUsingNativeShellFallback() && Recovery->Get_AuthoredShellView().IsValid());
    TestTrue(TEXT("Fallback owns both suite mechanics ports exactly once"),
        RecoveryRail.IsValid() && RecoveryRail->GetParentWidget().IsValid()
        && RecoveryContent.IsValid() && RecoveryContent->GetParentWidget().IsValid());

    FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Temp, TEXT("DebuggerSuiteShell.ui.html")));
    FFileHelper::SaveStringToFile(Css, *FPaths::Combine(Temp, TEXT("DebuggerSuiteShell.ui.css")));
    FCkDebuggerSuiteAuthoredTestAccess::Poll(Recovery);
    TestTrue(TEXT("Same-path suite repair atomically admits authored shell"),
        NOT Recovery->IsUsingNativeShellFallback());
    TestTrue(TEXT("Suite repair preserves exact port identities and sole live parentage"),
        RecoveryRail == FCkDebuggerSuiteAuthoredTestAccess::GetRail(Recovery)
        && RecoveryContent == FCkDebuggerSuiteAuthoredTestAccess::GetContentHost(Recovery)
        && RecoveryRail->GetParentWidget().IsValid()
        && RecoveryContent->GetParentWidget().IsValid());
    Slate.DestroyWindowImmediately(RecoveryHost.ToSharedRef());
    RecoveryHost.Reset();
    Recovery.Reset();

    const FName SelectedBeforeRelease = FCkDebuggerSuiteAuthoredTestAccess::GetSelected(Suite);
    Suite->Release_Presentation();
    Suite->Release_Presentation();
    TestFalse(TEXT("Idempotent suite owner release drops production view"),
        Suite->Get_AuthoredShellView().IsValid());
    FCkDebuggerSuiteAuthoredTestAccess::Select(Suite, NAME_None);
    TestEqual(TEXT("Held suite selection route is inert after release"),
        FCkDebuggerSuiteAuthoredTestAccess::GetSelected(Suite), SelectedBeforeRelease);
    TestTrue(TEXT("Held suite view reload stays safe after release"),
        View->TryReload(Markup, Css, TEXT("suite held inert reload")).Succeeded);

    Suite->Release_AllEmbeddedTools(true);
    TestFalse(TEXT("Suite release drops the cached embedded tab"),
        FCkDebuggerSuiteAuthoredTestAccess::IsEmbedded(Suite, TestTabId));
    TestEqual(TEXT("Suite release runs the embedded tab close callback exactly once"), EmbeddedCloseCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebuggerSuiteAuthoredModuleLifecycle, "Ck.DebuggerLauncher.Authored.SuiteCloseReopenPreExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebuggerSuiteAuthoredModuleLifecycle::RunTest(const FString&) -> bool
{
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Suite lifecycle test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    FCkDebuggerLauncherModule& Module = FCkDebuggerLauncherModule::Get();
    Module.CloseSuite();
    Module.OpenSuite();
    ck_debugger_launcher_authored_tests::Tick(Slate);

    TSharedPtr<SCkDebuggerSuiteWindow> First = FCkDebuggerLauncherModuleAuthoredTestAccess::GetSuiteWindow(Module);
    TSharedPtr<SDockTab> HeldTab = FCkDebuggerLauncherModuleAuthoredTestAccess::GetSuiteTab(Module);
    TSharedPtr<FCkUiView> HeldView = First.IsValid() ? First->Get_AuthoredShellView() : nullptr;
    if (NOT TestTrue(TEXT("Loaded module opens the real production suite tab"),
        Module.IsSuiteOpen() && HeldTab.IsValid() && HeldView.IsValid()))
    { return false; }

    Module.CloseSuite();
    ck_debugger_launcher_authored_tests::Tick(Slate);
    TestFalse(TEXT("Ordinary close releases module suite state"), Module.IsSuiteOpen());
    TestTrue(TEXT("Ordinary close empties held suite tab content"),
        HeldTab->GetContent() == SNullWidget::NullWidget);

    Module.OpenSuite();
    ck_debugger_launcher_authored_tests::Tick(Slate);
    if (NOT TestTrue(TEXT("Suite reopens after ordinary close"), Module.IsSuiteOpen())) { return false; }
    HeldTab = FCkDebuggerLauncherModuleAuthoredTestAccess::GetSuiteTab(Module);
    if (NOT TestTrue(TEXT("Suite reopen retains a tab for pre-exit validation"), HeldTab.IsValid())) { return false; }
    FCkDebuggerLauncherModuleAuthoredTestAccess::PreExit(Module);
    ck_debugger_launcher_authored_tests::Tick(Slate);
    TestFalse(TEXT("Private pre-exit releases module suite state"), Module.IsSuiteOpen());
    TestTrue(TEXT("Private pre-exit empties held suite tab content"),
        HeldTab->GetContent() == SNullWidget::NullWidget);

    FString Markup;
    FString Css;
    if (TestTrue(TEXT("Held suite-view inert reload resources are readable"),
        ck_debugger_launcher_authored_tests::ReadInstalledPair(TEXT("DebuggerSuiteShell"), Markup, Css)))
    {
        TestTrue(TEXT("Held suite view remains safe after close and pre-exit"),
            HeldView->TryReload(Markup, Css, TEXT("suite held lifecycle reload")).Succeeded);
    }

    HeldView.Reset();
    HeldTab.Reset();
    First.Reset();
    return true;
}

#endif
