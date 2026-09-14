#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"

#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkSlateLayout/SCkUiSplitter.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_astar_debugger_authored_shell_tests
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
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget)) { return true; }
        }
        return false;
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (auto Index = int32{0}; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget, const FVector2D& InLocalPosition) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(InLocalPosition);
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

    auto BeginPan(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget, const FVector2D& InLocalPosition) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(InLocalPosition);
        const FPointerEvent DownEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            {EKeys::RightMouseButton}, EKeys::RightMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        return WidgetPathContains(Path, InWidget)
            && InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent)
            && InWidget->HasMouseCapture();
    }

    auto ContinueAndEndPan(
        FSlateApplication& InSlate,
        const TSharedRef<SWidget>& InWidget,
        bool& OutMoveHandled,
        bool& OutUpHandled,
        bool& OutCaptureReleased) -> void
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Previous = Geometry.LocalToAbsolute(FVector2D{18.0f, 18.0f});
        const FVector2D Position = Geometry.LocalToAbsolute(FVector2D{28.0f, 28.0f});
        const FPointerEvent MoveEvent(0, FSlateApplication::CursorPointerIndex, Position, Previous,
            {EKeys::RightMouseButton}, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent UpEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            {}, EKeys::RightMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        OutMoveHandled = InSlate.ProcessMouseMoveEvent(MoveEvent, false);
        OutUpHandled = InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        OutCaptureReleased = NOT InWidget->HasMouseCapture();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAStarDebugger_AuthoredShell,
    "Ck.AStarDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAStarDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_astar_debugger_authored_shell_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("AStar authored-shell test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkAStarDebuggerWindow> DebuggerWindow = SNew(SCkAStarDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    TSharedPtr<FCkUiView> View = DebuggerWindow->_AuthoredShellView;
    if (NOT TestTrue(TEXT("production AStar window admits its authored shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    TSharedPtr<SWidget> Main = View->GetRegion(TEXT("main"));
    TSharedPtr<SCkUiSplitter> MainSplit = View->GetSplitter(TEXT("astar-shell-main-split"));
    TSharedPtr<SCkUiSplitter> SideSplit = View->GetSplitter(TEXT("astar-shell-side-split"));
    TestTrue(TEXT("authored shell owns the 70/30 and 60/40 splitter boundaries"),
        MainSplit.IsValid() && SideSplit.IsValid());
    TestTrue(TEXT("authored grid adapter mounts the exact production painted grid"),
        DebuggerWindow->_GridView.IsValid()
            && ContainsWidget(Main.ToSharedRef(), DebuggerWindow->_GridView.ToSharedRef()));
    TestTrue(TEXT("authored shell retains the production stats and history pane roots"),
        DebuggerWindow->_StatsPane.IsValid() && DebuggerWindow->_HistoryPane.IsValid()
            && ContainsWidget(Main.ToSharedRef(), DebuggerWindow->_StatsPane.ToSharedRef())
            && ContainsWidget(Main.ToSharedRef(), DebuggerWindow->_HistoryPane.ToSharedRef()));
    TestTrue(TEXT("authored shell exposes horizontal overflow reachability"),
        View->GetScroll(TEXT("astar-shell-scroll")).IsValid());

    auto Search = FCkAStarDebugger_SearchInfo{};
    Search.GridWidth = 2;
    Search.GridHeight = 2;
    Search.HasCellData = true;
    DebuggerWindow->_GridView->SetSearchInfo(Search);
    DebuggerWindow->_ViewModel->Set_SelectedCellIndex(-1);
    TestTrue(TEXT("physical routed grid click selects a production grid cell"),
        Click(Slate, DebuggerWindow->_GridView.ToSharedRef(), FVector2D{18.0f, 18.0f})
            && DebuggerWindow->_ViewModel->Get_SelectedCellIndex() == 0);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    const FString Directory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed AStar shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("AStarDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("AStarDebuggerShell.ui.css")))))
    { return false; }

    TestTrue(TEXT("production grid begins a routed right-button pan"),
        BeginPan(Slate, DebuggerWindow->_GridView.ToSharedRef(), FVector2D{18.0f, 18.0f}));
    const auto Revision = View->GetRevision();
    const FCkUiLoadResult Reloaded = View->TryReload(Markup, Css, TEXT("AStar compatible shell candidate"));
    TickSlate(Slate);
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible reload retains splitters and all native pane identities"),
        Reloaded.Succeeded && View->GetRevision() > Revision
            && View->GetSplitter(TEXT("astar-shell-main-split")) == MainSplit
            && View->GetSplitter(TEXT("astar-shell-side-split")) == SideSplit
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_GridView.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatsPane.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_HistoryPane.ToSharedRef())
            && DebuggerWindow->_ViewModel->Get_SelectedCellIndex() == 0
            && DebuggerWindow->_GridView->HasMouseCapture());
    bool PanMoveHandled = false;
    bool PanUpHandled = false;
    bool PanCaptureReleased = false;
    ContinueAndEndPan(Slate, DebuggerWindow->_GridView.ToSharedRef(),
        PanMoveHandled, PanUpHandled, PanCaptureReleased);
    TestTrue(TEXT("compatible reload preserves active grid pan movement"), PanMoveHandled);
    TestTrue(TEXT("compatible reload preserves active grid pan release"), PanUpHandled);
    TestTrue(TEXT("completed grid pan releases restored pointer capture"), PanCaptureReleased);
    DebuggerWindow->_ViewModel->Set_SelectedCellIndex(-1);
    TestTrue(TEXT("continued pan updates production grid hit testing"),
        Click(Slate, DebuggerWindow->_GridView.ToSharedRef(), FVector2D{28.0f, 28.0f})
            && DebuggerWindow->_ViewModel->Get_SelectedCellIndex() == 0);

    const auto RevisionBeforeReject = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native bind=\"missing-port\"/></region></ui>"),
        TEXT(""), TEXT("AStar rejected shell candidate"));
    TestFalse(TEXT("missing AStar native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("rejected reload leaves the committed shell and grid selection unchanged"),
        View->GetRevision() == RevisionBeforeReject
            && View->GetSplitter(TEXT("astar-shell-main-split")) == MainSplit
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_GridView.ToSharedRef())
            && DebuggerWindow->_ViewModel->Get_SelectedCellIndex() == 0);

    const TSharedPtr<SCkAStarDebugger_GridView> HeldGrid = DebuggerWindow->_GridView;
    const TSharedPtr<FCkAStarDebugger_ViewModel> HeldModel = DebuggerWindow->_ViewModel;
    const FGeometry HeldGeometry = HeldGrid->GetCachedGeometry();
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    DebuggerWindow.Reset();
    View.Reset();
    TestFalse(TEXT("authored view releases while its mounted subtree is held"), ReleasedView.IsValid());
    HeldModel->Set_SelectedCellIndex(-1);
    const FVector2D Position = HeldGeometry.LocalToAbsolute(FVector2D{18.0f, 18.0f});
    const FPointerEvent DetachedClick(0, FSlateApplication::CursorPointerIndex, Position, Position,
        {EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
    TestFalse(TEXT("detached held grid refuses routed input"),
        HeldGrid->OnMouseButtonDown(HeldGeometry, DetachedClick).IsEventHandled());
    TestEqual(TEXT("detached held grid does not mutate selection"),
        HeldModel->Get_SelectedCellIndex(), -1);
    return true;
}

#endif
