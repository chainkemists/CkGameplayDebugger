#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "CkSlateLayout/SCkUiSplitter.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_ecs_debugger_window_shell_authored_tests
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto CapturePanes(const TSharedPtr<SCkUiSplitter>& InSplitter) -> TArray<TSharedPtr<SWidget>>
    {
        auto Result = TArray<TSharedPtr<SWidget>>{};
        if (NOT InSplitter.IsValid() || NOT InSplitter->GetSplitter().IsValid()) { return Result; }
        const auto Children = InSplitter->GetSplitter()->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        { Result.Add(Children->GetChildAt(Index)); }
        return Result;
    }

    auto CaptureOnlyChild(const TSharedPtr<SWidget>& InWidget) -> TSharedPtr<SWidget>
    {
        if (NOT InWidget.IsValid()) { return nullptr; }
        FChildren* Children = InWidget->GetChildren();
        if (Children == nullptr || Children->Num() != 1) { return nullptr; }
        return ConstCastSharedRef<SWidget>(Children->GetChildAt(0));
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InTarget) -> bool
    {
        if (InRoot == InTarget) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget)) { return true; }
        }
        return false;
    }

    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto FindButtonForText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && ContainsText(InRoot, InText))
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindButtonForText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid()) { return Found; }
        }
        return nullptr;
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (auto Index = int32{0}; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const auto Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const auto Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const auto Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
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
        const auto TargetPath = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(TargetPath, InWidget)) { return false; }
        const auto DownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        const auto UpHandled = InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        return DownHandled && UpHandled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebugger_WindowShellAuthored,
    "Ck.EcsDebugger.AuthoredWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkEcsDebugger_WindowShellAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_ecs_debugger_window_shell_authored_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("ECS debugger authored window test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkDebuggerWindow_Main> DebuggerWindow = SNew(SCkDebuggerWindow_Main);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1200.0f, 760.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    auto View = DebuggerWindow->Get_AuthoredShellView();
    auto CenterView = DebuggerWindow->Get_AuthoredCenterView();
    if (NOT TestTrue(TEXT("production ECS debugger admits its authored stable shell"),
        View.IsValid() && View->GetLastResult().Succeeded
            && CenterView.IsValid() && CenterView->GetLastResult().Succeeded))
    {
        AddError(DebuggerWindow->Get_AuthoredShellLoadFailure());
        return false;
    }

    const auto Splitter = View->GetSplitter(TEXT("ecs-shell-main-split"));
    const auto Center = CenterView->GetRegion(TEXT("main"));
    const auto Panes = CapturePanes(Splitter);
    const auto LeftPane = DebuggerWindow->Get_AuthoredLeftPane();
    const auto CenterPane = DebuggerWindow->Get_AuthoredCenterPane();
    const auto InspectorPane = DebuggerWindow->Get_AuthoredInspectorPane();
    const auto PageTabs = DebuggerWindow->Get_PageTabsWidget();
    const auto PageContent = DebuggerWindow->Get_PageContentContainer();
    const auto LayoutMounted = Splitter.IsValid() && Center->GetChildren()->Num() > 0 && Panes.Num() == 3
        && LeftPane.IsValid() && CenterPane.IsValid() && InspectorPane.IsValid()
        && ContainsWidget(Splitter.ToSharedRef(), LeftPane.ToSharedRef())
        && ContainsWidget(Splitter.ToSharedRef(), CenterPane.ToSharedRef())
        && ContainsWidget(Splitter.ToSharedRef(), InspectorPane.ToSharedRef())
        && PageTabs.IsValid() && PageContent.IsValid()
        && ContainsWidget(Center, PageTabs.ToSharedRef())
        && ContainsWidget(Center, PageContent.ToSharedRef());
    if (NOT TestTrue(TEXT("authored ECS shell owns the three-pane and center tab/body boundaries"),
        LayoutMounted))
    { return false; }
    TestTrue(TEXT("authored ECS shell declares a horizontal overflow host for narrow layouts"),
        View->GetScroll(TEXT("ecs-shell-scroll")).IsValid());

    const auto InitialPageBody = CaptureOnlyChild(PageContent);
    const auto GraphButton = FindButtonForText(PageTabs.ToSharedRef(), TEXT("Graph"));
    TestTrue(TEXT("physical Graph-tab selection replaces only the page body"),
        GraphButton.IsValid() && Click(Slate, GraphButton.ToSharedRef())
            && CaptureOnlyChild(PageContent) != InitialPageBody
            && DebuggerWindow->Get_PageTabsWidget() == PageTabs
            && DebuggerWindow->Get_PageContentContainer() == PageContent);
    const auto GraphPageBody = CaptureOnlyChild(PageContent);

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    FString CenterMarkup;
    FString CenterCss;
    const auto ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed ECS shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(ResourceRoot, TEXT("EcsDebuggerShell.ui.css")))
        && FFileHelper::LoadFileToString(CenterMarkup, *FPaths::Combine(ResourceRoot, TEXT("EcsDebuggerCenter.ui.html")))
        && FFileHelper::LoadFileToString(CenterCss, *FPaths::Combine(ResourceRoot, TEXT("EcsDebuggerCenter.ui.css")))))
    { return false; }

    const auto Revision = View->GetRevision();
    const auto CenterRevision = CenterView->GetRevision();
    const auto CenterReloaded = CenterView->TryReload(
        CenterMarkup, CenterCss, TEXT("ECS debugger center compatible candidate"));
    const auto Reloaded = View->TryReload(Markup, Css, TEXT("ECS debugger shell compatible candidate"));
    TickSlate(Slate);
    if (NOT CenterReloaded.Succeeded) { AddError(FString::Join(CenterReloaded.Errors, TEXT("\n"))); }
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible ECS shell reload retains splitter and pane identity"), Reloaded.Succeeded
        && CenterReloaded.Succeeded
        && DebuggerWindow->Get_AuthoredShellView() == View && View->GetRevision() > Revision
        && DebuggerWindow->Get_AuthoredCenterView() == CenterView
        && CenterView->GetRevision() > CenterRevision
        && View->GetSplitter(TEXT("ecs-shell-main-split")) == Splitter
        && CenterView->GetRegion(TEXT("main")) == Center
        && CapturePanes(Splitter).Num() == 3
        && DebuggerWindow->Get_AuthoredLeftPane() == LeftPane
        && DebuggerWindow->Get_AuthoredCenterPane() == CenterPane
        && DebuggerWindow->Get_AuthoredInspectorPane() == InspectorPane
        && ContainsWidget(Splitter.ToSharedRef(), LeftPane.ToSharedRef())
        && ContainsWidget(Splitter.ToSharedRef(), CenterPane.ToSharedRef())
        && ContainsWidget(Splitter.ToSharedRef(), InspectorPane.ToSharedRef())
        && DebuggerWindow->Get_PageTabsWidget() == PageTabs
        && DebuggerWindow->Get_PageContentContainer() == PageContent
        && CaptureOnlyChild(PageContent) == GraphPageBody);

    const auto CenterRevisionBeforeRejectedReload = CenterView->GetRevision();
    const auto CenterRejected = CenterView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("ECS debugger center rejected candidate"));
    TestFalse(TEXT("missing ECS center native port is rejected atomically"), CenterRejected.Succeeded);
    TestTrue(TEXT("rejected ECS center reload retains mounted tree and native identity"),
        CenterView->GetRevision() == CenterRevisionBeforeRejectedReload
            && CenterView->GetRegion(TEXT("main")) == Center
            && DebuggerWindow->Get_PageTabsWidget() == PageTabs
            && DebuggerWindow->Get_PageContentContainer() == PageContent
            && CaptureOnlyChild(PageContent) == GraphPageBody);

    const auto MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
    const auto RevisionBeforeRejectedReload = View->GetRevision();
    const auto Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-port\" /></region></ui>"),
        TEXT(""), TEXT("ECS debugger shell rejected candidate"));
    TestFalse(TEXT("missing ECS shell native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("rejected ECS shell reload retains mounted tree and pane identity"),
        View->GetRegion(TEXT("main")) == MainBeforeRejectedReload
            && View->GetRevision() == RevisionBeforeRejectedReload
            && View->GetSplitter(TEXT("ecs-shell-main-split")) == Splitter
            && CapturePanes(Splitter).Num() == 3
            && DebuggerWindow->Get_AuthoredLeftPane() == LeftPane
            && DebuggerWindow->Get_AuthoredCenterPane() == CenterPane
            && DebuggerWindow->Get_AuthoredInspectorPane() == InspectorPane
            && ContainsWidget(Splitter.ToSharedRef(), LeftPane.ToSharedRef())
            && ContainsWidget(Splitter.ToSharedRef(), CenterPane.ToSharedRef())
            && ContainsWidget(Splitter.ToSharedRef(), InspectorPane.ToSharedRef()));

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    const TWeakPtr<FCkUiView> ReleasedCenterView = CenterView;
    View.Reset();
    CenterView.Reset();
    DebuggerWindow.Reset();
    TestTrue(TEXT("ECS debugger teardown releases both authored shell views"),
        NOT ReleasedView.IsValid() && NOT ReleasedCenterView.IsValid());
    return true;
}

#endif
