#include "CkEqsDebugger/Window/SCkEqsDebuggerWindow.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_CandidatePanel.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_QueryList.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_TestBreakdownPanel.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_eqs_debugger_window_shell_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InExpected) -> bool
    {
        if (InRoot == InExpected) { return true; }

        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InExpected))
            { return true; }
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEqsDebuggerWindow_AuthoredShell,
    "Ck.UiAuthoring.EqsDebugger.Window.AuthoredStableShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkEqsDebuggerWindow_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_eqs_debugger_window_shell_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("EQS authored-shell test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT
    {
        if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
    };

    TSharedPtr<SCkEqsDebuggerWindow> Window = SNew(SCkEqsDebuggerWindow);
    Host = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{1120.0f, 700.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Window.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = Window->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Production EQS window admits the authored stable shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(Window->Get_AuthoredShellLoadFailure());
        return false;
    }

    TestTrue(TEXT("Authored EQS shell retains its horizontal three-pane splitter"),
        View->GetSplitter(TEXT("eqs-shell-split")).IsValid());

    TSharedPtr<SCkEqsDebugger_QueryList> QueryList = Window->Get_QueryList();
    TSharedPtr<SCkEqsDebugger_CandidatePanel> CandidatePanel = Window->Get_CandidatePanel();
    TSharedPtr<SCkEqsDebugger_TestBreakdownPanel> TestBreakdownPanel = Window->Get_TestBreakdownPanel();
    if (!TestTrue(TEXT("EQS window creates all three native content panels"),
        QueryList.IsValid() && CandidatePanel.IsValid() && TestBreakdownPanel.IsValid()))
    { return false; }

    {
        const TSharedRef<SWidget> MountedTree = Window.ToSharedRef();
        TestTrue(TEXT("Authored EQS shell physically mounts the QueryList native port"), ContainsWidget(MountedTree, QueryList.ToSharedRef()));
        TestTrue(TEXT("Authored EQS shell physically mounts the CandidatePanel native port"), ContainsWidget(MountedTree, CandidatePanel.ToSharedRef()));
        TestTrue(TEXT("Authored EQS shell physically mounts the TestBreakdownPanel native port"), ContainsWidget(MountedTree, TestBreakdownPanel.ToSharedRef()));
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString InstalledMarkup;
    FString InstalledStylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (!TestTrue(TEXT("Installed EQS shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(InstalledMarkup, *FPaths::Combine(ResourceRoot, TEXT("EqsDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(InstalledStylesheet, *FPaths::Combine(ResourceRoot, TEXT("EqsDebuggerShell.ui.css")))))
    { return false; }

    const int64 RevisionBeforeAcceptedReload = View->GetRevision();
    const FCkUiLoadResult Accepted = View->TryReload(
        InstalledMarkup, InstalledStylesheet, TEXT("EqsDebuggerShell compatible test candidate"));
    Tick(Slate);
    TestTrue(TEXT("Compatible EQS shell candidate is accepted"), Accepted.Succeeded);
    TestTrue(TEXT("Compatible reload retains the three production panel identities"),
        Window->Get_QueryList() == QueryList
            && Window->Get_CandidatePanel() == CandidatePanel
            && Window->Get_TestBreakdownPanel() == TestBreakdownPanel
            && View->GetRevision() > RevisionBeforeAcceptedReload);

    {
        const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
        const int64 RevisionBeforeRejectedReload = View->GetRevision();
        const FCkUiLoadResult Rejected = View->TryReload(
            TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
            TEXT(""), TEXT("EqsDebuggerShell rejected test candidate"));
        TestFalse(TEXT("Invalid EQS shell candidate is rejected atomically"), Rejected.Succeeded);
        TestTrue(TEXT("Rejected EQS shell candidate retains the mounted tree and revision"),
            View->GetRegion(TEXT("main")) == MainBeforeRejectedReload && View->GetRevision() == RevisionBeforeRejectedReload);
    }

    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    QueryList.Reset();
    CandidatePanel.Reset();
    TestBreakdownPanel.Reset();
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    Window.Reset();
    TestFalse(TEXT("EQS window teardown releases its authored shell view"), ReleasedView.IsValid());
    return true;
}

#endif
