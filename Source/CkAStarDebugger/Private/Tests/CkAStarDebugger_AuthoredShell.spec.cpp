#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"

#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSplitter.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"

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

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
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

    auto CreateEntity(ck::FEcsWorld& InWorld) -> FCk_Handle
    {
        return UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
    }

    auto FirstRecordText(const TSharedPtr<const FCkUiCollection>& InCollection, const FString& InField) -> FString
    {
        if (NOT InCollection.IsValid() || InCollection->GetRecords().IsEmpty()) { return {}; }
        const FCkUiFieldValue* Value = InCollection->GetRecords()[0]->FindField(InField);
        return Value != nullptr && Value->Kind == ECkUiFieldKind::Text ? Value->Text.ToString() : FString{};
    }

    auto TaggedText(const TSharedRef<SWidget>& InRoot, const TCHAR* InTag) -> FString
    {
        const TSharedPtr<SWidget> Widget = FindTaggedWidget(InRoot, FName{InTag});
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetText().ToString() : FString{};
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

    TSharedPtr<FCkUiView> StatsView = DebuggerWindow->_StatsPanel->Get_AuthoredView();
    TSharedPtr<FCkUiView> HistoryView = DebuggerWindow->_SearchHistory->Get_AuthoredView();
    if (NOT TestTrue(TEXT("production stats and history panels admit independent authored views"),
        StatsView.IsValid() && StatsView->GetLastResult().Succeeded
            && HistoryView.IsValid() && HistoryView->GetLastResult().Succeeded))
    {
        if (StatsView.IsValid() && NOT StatsView->GetLastResult().Succeeded)
        { AddError(TEXT("Stats authored admission: ") + FString::Join(StatsView->GetLastResult().Errors, TEXT("\n"))); }
        if (HistoryView.IsValid() && NOT HistoryView->GetLastResult().Succeeded)
        { AddError(TEXT("History authored admission: ") + FString::Join(HistoryView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    auto FixtureWorld = ck::FEcsWorld{};
    const FCk_Handle EntityA = CreateEntity(FixtureWorld);
    const FCk_Handle EntityB = CreateEntity(FixtureWorld);
    auto InfoA = FCkAStarDebugger_SearchInfo{};
    InfoA.EntityHandle = EntityA;
    InfoA.DebugName = TEXT("A");
    InfoA.SearchStatus = ECk_AStarSearchStatus::InProgress;
    InfoA.GridWidth = 2;
    InfoA.GridHeight = 2;
    InfoA.GoalNode = 3;
    InfoA.HasCellData = true;
    InfoA.GScores.Add(0, 2.0f);
    InfoA.CameFrom.Add(0, 1);
    InfoA.OpenSetCells.Add(0);
    InfoA.OpenSetSize = 1;
    InfoA.ClosedSetSize = 1;
    InfoA.TotalIterations = 3;
    InfoA.BudgetUsagePercent = 50.0f;
    auto InfoB = InfoA;
    InfoB.EntityHandle = EntityB;
    InfoB.DebugName = TEXT("B");
    DebuggerWindow->_ViewModel->_DataCollector._SearchEntities = {InfoA, InfoB};
    DebuggerWindow->_ViewModel->Set_SelectedEntityHandle(EntityA);
    DebuggerWindow->_ViewModel->Set_SelectedCellIndex(0);
    DebuggerWindow->_StatsPanel->Tick(
        DebuggerWindow->_StatsPanel->GetCachedGeometry(), FPlatformTime::Seconds(), 0.016f);
    TestTrue(TEXT("authored stats project the selected production search and cell"),
        DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-iterations")).ToString() == TEXT("3")
            && DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-state")).ToString() == TEXT("Open")
            && DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-g")).ToString() == TEXT("2.0"));
    DebuggerWindow->_ViewModel->_DataCollector._SearchEntities[0].GScores.Add(0, 7.0f);
    DebuggerWindow->_ViewModel->_DataCollector._SearchEntities[0].OpenSetCells.Remove(0);
    DebuggerWindow->_ViewModel->_DataCollector._SearchEntities[0].ClosedSetCells.Add(0);
    DebuggerWindow->_StatsPanel->Tick(
        DebuggerWindow->_StatsPanel->GetCachedGeometry(), FPlatformTime::Seconds(), 0.016f);
    TickSlate(Slate);
    TestTrue(TEXT("same selected cell refreshes when its production search data changes"),
        DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-state")).ToString() == TEXT("Closed")
            && DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-g")).ToString() == TEXT("7.0")
            && TaggedText(StatsView->GetRegion(TEXT("main")), TEXT("astar-stats-cell-state")) == TEXT("Closed")
            && TaggedText(StatsView->GetRegion(TEXT("main")), TEXT("astar-stats-cell-g")) == TEXT("7.0"));
    TSharedPtr<SWidget> StatsCopy = FindTaggedWidget(
        StatsView->GetRegion(TEXT("main")), FName{TEXT("astar-stats-copy")});
    FString CopiedStats;
    FPlatformApplicationMisc::ClipboardCopy(TEXT("unchanged"));
    const bool StatsCopyClicked = StatsCopy.IsValid() && Click(
        Slate, StatsCopy.ToSharedRef(), StatsCopy->GetCachedGeometry().GetLocalSize() * 0.5f);
    FPlatformApplicationMisc::ClipboardPaste(CopiedStats);
    TestTrue(TEXT("physical authored stats copy preserves information-copy parity"),
        StatsCopyClicked && CopiedStats.Contains(TEXT("iterations: 3"))
            && CopiedStats.Contains(TEXT("g-score:    7.0")));

    auto HistoryA = FCkAStarDebugger_HistoryEntry{};
    HistoryA.FrameNumber = 11;
    HistoryA.FinalStatus = ECk_AStarSearchStatus::Complete;
    HistoryA.TotalIterations = 4;
    HistoryA.TotalTimeMicroseconds = 21;
    HistoryA.TotalCost = 3.5f;
    HistoryA.PathLength = 2;
    auto HistoryB = HistoryA;
    HistoryB.FrameNumber = 22;
    HistoryB.FinalStatus = ECk_AStarSearchStatus::Failed;
    HistoryB.TotalIterations = 8;
    DebuggerWindow->_ViewModel->_DataCollector._SearchHistory.Add(GetTypeHash(EntityA), {HistoryA});
    DebuggerWindow->_ViewModel->_DataCollector._SearchHistory.Add(GetTypeHash(EntityB), {HistoryB});
    DebuggerWindow->_ViewModel->Set_SelectedEntityHandle(EntityA);
    DebuggerWindow->_SearchHistory->RefreshFromViewModel();
    TestTrue(TEXT("authored history projects the selected entity's production record"),
        FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("frame")) == TEXT("F#11")
            && FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("status")) == TEXT("Complete"));
    DebuggerWindow->_ViewModel->Set_SelectedEntityHandle(EntityB);
    DebuggerWindow->_SearchHistory->RefreshFromViewModel();
    TestTrue(TEXT("equal-count entity switch replaces stale authored history"),
        FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("frame")) == TEXT("F#22")
            && FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("status")) == TEXT("Failed"));
    TickSlate(Slate);
    TSharedPtr<SWidget> HistoryCopy = FindTaggedWidget(
        HistoryView->GetRegion(TEXT("main")), FName{TEXT("astar-history-copy")});
    FString CopiedHistory;
    FPlatformApplicationMisc::ClipboardCopy(TEXT("unchanged"));
    const bool HistoryCopyClicked = HistoryCopy.IsValid() && Click(
        Slate, HistoryCopy.ToSharedRef(), HistoryCopy->GetCachedGeometry().GetLocalSize() * 0.5f);
    FPlatformApplicationMisc::ClipboardPaste(CopiedHistory);
    TestTrue(TEXT("physical authored history copy routes the selected production record"),
        HistoryCopyClicked && CopiedHistory.Contains(TEXT("A* search [Failed]"))
            && CopiedHistory.Contains(TEXT("frame:      22")));

    HostWindow->Resize(FVector2D{520.0f, 600.0f});
    TickSlate(Slate);
    TestTrue(TEXT("actual narrow AStar window has reachable horizontal overflow"),
        View->GetScroll(TEXT("astar-shell-scroll")).IsValid()
            && View->GetScroll(TEXT("astar-shell-scroll"))->GetScrollOffsetOfEnd() > 0.0f);
    HostWindow->Resize(FVector2D{1100.0f, 720.0f});
    TickSlate(Slate);

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

    FString StatsMarkup;
    FString StatsCss;
    FString HistoryMarkup;
    FString HistoryCss;
    if (NOT TestTrue(TEXT("installed AStar stats and history resources are readable"),
        FFileHelper::LoadFileToString(StatsMarkup, *FPaths::Combine(Directory, TEXT("AStarDebuggerStats.ui.html")))
            && FFileHelper::LoadFileToString(StatsCss, *FPaths::Combine(Directory, TEXT("AStarDebuggerStats.ui.css")))
            && FFileHelper::LoadFileToString(HistoryMarkup, *FPaths::Combine(Directory, TEXT("AStarDebuggerSearchHistory.ui.html")))
            && FFileHelper::LoadFileToString(HistoryCss, *FPaths::Combine(Directory, TEXT("AStarDebuggerSearchHistory.ui.css")))))
    { return false; }
    const TSharedRef<SWidget> StatsMain = StatsView->GetRegion(TEXT("main"));
    const TSharedPtr<SCkUiRepeat> HistoryRepeat = HistoryView->GetRepeat(TEXT("astar-history-records"));
    const auto StatsRevision = StatsView->GetRevision();
    const auto HistoryRevision = HistoryView->GetRevision();
    const FCkUiLoadResult StatsReloaded = StatsView->TryReload(
        StatsMarkup, StatsCss, TEXT("AStar compatible stats candidate"));
    const FCkUiLoadResult HistoryReloaded = HistoryView->TryReload(
        HistoryMarkup, HistoryCss, TEXT("AStar compatible history candidate"));
    TickSlate(Slate);
    TestTrue(TEXT("compatible stats and history reload retains roots, repeated records, and live values"),
        StatsReloaded.Succeeded && HistoryReloaded.Succeeded
            && StatsView->GetRevision() > StatsRevision && HistoryView->GetRevision() > HistoryRevision
            && StatsView->GetRegion(TEXT("main")) == StatsMain
            && HistoryView->GetRepeat(TEXT("astar-history-records")) == HistoryRepeat
            && DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-g")).ToString() == TEXT("2.0")
            && FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("frame")) == TEXT("F#22"));
    StatsCopy = FindTaggedWidget(StatsView->GetRegion(TEXT("main")), FName{TEXT("astar-stats-copy")});
    HistoryCopy = FindTaggedWidget(HistoryView->GetRegion(TEXT("main")), FName{TEXT("astar-history-copy")});
    TestTrue(TEXT("compatible reload exposes the current stats and history actions"),
        StatsCopy.IsValid() && HistoryCopy.IsValid());

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

    const auto StatsRevisionBeforeReject = StatsView->GetRevision();
    const auto HistoryRevisionBeforeReject = HistoryView->GetRevision();
    const FCkUiLoadResult StatsRejected = StatsView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native bind=\"missing-stats-port\"/></region></ui>"),
        TEXT(""), TEXT("AStar rejected stats candidate"));
    const FCkUiLoadResult HistoryRejected = HistoryView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><repeat id=\"history\" bind=\"missing-history\"/></region></ui>"),
        TEXT(""), TEXT("AStar rejected history candidate"));
    TestTrue(TEXT("invalid stats and history candidates reject atomically without losing live projections"),
        NOT StatsRejected.Succeeded && NOT HistoryRejected.Succeeded
            && StatsView->GetRevision() == StatsRevisionBeforeReject
            && HistoryView->GetRevision() == HistoryRevisionBeforeReject
            && StatsView->GetRegion(TEXT("main")) == StatsMain
            && HistoryView->GetRepeat(TEXT("astar-history-records")) == HistoryRepeat
            && DebuggerWindow->_StatsPanel->_AuthoredText.FindRef(TEXT("astar-stats-cell-g")).ToString() == TEXT("2.0")
            && FirstRecordText(DebuggerWindow->_SearchHistory->Get_AuthoredCollection(), TEXT("frame")) == TEXT("F#22"));

    const TSharedPtr<SCkAStarDebugger_GridView> HeldGrid = DebuggerWindow->_GridView;
    const TSharedPtr<SCkAStarDebugger_StatsPanel> HeldStats = DebuggerWindow->_StatsPanel;
    const TSharedPtr<SCkAStarDebugger_SearchHistory> HeldHistory = DebuggerWindow->_SearchHistory;
    const TSharedPtr<FCkAStarDebugger_ViewModel> HeldModel = DebuggerWindow->_ViewModel;
    const TWeakPtr<FCkUiView> ReleasedStatsView = StatsView;
    const TWeakPtr<FCkUiView> ReleasedHistoryView = HistoryView;
    const FGeometry HeldGeometry = HeldGrid->GetCachedGeometry();
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    DebuggerWindow.Reset();
    View.Reset();
    StatsView.Reset();
    HistoryView.Reset();
    TestFalse(TEXT("authored view releases while its mounted subtree is held"), ReleasedView.IsValid());
    TestTrue(TEXT("held stats and history roots release their authored views with the production window"),
        NOT ReleasedStatsView.IsValid() && NOT ReleasedHistoryView.IsValid()
            && NOT HeldStats->Get_AuthoredView().IsValid() && NOT HeldHistory->Get_AuthoredView().IsValid());
    StatsCopy->SlatePrepass();
    HistoryCopy->SlatePrepass();
    FPlatformApplicationMisc::ClipboardCopy(TEXT("released-actions-inert"));
    StaticCastSharedPtr<SButton>(StatsCopy)->SimulateClick();
    StaticCastSharedPtr<SButton>(HistoryCopy)->SimulateClick();
    FString ClipboardAfterRelease;
    FPlatformApplicationMisc::ClipboardPaste(ClipboardAfterRelease);
    TestTrue(TEXT("held authored stats and history actions revoke dispatch after owner release"),
        StatsCopy.IsValid() && HistoryCopy.IsValid()
            && ClipboardAfterRelease == TEXT("released-actions-inert"));
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
