#include "CkVisualLodDebugger/Window/SCkVisualLodDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_visual_lod_debugger_window_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkVisualLodDebugger_AuthoredWindow,
    "Ck.VisualLodDebugger.AuthoredWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkVisualLodDebugger_AuthoredWindow::RunTest(const FString&) -> bool
{
    using namespace ck_visual_lod_debugger_window_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Visual LOD debugger authored window test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkVisualLodDebuggerWindow> DebuggerWindow = SNew(SCkVisualLodDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 900.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = DebuggerWindow->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Production Visual LOD debugger admits its authored stable shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(DebuggerWindow->Get_AuthoredShellLoadFailure());
        return false;
    }

    const TSharedPtr<SCkUiSplitter> MainSplit = View->GetSplitter(TEXT("vl-shell-main-split"));
    const TSharedPtr<SCkUiSplitter> LowerSplit = View->GetSplitter(TEXT("vl-shell-lower-split"));
    const TSharedPtr<SCkUiSplitter> TunersSplit = View->GetSplitter(TEXT("vl-shell-tuners-split"));
    const TSharedPtr<SCkUiSplitter> InvestigationSplit = View->GetSplitter(TEXT("vl-shell-investigation-split"));
    const TSharedPtr<SCkUiSplitter> DetailSplit = View->GetSplitter(TEXT("vl-shell-detail-split"));
    TestTrue(TEXT("Authored Visual LOD shell owns every stable nested pane boundary"),
        MainSplit.IsValid() && LowerSplit.IsValid() && TunersSplit.IsValid()
            && InvestigationSplit.IsValid() && DetailSplit.IsValid());
    TestTrue(TEXT("Authored Visual LOD shell provides narrow-width reachability"),
        View->GetScroll(TEXT("vl-shell-scroll")).IsValid());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString InstalledMarkup;
    FString InstalledStylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (!TestTrue(TEXT("Installed Visual LOD shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(InstalledMarkup,
            *FPaths::Combine(ResourceRoot, TEXT("VisualLodDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(InstalledStylesheet,
            *FPaths::Combine(ResourceRoot, TEXT("VisualLodDebuggerShell.ui.css")))))
    { return false; }

    const int64 RevisionBeforeAcceptedReload = View->GetRevision();
    const FCkUiLoadResult Accepted = View->TryReload(
        InstalledMarkup, InstalledStylesheet, TEXT("VisualLodDebuggerShell compatible test candidate"));
    Tick(Slate);
    TestTrue(TEXT("Compatible Visual LOD shell candidate is accepted"), Accepted.Succeeded);
    TestTrue(TEXT("Compatible Visual LOD shell reload retains splitter identity"),
        DebuggerWindow->Get_AuthoredShellView() == View
            && View->GetRevision() > RevisionBeforeAcceptedReload
            && View->GetSplitter(TEXT("vl-shell-main-split")) == MainSplit
            && View->GetSplitter(TEXT("vl-shell-lower-split")) == LowerSplit
            && View->GetSplitter(TEXT("vl-shell-tuners-split")) == TunersSplit
            && View->GetSplitter(TEXT("vl-shell-investigation-split")) == InvestigationSplit
            && View->GetSplitter(TEXT("vl-shell-detail-split")) == DetailSplit);

    const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejectedReload = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("VisualLodDebuggerShell rejected test candidate"));
    TestFalse(TEXT("Missing Visual LOD native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("Rejected Visual LOD shell retains its mounted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejectedReload
            && View->GetRevision() == RevisionBeforeRejectedReload
            && View->GetSplitter(TEXT("vl-shell-main-split")) == MainSplit);

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    DebuggerWindow.Reset();
    TestFalse(TEXT("Visual LOD window teardown releases its authored shell view"), ReleasedView.IsValid());
    return true;
}

#endif
