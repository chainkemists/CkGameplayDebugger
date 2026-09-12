#include "CkTextureDebugger/Window/SCkTextureDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_window_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText)
        { return true; }

        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            { return true; }
        }
        return false;
    }

    auto FindButtonWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton"))
        {
            const TSharedRef<SButton> Button = StaticCastSharedRef<SButton>(InRoot);
            if (ContainsText(Button, InText)) { return Button; }
        }

        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonWithText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (int32 Index = 0; Index < InPath.Widgets.Num(); ++Index)
        {
            if (InPath.Widgets[Index].Widget == InWidget) { return true; }
        }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWindow>& InWindow, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (!InWindow->GetNativeWindow().IsValid() || Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }

        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftButtonDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftButtonDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const FWidgetPath TargetPath = InSlate.LocateWindowUnderMouse(Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (!WidgetPathContains(TargetPath, InWidget)) { return false; }

        const bool bDownHandled = InSlate.ProcessMouseButtonDownEvent(InWindow->GetNativeWindow(), Down);
        const bool bUpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return bDownHandled && bUpHandled;
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), true);
        return FImageUtils::SaveImageByExtension(*InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_AuthoredWindow,
    "Ck.TextureDebugger.AuthoredWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_AuthoredWindow::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_window_authored_tests;

    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Texture debugger authored window test requires Slate.")); return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkTextureDebuggerWindow> TextureWindow = SNew(SCkTextureDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1200.0f, 820.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [TextureWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = TextureWindow->_AuthoredShellView;
    if (!TestTrue(TEXT("Production Texture debugger admits its authored stable shell"),
        TextureWindow->_AuthoredShellMounted && View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(TextureWindow->_AuthoredShellLoadFailure);
        return false;
    }
    TestTrue(TEXT("Authored Texture shell owns its six-page tabs"), View->GetTabs(TEXT("texture-shell-tabs")).IsValid());
    TestTrue(TEXT("Authored Texture shell provides narrow-width reachability"), View->GetScroll(TEXT("texture-shell-scroll")).IsValid());

    const TArray<TPair<FString, int32>> Tabs{
        {TEXT("Checker"), 0}, {TEXT("Texture Health"), 1}, {TEXT("UV & Density"), 2},
        {TEXT("Material Inputs"), 3}, {TEXT("Surface & Lighting"), 4}, {TEXT("Scene Audit"), 5}};
    for (const auto& Tab : Tabs)
    {
        const TSharedPtr<SButton> Button = FindButtonWithText(TextureWindow.ToSharedRef(), Tab.Key);
        if (!TestTrue(*FString::Printf(TEXT("Mounted %s page tab exposes a physical SButton"), *Tab.Key), Button.IsValid())) { return false; }

        const bool bClicked = Click(Slate, HostWindow.ToSharedRef(), Button.ToSharedRef());
        const bool bSelected = bClicked && TextureWindow->Get_ActivePageIndex() == Tab.Value;
        if (!TestTrue(*FString::Printf(TEXT("Physical %s tab click selects its production page"), *Tab.Key), bSelected)) { return false; }
    }

    const TSharedPtr<SButton> RefreshButton = FindButtonWithText(TextureWindow.ToSharedRef(), TEXT("Refresh"));
    TestTrue(TEXT("Mounted Refresh control exposes a physical SButton"), RefreshButton.IsValid());
    TestTrue(TEXT("Physical Refresh click is routed without changing the selected page"), RefreshButton.IsValid()
        && Click(Slate, HostWindow.ToSharedRef(), RefreshButton.ToSharedRef()) && TextureWindow->Get_ActivePageIndex() == 5);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString InstalledMarkup;
    FString InstalledStylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (!TestTrue(TEXT("Installed Texture shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(InstalledMarkup, *FPaths::Combine(ResourceRoot, TEXT("TextureDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(InstalledStylesheet, *FPaths::Combine(ResourceRoot, TEXT("TextureDebuggerShell.ui.css")))))
    { return false; }

    const int64 RevisionBeforeAcceptedReload = View->GetRevision();
    const FCkUiLoadResult Accepted = View->TryReload(
        InstalledMarkup, InstalledStylesheet, TEXT("TextureDebuggerShell compatible test candidate"));
    Tick(Slate);
    TestTrue(TEXT("Compatible Texture shell candidate is accepted"), Accepted.Succeeded);
    TestTrue(TEXT("Compatible Texture shell reload preserves authoritative page selection"),
        TextureWindow->Get_ActivePageIndex() == 5 && View->GetRevision() > RevisionBeforeAcceptedReload);

    const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejectedReload = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("TextureDebuggerShell rejected test candidate"));
    TestFalse(TEXT("Invalid Texture shell candidate is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("Rejected Texture shell candidate retains the mounted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejectedReload && View->GetRevision() == RevisionBeforeRejectedReload);

    const FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TextureDebugger/WindowAuthored"));
    TestTrue(TEXT("Wide full Texture debugger window capture writes"), Capture(Slate, HostWindow.ToSharedRef(), FPaths::Combine(OutputDirectory, TEXT("Window-Wide.png"))));
    HostWindow->Resize(FVector2D{640.0f, 560.0f});
    Tick(Slate);
    TestTrue(TEXT("Narrow full Texture debugger window capture writes"), Capture(Slate, HostWindow.ToSharedRef(), FPaths::Combine(OutputDirectory, TEXT("Window-Narrow.png"))));

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    Tick(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    View.Reset();
    TextureWindow.Reset();
    TestFalse(TEXT("Texture window teardown releases its authored shell view"), ReleasedView.IsValid());
    return true;
}

#endif
