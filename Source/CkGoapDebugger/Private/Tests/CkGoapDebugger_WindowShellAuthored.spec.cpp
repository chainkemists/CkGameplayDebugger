#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_goap_debugger_window_shell_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkGoapDebuggerWindow_AuthoredShell,
    "Ck.UiAuthoring.GoapDebugger.Window.AuthoredStableShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkGoapDebuggerWindow_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_goap_debugger_window_shell_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("GOAP authored-shell test requires Slate."));
        return false;
    }

    TSharedPtr<SCkGoapDebuggerWindow> Window = SNew(SCkGoapDebuggerWindow);
    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{1024.0f, 700.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Window.ToSharedRef()];
    ON_SCOPE_EXIT
    {
        if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
    };

    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = Window->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Production GOAP window admits the authored stable shell"), View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(Window->Get_AuthoredShellLoadFailure());
        return false;
    }

    TestTrue(TEXT("Authored shell retains outer tabs"), View->GetTabs(TEXT("goap-main-tabs")).IsValid());
    TestTrue(TEXT("Authored shell retains center tabs"), View->GetTabs(TEXT("goap-center-tabs")).IsValid());
    TestTrue(TEXT("Authored shell retains the inspector split grid"),
        View->GetSplitter(TEXT("goap-inspector-split")).IsValid()
            && View->GetSplitter(TEXT("goap-inspector-top-split")).IsValid()
            && View->GetSplitter(TEXT("goap-inspector-left-split")).IsValid());
    TestTrue(TEXT("Authored shell provides narrow-width reachability"), View->GetScroll(TEXT("goap-shell-scroll")).IsValid());

    const TWeakPtr<FCkUiView> ViewBeforePoll = View;
    const TSharedRef<SWidget> MainBeforePoll = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforePoll = View->GetRevision();
    TestFalse(TEXT("Unchanged GOAP shell resources are deduplicated"), View->PollFiles());
    Tick(Slate);
    TestTrue(TEXT("Deduplicated poll retains shell view, mount, and revision identity"),
        Window->Get_AuthoredShellView() == ViewBeforePoll.Pin()
            && View->GetRegion(TEXT("main")) == MainBeforePoll
            && View->GetRevision() == RevisionBeforePoll);

    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"goap-shell-squad\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("GoapDebuggerShell rejected test candidate"));
    TestFalse(TEXT("Invalid shell candidate is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("Rejected shell candidate retains the mounted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforePoll && View->GetRevision() == RevisionBeforePoll);

    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    Window.Reset();
    TestFalse(TEXT("GOAP window teardown releases its authored shell view"), ReleasedView.IsValid());

    return true;
}

#endif
