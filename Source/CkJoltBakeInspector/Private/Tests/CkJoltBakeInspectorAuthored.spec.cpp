#include "CkJoltBakeInspector/CkJoltBakeInspector_Module.h"
#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"
#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"

#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkEditorTools/Settings/CkStyleSettings.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSplitter.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/SListView.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// This is deliberately a friend-only fixture seam: production never accepts injected audits.
struct FCkJoltBakeInspectorAuthoredTestAccess
{
    static auto Install_ValueOnlyRows(
        SCkJoltBakeInspectorWindow& InWindow,
        const TArray<SCkJoltBakeInspectorWindow::FRowPtr>& InRows)
        -> void
    {
        InWindow._AllRows = InRows;
        InWindow._VisibleRows = InRows;
        InWindow._SelectedRow.Reset();
        InWindow._ListView->RequestListRefresh();
        InWindow._Preview->Clear();
    }

    static auto Get_SelectedRow(const SCkJoltBakeInspectorWindow& InWindow)
        -> SCkJoltBakeInspectorWindow::FRowPtr
    { return InWindow._SelectedRow; }

    static auto Get_VisibleCount(const SCkJoltBakeInspectorWindow& InWindow) -> int32
    { return InWindow._VisibleRows.Num(); }

    static auto Poll_AuthoredPresentation(SCkJoltBakeInspectorWindow& InWindow) -> void
    { InWindow.PollAuthoredPresentation(FPlatformTime::Seconds() + 1.0); }

    static auto Get_NativeChrome(const SCkJoltBakeInspectorWindow& InWindow) -> TSharedPtr<SCkDebug_WindowChrome>
    { return InWindow._NativeChrome; }

    static auto Is_Analyzing(const SCkJoltBakeInspectorWindow& InWindow) -> bool
    { return InWindow.GetIsAnalyzing(); }

    static auto Has_AnalysisLease(const SCkJoltBakeInspectorWindow& InWindow) -> bool
    { return InWindow._AnalysisJoltLease.IsValid(); }

    static auto Get_AnalysisQueueCount(const SCkJoltBakeInspectorWindow& InWindow) -> int32
    { return InWindow._AnalysisQueue.Num(); }

    static auto Get_LegendView(const SCkJoltBakeInspectorPreview& InPreview) -> TSharedPtr<FCkUiView>
    { return InPreview._LegendView; }

    static auto Get_Window(const FCkJoltBakeInspectorModule& InModule)
        -> TSharedPtr<SCkJoltBakeInspectorWindow>
    { return InModule._Window; }

    static auto InvokePreExit(FCkJoltBakeInspectorModule& InModule) -> void
    { InModule.HandleEnginePreExit(); }
};

