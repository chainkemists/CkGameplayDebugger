#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_authored_shell_tests
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InTarget) -> bool
    {
        if (InRoot == InTarget) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget))
            { return true; }
        }
        return false;
    }

    auto SubtreeHasText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText)
        { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (SubtreeHasText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            { return true; }
        }
        return false;
    }

    auto FindButtonWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && SubtreeHasText(InRoot, InText))
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonWithText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (int32 Index = 0; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent MoveEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent DownEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent UpEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(MoveEvent, true);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(Path, InWidget)) { return false; }
        const bool DownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        return DownHandled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_AuthoredShell,
    "Ck.AudioDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_authored_shell_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Audio authored-shell test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkAudioDebuggerWindow> DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    TSharedPtr<FCkUiView> View = DebuggerWindow->_AuthoredShellView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    TestTrue(TEXT("authored shell retains all four production presentation boundaries"),
        DebuggerWindow->_Tabs.IsValid()
            && DebuggerWindow->_StatCards.IsValid()
            && DebuggerWindow->_FilterRow.IsValid()
            && DebuggerWindow->_PageSwitcher.IsValid()
            && ContainsWidget(Main, DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_PageSwitcher.ToSharedRef()));
    TestEqual(TEXT("production page switcher retains all six Audio pages"),
        DebuggerWindow->_PageSwitcher->GetNumWidgets(), 6);
    TestEqual(TEXT("production Audio shell preserves Tracks as its default page"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex(), 1);
    TestTrue(TEXT("authored shell exposes horizontal overflow reachability"),
        View->GetScroll(TEXT("audio-shell-scroll")).IsValid());

    const TSharedPtr<SButton> CrossfadeTab = FindButtonWithText(
        DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Crossfade"));
    TestTrue(TEXT("physical nondefault tab selection routes through the production tab strip"),
        CrossfadeTab.IsValid() && Click(Slate, CrossfadeTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    const FString Directory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed Audio shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.css")))))
    { return false; }

    const auto Revision = View->GetRevision();
    const FCkUiLoadResult Reloaded = View->TryReload(Markup, Css, TEXT("Audio compatible shell candidate"));
    TickSlate(Slate);
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible reload retains every native boundary and page state"),
        Reloaded.Succeeded && View->GetRevision() > Revision
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const auto RevisionBeforeReject = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>"),
        TEXT(""), TEXT("Audio rejected shell candidate"));
    TestFalse(TEXT("missing Audio native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("rejection reaches missing-binding admission"),
        FString::Join(Rejected.Errors, TEXT("\n")).Contains(TEXT("missing-audio-port")));
    TestTrue(TEXT("rejected reload leaves the committed shell and page state intact"),
        View->GetRevision() == RevisionBeforeReject
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    HostWindow->Resize(FVector2D{480.0f, 480.0f});
    TickSlate(Slate);
    TestTrue(TEXT("actual narrow Audio shell keeps horizontal overflow reachable"),
        View->GetScroll(TEXT("audio-shell-scroll"))->GetScrollOffsetOfEnd() > 0.0f);

    HostWindow->Resize(FVector2D{1100.0f, 240.0f});
    TickSlate(Slate);
    TestTrue(TEXT("short Audio shell lets its page body shrink without vertical clipping pressure"),
        DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y > 0.0f
            && DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y < 320.0f);

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    DebuggerWindow.Reset();
    View.Reset();
    TestFalse(TEXT("authored Audio view releases with its production window"), ReleasedView.IsValid());

    const FString InvalidStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>");
    bool MarkupRestored = false;
    ON_SCOPE_EXIT
    {
        if (NOT MarkupRestored)
        { FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html"))); }
    };
    if (NOT TestTrue(TEXT("Audio fixture installs its valid-but-unbound startup candidate"),
        FFileHelper::SaveStringToFile(
            InvalidStartupMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))))
    { return false; }

    DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);
    View = DebuggerWindow->_AuthoredShellView;
    TestTrue(TEXT("invalid startup resource mounts the complete native fallback"),
        View.IsValid() && NOT View->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(),
                DebuggerWindow->_PageSwitcher.ToSharedRef()));

    MarkupRestored = FFileHelper::SaveStringToFile(
        Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid production resource"), MarkupRestored);
    DebuggerWindow->PollAuthoredShell(FPlatformTime::Seconds() + 10.0);
    TickSlate(Slate);
    TestTrue(TEXT("valid file change recovers a live startup fallback without reopening"),
        View->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef()));
    return true;
}

#endif
