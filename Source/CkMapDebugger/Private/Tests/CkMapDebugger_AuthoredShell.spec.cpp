#include "CkMapDebugger/Window/SCkMapDebuggerWindow.h"
#include "../../CkMapDebugger_Module.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkMapDebuggerAuthoredTestAccess
{
    static auto PollAuthoredShell(SCkMapDebuggerWindow& InWindow) -> void
    { InWindow.Poll_AuthoredShell(InWindow._NextAuthoredShellPollSeconds); }
    static auto GetList(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<SListView<TSharedPtr<FCkMapDebug_PoiRow>>>
    { return InWindow.IsValid() ? InWindow->_PoiList : nullptr; }
    static auto GetCanvas(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<SCkMapDebug_Canvas>
    { return InWindow.IsValid() ? InWindow->_Canvas : nullptr; }
    static auto PopulateOne(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<FCkMapDebug_PoiRow>
    {
        if (!InWindow.IsValid() || !InWindow->_PoiList.IsValid()) { return {}; }
        const TSharedPtr<FCkMapDebug_PoiRow> Row = MakeShared<FCkMapDebug_PoiRow>();
        Row->Name = TEXT("Authored POI"); Row->Distance = TEXT("42m");
        Row->Color = FLinearColor{0.2f, 0.7f, 0.4f}; Row->Enabled = true; Row->HighlightMatch = true;
        InWindow->_AllRows = {Row}; InWindow->_VisibleRows = {Row};
        InWindow->_PoiList->RebuildList();
        InWindow->_PoiList->RequestScrollIntoView(Row);
        return Row;
    }
    static auto GetSearch(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<SCkDebug_DualSearchBar>
    { return InWindow.IsValid() ? InWindow->_SearchBar : nullptr; }
    static auto GetEntity(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<SCkDebug_EntityRef>
    { return InWindow.IsValid() ? InWindow->_SelectedEntity : nullptr; }
    static auto GetToggle(const TSharedPtr<SCkMapDebuggerWindow>& InWindow) -> TSharedPtr<SCkDebug_IconToggle>
    { return InWindow.IsValid() ? InWindow->_EnabledPoisToggle : nullptr; }
    static auto InvokeMouseButtonDown(const TSharedPtr<SCkMapDebug_Canvas>& InCanvas, const FGeometry& InGeometry,
        const FPointerEvent& InEvent) -> FReply
    { return InCanvas.IsValid() ? InCanvas->OnMouseButtonDown(InGeometry, InEvent) : FReply::Unhandled(); }
    static auto InvokeMouseWheel(const TSharedPtr<SCkMapDebug_Canvas>& InCanvas, const FGeometry& InGeometry,
        const FPointerEvent& InEvent) -> FReply
    { return InCanvas.IsValid() ? InCanvas->OnMouseWheel(InGeometry, InEvent) : FReply::Unhandled(); }
    static auto GetZoom(const TSharedPtr<SCkMapDebug_Canvas>& InCanvas) -> float
    { return InCanvas.IsValid() ? InCanvas->_Zoom : 0.0f; }
    static auto GetPanOffset(const TSharedPtr<SCkMapDebug_Canvas>& InCanvas) -> FVector2D
    { return InCanvas.IsValid() ? InCanvas->_PanOffset : FVector2D::ZeroVector; }
};

struct FCkMapDebuggerModuleTestAccess
{
    static auto GetWindow(FCkMapDebuggerModule& InModule) -> TSharedPtr<SCkMapDebuggerWindow> { return InModule._DebuggerWindow; }
    static auto PreExit(FCkMapDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
    static auto GetTab(FCkMapDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
};

namespace ck_map_debugger_authored_shell_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag))
            { return Found; }
        }
        return {};
    }

    auto ReadInstalledShell(FString& OutMarkup, FString& OutStylesheet) -> bool
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (!Plugin.IsValid()) { return false; }
        const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        return FFileHelper::LoadFileToString(OutMarkup, *FPaths::Combine(Directory, TEXT("MapDebugger.ui.html")))
            && FFileHelper::LoadFileToString(OutStylesheet, *FPaths::Combine(Directory, TEXT("MapDebugger.ui.css")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkMapDebuggerAuthoredPortsAndReload,
    "Ck.MapDebugger.Authored.PortsAndReload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkMapDebuggerAuthoredPortsAndReload::RunTest(const FString&) -> bool
{
    using namespace ck_map_debugger_authored_shell_tests;
    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Map authored-shell test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };

    TSharedPtr<SCkMapDebuggerWindow> Window = SNew(SCkMapDebuggerWindow);
    Host = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{1120.0f, 700.0f})
        .CreateTitleBar(false).HasCloseButton(false)[Window.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = Window->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Production Map shell admits one retained authored view"),
        View.IsValid() && View->GetLastResult().Succeeded && !Window->IsUsingNativeShellFallback()))
    {
        AddError(Window->Get_AuthoredShellLoadFailure());
        return false;
    }

    const TSharedRef<SWidget> Actions = View->GetRegion(TEXT("actions"));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const TArray<FName> RequiredIds{TEXT("map-topology"), TEXT("map-rail"), TEXT("map-list"),
        TEXT("map-canvas"), TEXT("map-detail"), TEXT("map-entity"), TEXT("map-status")};
    for (const FName Id : RequiredIds)
    {
        TestTrue(*FString::Printf(TEXT("Authored Map shell owns exact port/topology id '%s'"), *Id.ToString()),
            FindTaggedWidget(Main, Id).IsValid() || FindTaggedWidget(View->GetRegion(TEXT("status")), Id).IsValid());
    }
    TestTrue(TEXT("Authored Map toolbar owns exact enabled-POIs port"), FindTaggedWidget(Actions, TEXT("map-enabled")).IsValid());

    FString Markup;
    FString Stylesheet;
    if (!TestTrue(TEXT("Installed Map shell resources are readable"), ReadInstalledShell(Markup, Stylesheet)))
    { return false; }

    const int64 RevisionBeforeAcceptedReload = View->GetRevision();
    TestTrue(TEXT("Compatible Map shell reload succeeds"), View->TryReload(Markup, Stylesheet, TEXT("Map shell compatible candidate")).Succeeded);
    Tick(Slate);
    TestTrue(TEXT("Accepted reload retains the production view and advances its revision"),
        Window->Get_AuthoredShellView() == View && View->GetRevision() > RevisionBeforeAcceptedReload);

    const TSharedRef<SWidget> MainBeforeRejectedReload = View->GetRegion(TEXT("main"));
    const int64 RevisionBeforeRejectedReload = View->GetRevision();
    TestFalse(TEXT("Missing native port is rejected atomically"), View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"map-search\" bind=\"map-search\"/></region></ui>"),
        TEXT(""), TEXT("Map shell missing-port candidate")).Succeeded);
    TestTrue(TEXT("Rejected Map shell retains accepted tree and revision"),
        View->GetRegion(TEXT("main")) == MainBeforeRejectedReload && View->GetRevision() == RevisionBeforeRejectedReload);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkMapDebuggerAuthoredRelease,
    "Ck.MapDebugger.Authored.CloseReopenPreExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkMapDebuggerAuthoredRelease::RunTest(const FString&) -> bool
{
    using namespace ck_map_debugger_authored_shell_tests;
    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Map authored-release test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };

    TSharedPtr<SCkMapDebuggerWindow> Window = SNew(SCkMapDebuggerWindow);
    Host = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{1120.0f, 700.0f})
        .CreateTitleBar(false).HasCloseButton(false)[Window.ToSharedRef()];
    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> HeldView = Window->Get_AuthoredShellView();
    if (!TestTrue(TEXT("Map shell is live before close/pre-exit release"), HeldView.IsValid())) { return false; }
    Window->Release_Presentation();
    TestFalse(TEXT("Owner release drops the production Map shell reference"), Window->Get_AuthoredShellView().IsValid());
    TestTrue(TEXT("Held authored view remains safe after owner release"), HeldView.IsValid());

    FString Markup;
    FString Stylesheet;
    if (TestTrue(TEXT("Map resources remain readable after release"), ReadInstalledShell(Markup, Stylesheet)))
    {
        TestTrue(TEXT("Held view can reload without dispatching owner actions after release"),
            HeldView->TryReload(Markup, Stylesheet, TEXT("Map held-view inert reload")).Succeeded);
    }

    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    HeldView.Reset();
    Window.Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkMapDebuggerAuthoredModuleLifecycle,
    "Ck.MapDebugger.Authored.ModuleOpenCloseOpenPreExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkMapDebuggerAuthoredModuleLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_map_debugger_authored_shell_tests;
    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Map module lifecycle test requires Slate."));
        return false;
    }

    FSlateApplication& Slate = FSlateApplication::Get();
    FCkMapDebuggerModule& Module = FCkMapDebuggerModule::Get();
    Module.CloseDebugger();
    Module.OpenDebugger();
    Tick(Slate);
    TSharedPtr<SCkMapDebuggerWindow> FirstWindow = FCkMapDebuggerModuleTestAccess::GetWindow(Module);
    TSharedPtr<SDockTab> HeldTab = FCkMapDebuggerModuleTestAccess::GetTab(Module);
    TSharedPtr<FCkUiView> HeldView = FirstWindow.IsValid() ? FirstWindow->Get_AuthoredShellView() : nullptr;
    TSharedPtr<SListView<TSharedPtr<FCkMapDebug_PoiRow>>> HeldList = FCkMapDebuggerAuthoredTestAccess::GetList(FirstWindow);
    TSharedPtr<SCkMapDebug_Canvas> HeldCanvas = FCkMapDebuggerAuthoredTestAccess::GetCanvas(FirstWindow);
    if (!TestTrue(TEXT("Loaded Map module opens a production tab"), Module.IsDebuggerOpen() && HeldTab.IsValid() && HeldView.IsValid() && HeldList.IsValid() && HeldCanvas.IsValid()))
    { return false; }

    Module.CloseDebugger();
    Tick(Slate);
    TestFalse(TEXT("Close releases the loaded module tab"), Module.IsDebuggerOpen());
    TestTrue(TEXT("Ordinary close clears retained tab content"), HeldTab->GetContent() == SNullWidget::NullWidget);
    TestEqual(TEXT("Held list is detached from its released item source"), HeldList->GetItems().Num(), 0);

    const FGeometry CanvasGeometry = FGeometry::MakeRoot(FVector2D{480.0f, 420.0f}, FSlateLayoutTransform{});
    const FVector2D CanvasPosition{240.0f, 210.0f};
    const FPointerEvent RightClick(0, CanvasPosition, CanvasPosition, TSet<FKey>{EKeys::RightMouseButton},
        EKeys::RightMouseButton, 0.0f, FModifierKeysState{});
    const FPointerEvent Wheel(0, CanvasPosition, CanvasPosition, TSet<FKey>{}, EKeys::Invalid, 1.0f,
        FModifierKeysState{});
    const float ZoomAfterRelease = FCkMapDebuggerAuthoredTestAccess::GetZoom(HeldCanvas);
    const FVector2D PanAfterRelease = FCkMapDebuggerAuthoredTestAccess::GetPanOffset(HeldCanvas);
    const FReply ReleasedRightClick = FCkMapDebuggerAuthoredTestAccess::InvokeMouseButtonDown(HeldCanvas, CanvasGeometry, RightClick);
    const FReply ReleasedWheel = FCkMapDebuggerAuthoredTestAccess::InvokeMouseWheel(HeldCanvas, CanvasGeometry, Wheel);
    TestTrue(TEXT("Released retained canvas rejects physical right-click and does not capture"),
        NOT ReleasedRightClick.IsEventHandled() && NOT HeldCanvas->HasMouseCapture());
    TestTrue(TEXT("Released retained canvas rejects physical wheel without changing zoom or pan"),
        NOT ReleasedWheel.IsEventHandled() && FCkMapDebuggerAuthoredTestAccess::GetZoom(HeldCanvas) == ZoomAfterRelease
            && FCkMapDebuggerAuthoredTestAccess::GetPanOffset(HeldCanvas) == PanAfterRelease);

    Module.OpenDebugger();
    Tick(Slate);
    TestTrue(TEXT("Loaded Map module reopens after close"), Module.IsDebuggerOpen());
    HeldTab = FCkMapDebuggerModuleTestAccess::GetTab(Module);
    if (!TestTrue(TEXT("Reopened Map module retains a tab for pre-exit validation"), HeldTab.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }
    FCkMapDebuggerModuleTestAccess::PreExit(Module);
    Tick(Slate);
    TestFalse(TEXT("Private pre-exit release drops the loaded module tab"), Module.IsDebuggerOpen());
    TestTrue(TEXT("Private pre-exit handler clears retained tab content"), HeldTab->GetContent() == SNullWidget::NullWidget);

    FString Markup;
    FString Stylesheet;
    if (TestTrue(TEXT("Held-view reload resources are readable"), ReadInstalledShell(Markup, Stylesheet)))
    {
        TestTrue(TEXT("Held authored view remains inert/safe after close and pre-exit"),
            HeldView->TryReload(Markup, Stylesheet, TEXT("Map module lifecycle held view")).Succeeded);
    }
    HeldCanvas.Reset();
    HeldList.Reset();
    HeldView.Reset();
    HeldTab.Reset();
    FirstWindow.Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkMapDebuggerAuthoredStartupRepairAndRow,
    "Ck.MapDebugger.Authored.StartupRepairAndLazyRow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkMapDebuggerAuthoredStartupRepairAndRow::RunTest(const FString&) -> bool
{
    using namespace ck_map_debugger_authored_shell_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Map repair test requires Slate.")); return false; }
    FString Markup; FString Stylesheet;
    if (!TestTrue(TEXT("Installed Map pair is readable"), ReadInstalledShell(Markup, Stylesheet))) { return false; }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString RowMarkup; FString RowStylesheet;
    if (!TestTrue(TEXT("Installed Map POI row pair is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(RowMarkup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/MapDebuggerPoiRow.ui.html")))
        && FFileHelper::LoadFileToString(RowStylesheet, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/MapDebuggerPoiRow.ui.css"))))) { return false; }
    const FString TempDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/MapDebuggerStartupRepair"));
    IFileManager::Get().MakeDirectory(*TempDirectory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*TempDirectory, false, true); };
    FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"bad\" bind=\"bad\"/></region></ui>"), *FPaths::Combine(TempDirectory, TEXT("MapDebugger.ui.html")));
    FFileHelper::SaveStringToFile(TEXT(""), *FPaths::Combine(TempDirectory, TEXT("MapDebugger.ui.css")));
    FFileHelper::SaveStringToFile(RowMarkup, *FPaths::Combine(TempDirectory, TEXT("MapDebuggerPoiRow.ui.html")));
    FFileHelper::SaveStringToFile(RowStylesheet, *FPaths::Combine(TempDirectory, TEXT("MapDebuggerPoiRow.ui.css")));

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host;
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };
    TSharedPtr<SCkMapDebuggerWindow> Window = SNew(SCkMapDebuggerWindow).ResourceDirectoryOverride(TempDirectory);
    Host = SNew(SWindow).ClientSize(FVector2D{1120,700})[Window.ToSharedRef()]; Slate.AddWindow(Host.ToSharedRef(), true); Tick(Slate);
    const auto Search = FCkMapDebuggerAuthoredTestAccess::GetSearch(Window);
    const auto List = FCkMapDebuggerAuthoredTestAccess::GetList(Window);
    const auto Canvas = FCkMapDebuggerAuthoredTestAccess::GetCanvas(Window);
    const auto Entity = FCkMapDebuggerAuthoredTestAccess::GetEntity(Window);
    const auto Toggle = FCkMapDebuggerAuthoredTestAccess::GetToggle(Window);
    TestTrue(TEXT("Malformed same-path startup retains native fallback"), Window->IsUsingNativeShellFallback() && Window->Get_AuthoredShellView().IsValid());

    FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(TempDirectory, TEXT("MapDebugger.ui.html")));
    FFileHelper::SaveStringToFile(Stylesheet, *FPaths::Combine(TempDirectory, TEXT("MapDebugger.ui.css")));
    FCkMapDebuggerAuthoredTestAccess::PollAuthoredShell(*Window);
    TestTrue(TEXT("Same-path repair atomically admits authored shell"), !Window->IsUsingNativeShellFallback());
    TestTrue(TEXT("Repair preserves all five native control identities"),
        Search == FCkMapDebuggerAuthoredTestAccess::GetSearch(Window) && List == FCkMapDebuggerAuthoredTestAccess::GetList(Window)
        && Canvas == FCkMapDebuggerAuthoredTestAccess::GetCanvas(Window) && Entity == FCkMapDebuggerAuthoredTestAccess::GetEntity(Window)
        && Toggle == FCkMapDebuggerAuthoredTestAccess::GetToggle(Window));

    const TSharedPtr<FCkMapDebug_PoiRow> Row = FCkMapDebuggerAuthoredTestAccess::PopulateOne(Window);
    // PopulateOne deliberately injects a derived, value-only row. Keep the production collector tick from
    // replacing that fixture row while Slate performs only the layout/paint pass that realizes the SListView row.
    const bool WindowCanTick = Window->GetCanTick();
    Window->SetCanTick(false);
    Tick(Slate);
    Window->SetCanTick(WindowCanTick);
    TestTrue(TEXT("Lazy production list generation creates authored POI presentation"), Row.IsValid() && Row->Presentation.IsValid());
    const TSharedPtr<FCkUiView> HeldRowView = Row.IsValid() ? Row->Presentation : nullptr;
    TestTrue(TEXT("Authored POI row exposes swatch/name/distance tags"), HeldRowView.IsValid()
        && FindTaggedWidget(HeldRowView->GetRegion(TEXT("main")), TEXT("map-poi-row-swatch")).IsValid()
        && FindTaggedWidget(HeldRowView->GetRegion(TEXT("main")), TEXT("map-poi-row-name")).IsValid()
        && FindTaggedWidget(HeldRowView->GetRegion(TEXT("main")), TEXT("map-poi-row-distance")).IsValid());
    Window->Release_Presentation();
    TestFalse(TEXT("Owner release resets retained row presentation"), Row->Presentation.IsValid());
    TestTrue(TEXT("Held row view remains inert/safe after release"), HeldRowView.IsValid());
    return true;
}

#endif