namespace ck_jolt_bake_inspector_authored_tests
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        if (NOT Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SButton") || Tagged->GetTypeAsString() == TEXT("SCkUiStyledButton"))
        { return StaticCastSharedPtr<SButton>(Tagged); }
        FChildren* Children = Tagged->GetChildren();
        for (auto Index = int32{0}; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedRef<SWidget> Child = ConstCastSharedRef<SWidget>(Children->GetChildAt(Index));
            if (Child->GetTypeAsString() == TEXT("SButton") || Child->GetTypeAsString() == TEXT("SCkUiStyledButton"))
            { return StaticCastSharedRef<SButton>(Child); }
        }
        return {};
    }

    auto TaggedText(const TSharedRef<SWidget>& InRoot, const FName InTag) -> FString
    {
        const TSharedPtr<SWidget> Widget = FindTagged(InRoot, InTag);
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetText().ToString()
            : FString{};
    }

    auto TaggedFontSize(const TSharedRef<SWidget>& InRoot, const FName InTag) -> int32
    {
        const TSharedPtr<SWidget> Widget = FindTagged(InRoot, InTag);
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetFont().Size
            : INDEX_NONE;
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

    auto RemoveNativePort(const FString& InMarkup, const FString& InId, const FString& InBind) -> FString
    {
        const FString Native = FString::Printf(TEXT("<native id=\"%s\" bind=\"%s\" class=\"jolt-bake-%s\" />"),
            *InId, *InBind, *InBind.RightChop(FString{TEXT("jolt-bake-")}.Len()));
        return InMarkup.Replace(*Native, TEXT(""), ESearchCase::CaseSensitive);
    }

    struct FScopedJoltBakeStyle
    {
        FScopedJoltBakeStyle()
        {
            if (const auto* Settings = GetDefault<UCk_Style_UserSettings_UE>())
            { _FontSizeH3 = Settings->FontSizeH3; }
        }

        ~FScopedJoltBakeStyle()
        {
            if (auto* Settings = GetMutableDefault<UCk_Style_UserSettings_UE>())
            { Settings->FontSizeH3 = _FontSizeH3; }
        }

        int32 _FontSizeH3 = INDEX_NONE;
    };

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (auto Index = int32{0}; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto DescribePath(const FWidgetPath& InPath) -> FString
    {
        auto Entries = TArray<FString>{};
        for (auto Index = int32{0}; Index < InPath.Widgets.Num(); ++Index)
        {
            const FArrangedWidget& Arranged = InPath.Widgets[Index];
            Entries.Add(FString::Printf(TEXT("%s(tag=%s)"), *Arranged.Widget->GetTypeAsString(),
                *Arranged.Widget->GetTag().ToString()));
        }
        return FString::Join(Entries, TEXT(" > "));
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget,
        FAutomationTestBase& InTest, const TCHAR* InDiagnosticName, const TSharedPtr<FCkUiView>& InView) -> bool
    {
        FWidgetPath AncestorPath;
        if (InSlate.GeneratePathToWidgetUnchecked(InWidget, AncestorPath))
        {
            for (auto Index = int32{0}; Index < AncestorPath.Widgets.Num(); ++Index)
            {
                const TSharedRef<SWidget> Ancestor = AncestorPath.Widgets[Index].Widget;
                if (Ancestor->GetTypeAsString().Contains(TEXT("ScrollBox")))
                {
                    StaticCastSharedRef<SScrollBox>(Ancestor)->ScrollDescendantIntoView(
                        InWidget, false, EDescendantScrollDestination::IntoView);
                }
            }
            TickSlate(InSlate);
        }
        const int64 RevisionBefore = InView.IsValid() ? InView->GetRevision() : INDEX_NONE;
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid())
        {
            InTest.AddInfo(FString::Printf(TEXT("Click[%s] unavailable: target=%s tag=%s geometry=(%.1f,%.1f) window=%d native=%d"),
                InDiagnosticName, *InWidget->GetTypeAsString(), *InWidget->GetTag().ToString(),
                InWidget->GetCachedGeometry().GetLocalSize().X, InWidget->GetCachedGeometry().GetLocalSize().Y, Window.IsValid(),
                Window.IsValid() && Window->GetNativeWindow().IsValid()));
            return false;
        }

        Window->BringToFront(true);
        InWidget->SlatePrepass();
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f)
        {
            InTest.AddInfo(FString::Printf(TEXT("Click[%s] unavailable after foreground prepass: target=%s tag=%s geometry=(%.1f,%.1f)"),
                InDiagnosticName, *InWidget->GetTypeAsString(), *InWidget->GetTag().ToString(),
                Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y));
            return false;
        }

        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position,
            LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(Path, InWidget))
        {
            InTest.AddInfo(FString::Printf(TEXT("Click[%s] pre-hit misses target=%s tag=%s geometry=(%.1f,%.1f) position=(%.1f,%.1f) path=%s"),
                InDiagnosticName, *InWidget->GetTypeAsString(), *InWidget->GetTag().ToString(),
                Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y, Position.X, Position.Y, *DescribePath(Path)));
            return false;
        }
        const bool Handled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        const FWidgetPath UpPath = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        const bool UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        TickSlate(InSlate);
        const bool Succeeded = Handled && UpHandled && WidgetPathContains(UpPath, InWidget);
        if (NOT Succeeded)
        {
            const bool InActions = InView.IsValid() && ContainsWidget(InView->GetRegion(TEXT("actions")), InWidget);
            const bool InMain = InView.IsValid() && ContainsWidget(InView->GetRegion(TEXT("main")), InWidget);
            InTest.AddInfo(FString::Printf(TEXT("Click[%s] failed: target=%s tag=%s geometry=(%.1f,%.1f) down=%d up=%d captured=%d revision=%lld->%lld current(actions=%d main=%d) pre-path=%s up-path=%s"),
                InDiagnosticName, *InWidget->GetTypeAsString(), *InWidget->GetTag().ToString(),
                Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y, Handled, UpHandled, InWidget->HasMouseCapture(),
                RevisionBefore, InView.IsValid() ? InView->GetRevision() : INDEX_NONE, InActions, InMain,
                *DescribePath(Path), *DescribePath(UpPath)));
        }
        return Succeeded;
    }

    auto MakeValueOnlyRow(const FString& InName, const bool InUsesHeuristic, const bool InWouldFailBake)
        -> SCkJoltBakeInspectorWindow::FRowPtr
    {
        const auto Row = MakeShared<FCkJoltBakeInspectorRow>();
        Row->DisplayName = InName;
        Row->PackagePath = TEXT("/Game/Tests/") + InName;
        Row->Classification = TEXT("Rebuild stale shape");
        Row->Detail = TEXT("value-only audit fixture");
        auto Audit = ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult{};
        Audit._SourcePreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
            FVector::ZeroVector, FVector{100.0f, 0.0f, 0.0f}, FVector{0.0f, 100.0f, 0.0f}});
        Audit._NormalizedPreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
            FVector::ZeroVector, FVector{0.0f, 100.0f, 0.0f}, FVector{100.0f, 0.0f, 0.0f}});
        Audit._CookedPreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
            FVector::ZeroVector, FVector{0.0f, 0.0f, 100.0f}, FVector{100.0f, 0.0f, 0.0f}});
        Audit._CookedPreviewAvailability = ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedPreviewAvailability::Available;
        Audit._bWouldUseHeuristic = InUsesHeuristic;
        Audit._bWouldFailBake = InWouldFailBake;
        Row->Audit = MoveTemp(Audit);
        return Row;
    }

    auto MakeUnresolvedRows(const int32 InCount) -> TArray<SCkJoltBakeInspectorWindow::FRowPtr>
    {
        auto Rows = TArray<SCkJoltBakeInspectorWindow::FRowPtr>{};
        Rows.Reserve(InCount);
        for (auto Index = 0; Index < InCount; ++Index)
        {
            const auto Row = MakeShared<FCkJoltBakeInspectorRow>();
            Row->DisplayName = FString::Printf(TEXT("UnresolvedAnalysisFixture%d"), Index);
            Row->PackagePath = TEXT("/Game/Tests/UnresolvedAnalysisFixture");
            Row->Classification = TEXT("Unresolved fixture");
            Row->Detail = TEXT("Default FAssetData must be rejected before mesh analysis.");
            Rows.Add(Row);
        }
        return Rows;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspector_AuthoredPresentation,
    "Ck.Jolt.Cook.Inspector.Authored.Presentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspector_AuthoredPresentation::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_bake_inspector_authored_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Jolt Bake Inspector authored presentation test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkJoltBakeInspectorWindow> Window = SNew(SCkJoltBakeInspectorWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1200.0f, 820.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Window.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    TSharedPtr<FCkUiView> View = Window->Get_AuthoredView();
    if (NOT TestTrue(TEXT("production Jolt Bake Inspector admits installed authored resources"),
        View.IsValid() && View->GetLastResult().Succeeded && NOT Window->IsUsingNativeFallback()))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    const auto OrdinaryRow = MakeValueOnlyRow(TEXT("AuthoredFixtureA"), false, false);
    const auto HeuristicRow = MakeValueOnlyRow(TEXT("AuthoredFixtureB"), true, false);
    const auto SelectedRow = MakeValueOnlyRow(TEXT("AuthoredFixtureC"), false, true);
    FCkJoltBakeInspectorAuthoredTestAccess::Install_ValueOnlyRows(*Window,
        {OrdinaryRow, HeuristicRow, SelectedRow});
    TickSlate(Slate);
    const TSharedPtr<SCkJoltBakeInspectorPreview> Preview = Window->Get_NativePreview();
    const TSharedPtr<SListView<SCkJoltBakeInspectorWindow::FRowPtr>> List = Window->Get_NativeList();
    const TSharedPtr<SCkDebug_SearchBar> Search = Window->Get_NativeSearch();
    const TSharedPtr<SCkUiSplitter> MainSplit = View->GetSplitter(TEXT("jolt-bake-main-split"));
    const TSharedPtr<SCkUiSplitter> DetailSplit = View->GetSplitter(TEXT("jolt-bake-detail-split"));
    if (NOT TestTrue(TEXT("authored shell mounts exact production search, list, and preview ports"),
        Preview.IsValid() && List.IsValid() && Search.IsValid()
            && NOT Preview->Get_RenderedBounds().IsValid))
    { return false; }
    TestTrue(TEXT("authored metrics project live pre-audited row counts through the installed resource"),
        TaggedText(View->GetRegion(TEXT("main")), TEXT("jolt-bake-inventory")) == TEXT("3")
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("jolt-bake-analyzed")) == TEXT("3")
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("jolt-bake-heuristic")) == TEXT("1")
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("jolt-bake-fail")) == TEXT("1"));
    const int32 InitialMetricFontSize = TaggedFontSize(View->GetRegion(TEXT("main")), TEXT("jolt-bake-inventory"));
    TestEqual(TEXT("authored metric uses the current Ck style H3 size"), InitialMetricFontSize, CkStyle::FontSizeH3());
    {
        const FScopedJoltBakeStyle StyleGuard;
        if (auto* StyleSettings = GetMutableDefault<UCk_Style_UserSettings_UE>(); StyleSettings != nullptr)
        {
            StyleSettings->FontSizeH3 = StyleGuard._FontSizeH3 < 24 ? StyleGuard._FontSizeH3 + 1 : StyleGuard._FontSizeH3 - 1;
            FCkJoltBakeInspectorAuthoredTestAccess::Poll_AuthoredPresentation(*Window);
            TickSlate(Slate);
            TestTrue(TEXT("live Ck style token reload changes existing authored metric typography"),
                TaggedFontSize(View->GetRegion(TEXT("main")), TEXT("jolt-bake-inventory")) != InitialMetricFontSize);
        }
        else { AddError(TEXT("Ck style user settings are unavailable for the live token assertion.")); }
    }

    const TSharedRef<SWidget> Actions = View->GetRegion(TEXT("actions"));
    const TSharedPtr<SButton> FilterAll = FindButton(Actions, TEXT("jolt-bake-filter-all"));
    const TSharedPtr<SButton> FilterHeuristic = FindButton(Actions, TEXT("jolt-bake-filter-heuristic"));
    const TSharedPtr<SButton> FilterFail = FindButton(Actions, TEXT("jolt-bake-filter-fail"));
    TestTrue(TEXT("authored shell exposes physical non-bake filter actions"),
        FilterAll.IsValid() && FilterHeuristic.IsValid() && FilterFail.IsValid());
    TestTrue(TEXT("physical heuristic filter retains only its audited fixture row"),
        FilterHeuristic.IsValid() && Click(Slate, FilterHeuristic.ToSharedRef(), *this, TEXT("heuristic-filter"), View)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_VisibleCount(*Window) == 1);
    const int64 WouldFailRevisionBefore = View->GetRevision();
    const bool WouldFailClicked = FilterFail.IsValid()
        && Click(Slate, FilterFail.ToSharedRef(), *this, TEXT("would-fail-filter"), View);
    const int32 WouldFailVisibleCount = FCkJoltBakeInspectorAuthoredTestAccess::Get_VisibleCount(*Window);
    TestTrue(TEXT("physical would-fail filter routes its click"), WouldFailClicked);
    TestEqual(TEXT("physical would-fail filter leaves exactly one visible row"), WouldFailVisibleCount, 1);
    AddInfo(FString::Printf(TEXT("WouldFail filter diagnostic: revision=%lld->%lld visible=%d target-current-actions=%d"),
        WouldFailRevisionBefore, View->GetRevision(),
        WouldFailVisibleCount, FilterFail.IsValid() && ContainsWidget(View->GetRegion(TEXT("actions")), FilterFail.ToSharedRef())));
    TestTrue(TEXT("physical all filter restores all distinct fixture rows"),
        FilterAll.IsValid() && Click(Slate, FilterAll.ToSharedRef(), *this, TEXT("all-filter"), View)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_VisibleCount(*Window) == 3);

    const TSharedPtr<ITableRow> RowWidget = List->WidgetFromItem(SelectedRow);
    TestTrue(TEXT("physically routed production list selection preserves the pre-audited identity"),
        RowWidget.IsValid() && Click(Slate, RowWidget->AsWidget(), *this, TEXT("selected-row"), View)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_SelectedRow(*Window) == SelectedRow
            && Preview->Get_RenderedBounds().IsValid != 0);
    TestTrue(TEXT("native preview retains its installed authored source-normalized-cooked legend"),
        FindTagged(Preview.ToSharedRef(), TEXT("jolt-bake-legend-source")).IsValid()
            && FindTagged(Preview.ToSharedRef(), TEXT("jolt-bake-legend-normalized")).IsValid()
            && FindTagged(Preview.ToSharedRef(), TEXT("jolt-bake-legend-cooked")).IsValid());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString Directory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Jolt Bake authored resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("JoltBakeInspector.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Directory, TEXT("JoltBakeInspector.ui.css")))))
    { return false; }
    FString LegendMarkup;
    FString LegendStylesheet;
    TSharedPtr<FCkUiView> LegendView = FCkJoltBakeInspectorAuthoredTestAccess::Get_LegendView(*Preview);
    if (LegendView.IsValid() && NOT LegendView->GetLastResult().Succeeded)
    { AddError(FString::Join(LegendView->GetLastResult().Errors, TEXT("\n"))); }
    if (NOT TestTrue(TEXT("nested preview legend admits its installed authored resource"), LegendView.IsValid()
        && LegendView->GetLastResult().Succeeded
        && FFileHelper::LoadFileToString(LegendMarkup, *FPaths::Combine(Directory, TEXT("JoltBakeInspectorLegend.ui.html")))
        && FFileHelper::LoadFileToString(LegendStylesheet, *FPaths::Combine(Directory, TEXT("JoltBakeInspectorLegend.ui.css")))))
    { return false; }
    const int64 LegendRevision = LegendView->GetRevision();
    const FCkUiLoadResult LegendAccepted = LegendView->TryReload(LegendMarkup, LegendStylesheet,
        TEXT("Jolt Bake compatible nested legend candidate"));
    const TSharedRef<SWidget> LegendBeforeReject = LegendView->GetRegion(TEXT("legend"));
    const int64 LegendRevisionBeforeReject = LegendView->GetRevision();
    const FCkUiLoadResult LegendRejected = LegendView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"legend\"><text bind=\"missing-jolt-legend\" /></region></ui>"),
        TEXT(""), TEXT("Jolt Bake rejected nested legend candidate"));
    TestTrue(TEXT("nested legend compatible reload advances once and rejected binding remains atomic"),
        LegendAccepted.Succeeded && LegendView->GetRevision() > LegendRevision
            && NOT LegendRejected.Succeeded && LegendView->GetRevision() == LegendRevisionBeforeReject
            && LegendView->GetRegion(TEXT("legend")) == LegendBeforeReject
            && FindTagged(LegendView->GetRegion(TEXT("legend")), TEXT("jolt-bake-legend-source")).IsValid());
    const TSharedPtr<SWidget> HeldLegendSource = FindTagged(
        LegendView->GetRegion(TEXT("legend")), TEXT("jolt-bake-legend-source"));
    LegendView.Reset();

    const FString FallbackDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        TEXT("JoltBakeInspectorAuthored"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString FallbackMarkupPath = FPaths::Combine(FallbackDirectory, TEXT("JoltBakeInspector.ui.html"));
    const FString FallbackStylesheetPath = FPaths::Combine(FallbackDirectory, TEXT("JoltBakeInspector.ui.css"));
    if (NOT TestTrue(TEXT("fallback fixture creates its isolated temporary resource directory"),
        IFileManager::Get().MakeDirectory(*FallbackDirectory, true)
            && FFileHelper::SaveStringToFile(RemoveNativePort(Markup, TEXT("jolt-bake-preview-native"), TEXT("jolt-bake-preview")), *FallbackMarkupPath)
            && FFileHelper::SaveStringToFile(Stylesheet, *FallbackStylesheetPath)))
    { return false; }
    TSharedPtr<SWindow> FallbackHost;
    ON_SCOPE_EXIT
    {
        if (FallbackHost.IsValid()) { Slate.DestroyWindowImmediately(FallbackHost.ToSharedRef()); }
        IFileManager::Get().Delete(*FallbackMarkupPath, false, true, true);
        IFileManager::Get().Delete(*FallbackStylesheetPath, false, true, true);
        IFileManager::Get().DeleteDirectory(*FallbackDirectory, false, true);
    };

    TSharedPtr<SCkJoltBakeInspectorWindow> FallbackWindow = SNew(SCkJoltBakeInspectorWindow)
        .TestResourceDirectory(FallbackDirectory);
    FallbackHost = SNew(SWindow)
        .ClientSize(FVector2D{900.0f, 640.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [FallbackWindow.ToSharedRef()];
    Slate.AddWindow(FallbackHost.ToSharedRef(), true);
    TickSlate(Slate);
    const TSharedPtr<SCkJoltBakeInspectorPreview> FallbackPreview = FallbackWindow->Get_NativePreview();
    const TSharedPtr<SListView<SCkJoltBakeInspectorWindow::FRowPtr>> FallbackList = FallbackWindow->Get_NativeList();
    const TSharedPtr<SCkDebug_SearchBar> FallbackSearch = FallbackWindow->Get_NativeSearch();
    const TSharedPtr<SCkDebug_WindowChrome> FallbackChrome = FCkJoltBakeInspectorAuthoredTestAccess::Get_NativeChrome(*FallbackWindow);
    if (NOT TestTrue(TEXT("a valid-but-missing startup port selects the complete native fallback"),
        FallbackWindow->IsUsingNativeFallback() && FallbackWindow->Get_AuthoredView().IsValid()
            && NOT FallbackWindow->Get_AuthoredView()->GetLastResult().Succeeded
            && FallbackPreview.IsValid() && FallbackList.IsValid() && FallbackSearch.IsValid()
            && FallbackChrome.IsValid()))
    { return false; }
    const auto FallbackSelectedRow = MakeValueOnlyRow(TEXT("FallbackValueOnly"), false, false);
    FCkJoltBakeInspectorAuthoredTestAccess::Install_ValueOnlyRows(*FallbackWindow, {FallbackSelectedRow});
    TickSlate(Slate);
    const TSharedPtr<ITableRow> FallbackRowWidget = FallbackList->WidgetFromItem(FallbackSelectedRow);
    TestTrue(TEXT("physical fallback list selection remains routed without audit or bake work"),
        FallbackRowWidget.IsValid() && Click(Slate, FallbackRowWidget->AsWidget(), *this, TEXT("fallback-row"),
            FallbackWindow->Get_AuthoredView())
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_SelectedRow(*FallbackWindow) == FallbackSelectedRow
            && FallbackPreview->Get_RenderedBounds().IsValid != 0);
    Slate.SetKeyboardFocus(FallbackList.ToSharedRef(), EFocusCause::SetDirectly);
    TestTrue(TEXT("fallback owns focused native list before the recovered chrome releases stale input"),
        Slate.GetUserFocusedWidget(0) == FallbackList);
    if (NOT TestTrue(TEXT("fallback fixture replaces only its isolated source with the installed valid shell"),
        FFileHelper::SaveStringToFile(Markup, *FallbackMarkupPath))) { return false; }
    FCkJoltBakeInspectorAuthoredTestAccess::Poll_AuthoredPresentation(*FallbackWindow);
    TickSlate(Slate);
    const TSharedPtr<SCkDebug_WindowChrome> RecoveredChrome = FCkJoltBakeInspectorAuthoredTestAccess::Get_NativeChrome(*FallbackWindow);
    TestTrue(TEXT("changed recovered source transfers the exact fallback ports into one authored chrome"),
        NOT FallbackWindow->IsUsingNativeFallback() && FallbackWindow->Get_AuthoredView()->GetLastResult().Succeeded
            && FallbackWindow->Get_NativePreview() == FallbackPreview
            && FallbackWindow->Get_NativeList() == FallbackList
            && FallbackWindow->Get_NativeSearch() == FallbackSearch
            && RecoveredChrome.IsValid() && RecoveredChrome != FallbackChrome
            && Slate.GetUserFocusedWidget(0) != FallbackList
            && ContainsWidget(FallbackWindow->Get_AuthoredView()->GetRegion(TEXT("main")), FallbackPreview.ToSharedRef())
            && ContainsWidget(FallbackWindow->Get_AuthoredView()->GetRegion(TEXT("main")), FallbackList.ToSharedRef())
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_SelectedRow(*FallbackWindow) == FallbackSelectedRow);
    FCkJoltBakeInspectorAuthoredTestAccess::Poll_AuthoredPresentation(*FallbackWindow);
    TestTrue(TEXT("idle recovery poll neither rebuilds authored chrome nor reparents transferred ports"),
        FCkJoltBakeInspectorAuthoredTestAccess::Get_NativeChrome(*FallbackWindow) == RecoveredChrome
            && FallbackWindow->Get_NativePreview() == FallbackPreview
            && FallbackWindow->Get_NativeList() == FallbackList);
    Slate.DestroyWindowImmediately(FallbackHost.ToSharedRef());
    FallbackHost.Reset();
    FallbackWindow.Reset();
    TickSlate(Slate);
    TestFalse(TEXT("held fallback chrome is detached after its recovered owner and host close"),
        Slate.FindWidgetWindow(FallbackChrome.ToSharedRef()).IsValid());

    const int64 Revision = View->GetRevision();
    const FCkUiLoadResult Accepted = View->TryReload(Markup, Stylesheet, TEXT("Jolt Bake compatible test candidate"));
    TickSlate(Slate);
    TestTrue(TEXT("compatible Jolt Bake reload retains exact native renderer, list, search, and selected state"),
        Accepted.Succeeded && View->GetRevision() > Revision
            && Window->Get_NativePreview() == Preview && Window->Get_NativeList() == List
            && Window->Get_NativeSearch() == Search
            && View->GetSplitter(TEXT("jolt-bake-main-split")) == MainSplit
            && View->GetSplitter(TEXT("jolt-bake-detail-split")) == DetailSplit
            && ContainsWidget(View->GetRegion(TEXT("main")), Preview.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), List.ToSharedRef())
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_SelectedRow(*Window) == SelectedRow
            && Preview->Get_RenderedBounds().IsValid != 0);

    for (const TPair<FString, FString>& RequiredPort : TArray<TPair<FString, FString>>{
        {TEXT("jolt-bake-search-native"), TEXT("jolt-bake-search")},
        {TEXT("jolt-bake-list-native"), TEXT("jolt-bake-list")},
        {TEXT("jolt-bake-preview-native"), TEXT("jolt-bake-preview")},
    })
    {
        const TSharedRef<SWidget> MainBeforeReject = View->GetRegion(TEXT("main"));
        const int64 RevisionBeforeReject = View->GetRevision();
        const FCkUiLoadResult Rejected = View->TryReload(RemoveNativePort(Markup, RequiredPort.Key, RequiredPort.Value), Stylesheet,
            FString::Printf(TEXT("Jolt Bake missing required port %s"), *RequiredPort.Value));
        TestTrue(FString::Printf(TEXT("removing required native port %s rejects atomically"), *RequiredPort.Value),
            NOT Rejected.Succeeded && View->GetRevision() == RevisionBeforeReject
                && View->GetRegion(TEXT("main")) == MainBeforeReject
                && Window->Get_NativePreview() == Preview && Window->Get_NativeList() == List
                && Window->Get_NativeSearch() == Search
                && View->GetSplitter(TEXT("jolt-bake-main-split")) == MainSplit
                && View->GetSplitter(TEXT("jolt-bake-detail-split")) == DetailSplit
                && ContainsWidget(View->GetRegion(TEXT("main")), Preview.ToSharedRef())
                && ContainsWidget(View->GetRegion(TEXT("main")), List.ToSharedRef())
                && FCkJoltBakeInspectorAuthoredTestAccess::Get_SelectedRow(*Window) == SelectedRow);
    }

    const TWeakPtr<FCkUiView> ReleasedView = View;
    Window->Release_AuthoredPresentation();
    TickSlate(Slate);
    TestTrue(TEXT("held native ports become inert after production owner release"),
        NOT Window->Get_NativePreview().IsValid() && NOT Window->Get_NativeList().IsValid()
            && NOT Window->Get_NativeSearch().IsValid() && Preview->Get_PreviewWorld() == nullptr
            && NOT Preview->Get_RenderedBounds().IsValid
            && NOT FCkJoltBakeInspectorAuthoredTestAccess::Get_LegendView(*Preview).IsValid()
            && HeldLegendSource.IsValid() && TaggedText(HeldLegendSource.ToSharedRef(), TEXT("jolt-bake-legend-source")).IsEmpty());
    View.Reset();
    TestFalse(TEXT("owner release drops the authored view while native widgets are externally held"), ReleasedView.IsValid());

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TSharedPtr<SWindow> HeldPortsHost = SNew(SWindow)
        .ClientSize(FVector2D{560.0f, 160.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[FilterAll.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight()[Search.ToSharedRef()]
        ];
    Slate.AddWindow(HeldPortsHost.ToSharedRef(), true);
    TickSlate(Slate);
    Search->Set_SearchText(TEXT("must-not-refilter-after-release"));
    const bool ReleasedFilterAttempt = Click(Slate, FilterAll.ToSharedRef(), *this, TEXT("released-filter"), View);
    TestTrue(TEXT("rehosted held authored filter is fail-closed and search callback remains inert after owner release"),
        NOT FilterAll->IsEnabled() && NOT ReleasedFilterAttempt && NOT FilterAll->HasMouseCapture()
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_VisibleCount(*Window) == 0);
    Slate.DestroyWindowImmediately(HeldPortsHost.ToSharedRef());
    HeldPortsHost.Reset();
    Window.Reset();
    TickSlate(Slate);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspector_AuthoredModuleLifecycle,
    "Ck.Jolt.Cook.Inspector.Authored.ModuleLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspector_AuthoredModuleLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_bake_inspector_authored_tests;

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Jolt Bake Inspector module lifecycle test requires Slate."));
        return false;
    }
    auto& Slate = FSlateApplication::Get();
    FCkJoltBakeInspectorModule& Module = FCkJoltBakeInspectorModule::Get();
    ON_SCOPE_EXIT
    {
        Module.CloseInspector();
        TickSlate(Slate);
    };
    TestNull(TEXT("default FAssetData resolves no UObject before the active-analysis fixture"),
        FAssetData{}.GetAsset());
    Module.OpenInspector();
    TickSlate(Slate);
    const TSharedPtr<SCkJoltBakeInspectorWindow> FirstWindow = FCkJoltBakeInspectorAuthoredTestAccess::Get_Window(Module);
    if (NOT TestTrue(TEXT("public module open creates the registered production authored window"),
        FirstWindow.IsValid() && FirstWindow->Get_AuthoredView().IsValid()
            && FirstWindow->Get_AuthoredView()->GetLastResult().Succeeded))
    { return false; }
    const TSharedPtr<FCkUiView> FirstView = FirstWindow->Get_AuthoredView();
    FCkJoltBakeInspectorAuthoredTestAccess::Install_ValueOnlyRows(*FirstWindow, MakeUnresolvedRows(128));
    TickSlate(Slate);
    const TSharedPtr<SButton> FirstAnalyzeAll = FindButton(FirstView->GetRegion(TEXT("actions")), TEXT("jolt-bake-analyze-all"));
    if (NOT TestTrue(TEXT("active-analysis lifecycle fixture mounts the real authored Analyze All action"), FirstAnalyzeAll.IsValid()
        && Click(Slate, FirstAnalyzeAll.ToSharedRef(), *this, TEXT("module-close-analyze-all"), FirstView)))
    { return false; }
    TestTrue(TEXT("physical Analyze All starts bounded unresolved work and acquires the Jolt lease"),
        FCkJoltBakeInspectorAuthoredTestAccess::Is_Analyzing(*FirstWindow)
            && FCkJoltBakeInspectorAuthoredTestAccess::Has_AnalysisLease(*FirstWindow)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_AnalysisQueueCount(*FirstWindow) == 128);
    const TSharedPtr<SCkJoltBakeInspectorPreview> HeldClosePreview = FirstWindow->Get_NativePreview();
    Module.CloseInspector();
    TickSlate(Slate);
    TestTrue(TEXT("module close cancels active unresolved analysis and releases its held preview world"),
        NOT FCkJoltBakeInspectorAuthoredTestAccess::Get_Window(Module).IsValid()
            && HeldClosePreview.IsValid() && HeldClosePreview->Get_PreviewWorld() == nullptr
            && NOT FCkJoltBakeInspectorAuthoredTestAccess::Is_Analyzing(*FirstWindow)
            && NOT FCkJoltBakeInspectorAuthoredTestAccess::Has_AnalysisLease(*FirstWindow)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_AnalysisQueueCount(*FirstWindow) == 0);

    Module.OpenInspector();
    TickSlate(Slate);
    const TSharedPtr<SCkJoltBakeInspectorWindow> ReopenedWindow = FCkJoltBakeInspectorAuthoredTestAccess::Get_Window(Module);
    if (NOT TestTrue(TEXT("module can reopen a fresh production Jolt Bake window after close"), ReopenedWindow.IsValid()))
    { return false; }
    const TSharedPtr<FCkUiView> ReopenedView = ReopenedWindow->Get_AuthoredView();
    FCkJoltBakeInspectorAuthoredTestAccess::Install_ValueOnlyRows(*ReopenedWindow, MakeUnresolvedRows(128));
    TickSlate(Slate);
    const TSharedPtr<SButton> ReopenedAnalyzeAll = ReopenedView.IsValid()
        ? FindButton(ReopenedView->GetRegion(TEXT("actions")), TEXT("jolt-bake-analyze-all")) : TSharedPtr<SButton>{};
    if (NOT TestTrue(TEXT("reopened active-analysis fixture starts through the real authored action"), ReopenedAnalyzeAll.IsValid()
        && Click(Slate, ReopenedAnalyzeAll.ToSharedRef(), *this, TEXT("preexit-analyze-all"), ReopenedView)))
    { return false; }
    TestTrue(TEXT("reopened unresolved analysis remains active with its Jolt lease before pre-exit"),
        FCkJoltBakeInspectorAuthoredTestAccess::Is_Analyzing(*ReopenedWindow)
            && FCkJoltBakeInspectorAuthoredTestAccess::Has_AnalysisLease(*ReopenedWindow));
    const TSharedPtr<SCkJoltBakeInspectorPreview> HeldPreExitPreview = ReopenedWindow->Get_NativePreview();
    FCkJoltBakeInspectorAuthoredTestAccess::InvokePreExit(Module);
    TickSlate(Slate);
    TestTrue(TEXT("module pre-exit cancels active unresolved analysis before held preview-world teardown"),
        NOT FCkJoltBakeInspectorAuthoredTestAccess::Get_Window(Module).IsValid()
            && HeldPreExitPreview.IsValid() && HeldPreExitPreview->Get_PreviewWorld() == nullptr
            && NOT HeldPreExitPreview->Get_RenderedBounds().IsValid
            && NOT FCkJoltBakeInspectorAuthoredTestAccess::Is_Analyzing(*ReopenedWindow)
            && NOT FCkJoltBakeInspectorAuthoredTestAccess::Has_AnalysisLease(*ReopenedWindow)
            && FCkJoltBakeInspectorAuthoredTestAccess::Get_AnalysisQueueCount(*ReopenedWindow) == 0);
    return true;
}

#endif
