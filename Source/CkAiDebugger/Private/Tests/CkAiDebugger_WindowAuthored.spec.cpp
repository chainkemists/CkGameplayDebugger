#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_ai_debugger_window_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y)
        { return false; }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), true);
        return FImageUtils::SaveImageByExtension(
            *InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAiDebugger_AuthoredWindow,
    "Ck.AiDebugger.AuthoredWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAiDebugger_AuthoredWindow::RunTest(const FString&) -> bool
{
    using namespace ck_ai_debugger_window_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("AI debugger authored window test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkAiDebuggerWindow> AiWindow = SNew(SCkAiDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1280.0f, 900.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [AiWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = AiWindow->_AuthoredShellView;
    if (!TestTrue(TEXT("Production AI debugger admits its authored stable shell"),
        AiWindow->_AuthoredShellMounted && View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(AiWindow->_AuthoredShellLoadError);
        return false;
    }

    TestTrue(TEXT("Authored AI shell owns every stable splitter boundary"),
        View->GetSplitter(TEXT("ai-shell-root")).IsValid()
            && View->GetSplitter(TEXT("ai-workspace-split")).IsValid()
            && View->GetSplitter(TEXT("ai-overview-split")).IsValid()
            && View->GetSplitter(TEXT("ai-summary-split")).IsValid()
            && View->GetSplitter(TEXT("ai-topology-split")).IsValid()
            && View->GetSplitter(TEXT("ai-diagnostics-split")).IsValid()
            && View->GetSplitter(TEXT("ai-diagnostics-list-split")).IsValid());
    TestTrue(TEXT("Authored AI shell provides narrow-width reachability"),
        View->GetScroll(TEXT("ai-shell-scroll")).IsValid());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString InstalledMarkup;
    FString InstalledStylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (!TestTrue(TEXT("Installed AI shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(InstalledMarkup, *FPaths::Combine(ResourceRoot, TEXT("AiDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(InstalledStylesheet, *FPaths::Combine(ResourceRoot, TEXT("AiDebuggerShell.ui.css")))))
    { return false; }

    const int64 RevisionBeforeAcceptedReload = View->GetRevision();
    const FCkUiLoadResult Accepted = View->TryReload(
        InstalledMarkup, InstalledStylesheet, TEXT("AiDebuggerShell compatible test candidate"));
    Tick(Slate);
    TestTrue(TEXT("Compatible AI shell candidate is accepted"), Accepted.Succeeded);
    TestTrue(TEXT("Compatible AI shell reload retains the mounted view"),
        AiWindow->_AuthoredShellView == View && View->GetRevision() > RevisionBeforeAcceptedReload);

    const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejectedReload = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("AiDebuggerShell rejected test candidate"));
    TestFalse(TEXT("Invalid AI shell candidate is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("Rejected AI shell candidate retains the mounted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejectedReload
            && View->GetRevision() == RevisionBeforeRejectedReload);

    const FString OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation/AiDebugger/WindowAuthored"));
    TestTrue(TEXT("Wide full AI debugger window capture writes"), Capture(
        Slate, HostWindow.ToSharedRef(), FPaths::Combine(OutputDirectory, TEXT("Window-Wide.png"))));
    HostWindow->Resize(FVector2D{640.0f, 600.0f});
    Tick(Slate);
    TestTrue(TEXT("Narrow full AI debugger window capture writes"), Capture(
        Slate, HostWindow.ToSharedRef(), FPaths::Combine(OutputDirectory, TEXT("Window-Narrow.png"))));

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    AiWindow.Reset();
    TestFalse(TEXT("AI window teardown releases its authored shell view"), ReleasedView.IsValid());
    return true;
}

#endif
