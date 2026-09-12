#include "CkUIDebugger/Window/SCkUIDebuggerWindow.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"

#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiTreeCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiTree.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_ui_debugger_history_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindHistoryPanel(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkDebug_InspectorPanel>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel"))
        { return StaticCastSharedRef<SCkDebug_InspectorPanel>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SCkDebug_InspectorPanel> Found = FindHistoryPanel(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag, const FString& InType) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == InType) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag, InType);
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindWidgetByTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SWidget> Found = FindWidgetByTag(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto HasText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool;

    auto HasText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InRoot)->GetText().ToString() == InText) { return true; }
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText) { return true; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (HasText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto CollectText(const TSharedRef<SWidget>& InRoot, TArray<FString>& OutText) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const FString Text = StaticCastSharedRef<SCkFlexText>(InRoot)->GetText().ToString();
            if (!Text.IsEmpty()) { OutText.Add(Text); }
        }
        else if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const FString Text = StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString();
            if (!Text.IsEmpty()) { OutText.Add(Text); }
        }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            CollectText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutText);
        }
    }

    auto IsInside(const FGeometry& InChild, const FGeometry& InParent) -> bool
    {
        const FVector2D TopLeft = InParent.AbsoluteToLocal(InChild.GetAbsolutePosition());
        const FVector2D BottomRight = InParent.AbsoluteToLocal(InChild.LocalToAbsolute(InChild.GetLocalSize()));
        return InChild.GetLocalSize().X > 0.0f && InChild.GetLocalSize().Y > 0.0f
            && TopLeft.X >= -1.0f && TopLeft.Y >= -1.0f
            && BottomRight.X <= InParent.GetLocalSize().X + 1.0f
            && BottomRight.Y <= InParent.GetLocalSize().Y + 1.0f;
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size)
            || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        return FImageUtils::SaveImageByExtension(
            *InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkUIDebugger_HistoryAuthored,
    "Ck.UiAuthoring.UIDebugger.History",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkUIDebugger_HistoryAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_ui_debugger_history_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("UI Debugger history test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SCkUIDebuggerWindow> Panel = SNew(SCkUIDebuggerWindow);
    TSharedPtr<SWindow> HostWindow = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{1200.0f, 760.0f})
        .CreateTitleBar(false).HasCloseButton(false)[Panel.ToSharedRef()];
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    const TSharedPtr<FCkUiView> View = Panel->Get_HistoryView();
    if (!View.IsValid() || !View->GetLastResult().Succeeded)
    {
        TArray<FString> VisibleText;
        CollectText(Panel.ToSharedRef(), VisibleText);
        AddError(FString::Printf(TEXT("Production history view did not load installed resources. Visible text: %s"),
            *FString::Join(VisibleText, TEXT(" | "))));
        return false;
    }
    TSharedPtr<FCkUiView> SummaryView = Panel->Get_SummaryView();
    if (!TestTrue(TEXT("Mounted production summary view loads installed resources"),
        SummaryView.IsValid() && SummaryView->GetLastResult().Succeeded)) { return false; }
    const TSharedRef<SWidget> SummaryRegion = SummaryView->GetRegion(TEXT("summary"));
    const TSharedPtr<SWidget> SummaryLive = FindWidgetByTag(SummaryRegion, TEXT("ui-summary-live"));
    const TSharedPtr<SWidget> SummaryNoLayout = FindWidgetByTag(SummaryRegion, TEXT("ui-summary-empty"));
    if (!TestTrue(TEXT("Mounted authored summary shows no-layout branch and hides live branch"),
        SummaryLive.IsValid() && SummaryNoLayout.IsValid() && !SummaryLive->GetVisibility().IsVisible()
        && SummaryNoLayout->GetVisibility().IsVisible()
        && HasText(SummaryRegion, TEXT("No active layout. Start PIE to see layer data.")))) { return false; }

    const TSharedPtr<FCkUiView> LayerView = Panel->Get_LayerView();
    if (!LayerView.IsValid() || !LayerView->GetLastResult().Succeeded)
    {
        TArray<FString> VisibleText;
        CollectText(Panel.ToSharedRef(), VisibleText);
        AddError(FString::Printf(TEXT("Production layer view did not load installed resources. Visible text: %s"),
            *FString::Join(VisibleText, TEXT(" | "))));
        return false;
    }
    const TSharedPtr<const FCkUiTreeCollection> LayerCollection = Panel->Get_LayerCollection();
    const TSharedPtr<SCkUiTree> LayerTree = LayerView->GetTree(TEXT("ui-layer-tree"));
    if (!TestTrue(TEXT("Mounted production layer view resolves its retained empty tree"),
        LayerCollection.IsValid() && LayerCollection->GetNodes().IsEmpty()
        && LayerTree.IsValid() && LayerTree->GetVisibleNodeCount() == 0)) { return false; }
    const TSharedPtr<STreeView<SCkUiTree::FNode>> NativeLayerTree = LayerTree->GetTree();
    if (!TestTrue(TEXT("Mounted production layer view uses the native tree"), NativeLayerTree.IsValid())) { return false; }
    TSharedPtr<FCkUiView> CommandView = Panel->Get_CommandView();
    if (!TestTrue(TEXT("Mounted production command view loads installed resources"),
        CommandView.IsValid() && CommandView->GetLastResult().Succeeded)) { return false; }
    const TSharedRef<SWidget> CommandRegion = CommandView->GetRegion(TEXT("commands"));
    const TArray<FName> CommandControlIds{
        TEXT("ui-commands-root"),
        TEXT("ui-layer-filter"),
        TEXT("ui-layer-filter-clear"),
        TEXT("ui-active-layer-only"),
        TEXT("ui-force-refresh"),
        TEXT("ui-expand-all"),
        TEXT("ui-collapse-all"),
        TEXT("ui-clear-history"),
        TEXT("ui-name-depth-previous"),
        TEXT("ui-name-depth-value"),
        TEXT("ui-name-depth-next")};
    auto MissingCommandControls = TArray<FName>{};
    for (const FName& Id : CommandControlIds)
    {
        if (!FindWidgetByTag(Panel.ToSharedRef(), Id).IsValid()) { MissingCommandControls.Add(Id); }
    }
    if (!TestTrue(TEXT("Mounted command region installs every named authored control"),
        MissingCommandControls.IsEmpty())) { return false; }
    const TSharedPtr<SWidget> CommandSearchWidget = FindTaggedWidget(
        CommandRegion, TEXT("ui-layer-filter"), TEXT("SSearchBox"));
    if (!TestTrue(TEXT("Mounted command region retains its authored search control"),
        CommandSearchWidget.IsValid() && CommandSearchWidget->GetTypeAsString() == TEXT("SSearchBox"))) { return false; }
    const TSharedPtr<SSearchBox> HeldCommandSearch = StaticCastSharedPtr<SSearchBox>(CommandSearchWidget);
    const TArray<FName> CommandActionIds{
        TEXT("ui-layer-filter-clear"),
        TEXT("ui-active-layer-only"),
        TEXT("ui-force-refresh"),
        TEXT("ui-expand-all"),
        TEXT("ui-collapse-all"),
        TEXT("ui-clear-history"),
        TEXT("ui-name-depth-previous"),
        TEXT("ui-name-depth-next")};
    auto HeldCommandActions = TArray<TSharedPtr<SButton>>{};
    for (const FName& Id : CommandActionIds)
    {
        const TSharedPtr<SWidget> Control = FindTaggedWidget(CommandRegion, Id, TEXT("SCkUiStyledButton"));
        if (!TestTrue(FString::Printf(TEXT("Mounted authored command action '%s' is a button"), *Id.ToString()),
            Control.IsValid() && Control->GetTypeAsString() == TEXT("SCkUiStyledButton"))) { return false; }
        HeldCommandActions.Add(StaticCastSharedPtr<SButton>(Control));
    }
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const TSharedPtr<SCkDebug_InspectorPanel> HistoryPanel = FindHistoryPanel(Region);
    if (!TestTrue(TEXT("Mounted production history resolves its authored inspector"), HistoryPanel.IsValid())) { return false; }
    TestFalse(TEXT("Authored history panel starts collapsed"), HistoryPanel->Is_Expanded());
    HistoryPanel->Set_Expanded(true);
    Tick(Slate);
    const TSharedPtr<SWidget> RepeatWidget = FindTaggedWidget(Region, TEXT("history-records"), TEXT("SCkUiRepeat"));
    const TSharedPtr<SWidget> Scroll = FindTaggedWidget(Region, TEXT("history-scroll"), TEXT("SCkUiScrollBox"));
    if (!TestTrue(TEXT("Expanded production history resolves its nested repeat and scroll"),
        RepeatWidget.IsValid() && Scroll.IsValid())) { return false; }
    const TSharedPtr<SCkUiRepeat> Repeat = StaticCastSharedPtr<SCkUiRepeat>(RepeatWidget);
    TestTrue(TEXT("Empty production history collection is published"), Panel->Get_HistoryCollection().IsValid()
        && Panel->Get_HistoryCollection()->GetRecords().IsEmpty() && Repeat->GetItemCount() == 0);
    TestTrue(TEXT("Expanded authored history shows its empty state"), HasText(Region, TEXT("No events yet.")));
    const FString CaptureDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation/UiDebuggerHistory"));
    IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
    TestTrue(TEXT("Wide mounted history region has nonzero contained geometry"),
        Region->GetCachedGeometry().GetLocalSize().X > 0.0f && Region->GetCachedGeometry().GetLocalSize().Y > 0.0f
        && IsInside(Scroll->GetCachedGeometry(), Region->GetCachedGeometry()));
    TestTrue(TEXT("Wide authored history capture writes"), Capture(
        Slate, Region, FPaths::Combine(CaptureDirectory, TEXT("History-Wide.png"))));
    HistoryPanel->Set_Expanded(false);
    TestFalse(TEXT("Authored history panel can be collapsed before reload"), HistoryPanel->Is_Expanded());

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Debugger plugin resolves history resources"), Plugin.IsValid())) { return false; }
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    HeldCommandSearch->SetText(FText::FromString(TEXT("Retained command filter")));
    Tick(Slate);
    if (!TestEqual(TEXT("Mounted authored command search accepts its draft"),
        HeldCommandSearch->GetText().ToString(), FString(TEXT("Retained command filter")))) { return false; }
    const int64 CommandAcceptedRevision = CommandView->GetRevision();
    const auto CommandReload = CommandView->ReloadFiles(FPaths::Combine(Directory, TEXT("UiDebuggerCommands.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerCommands.ui.css")));
    if (!TestTrue(TEXT("Compatible command reload succeeds"), CommandReload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Compatible command reload retains command view, region, and search draft identity"),
        Panel->Get_CommandView() == CommandView && CommandView->GetRevision() > CommandAcceptedRevision
        && CommandView->GetRegion(TEXT("commands")) == CommandRegion
        && FindTaggedWidget(CommandRegion, TEXT("ui-layer-filter"), TEXT("SSearchBox")) == HeldCommandSearch
        && HeldCommandSearch->GetText().ToString() == TEXT("Retained command filter"));
    const int64 AcceptedCommandRevision = CommandView->GetRevision();
    TestFalse(TEXT("Malformed command reload rejects"), CommandView->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected command reload preserves accepted view, region, and retained search"),
        Panel->Get_CommandView() == CommandView && CommandView->GetRevision() == AcceptedCommandRevision
        && CommandView->GetRegion(TEXT("commands")) == CommandRegion
        && FindTaggedWidget(CommandRegion, TEXT("ui-layer-filter"), TEXT("SSearchBox")) == HeldCommandSearch
        && HeldCommandSearch->GetText().ToString() == TEXT("Retained command filter"));
    const int64 SummaryAcceptedRevision = SummaryView->GetRevision();
    const auto SummaryReload = SummaryView->ReloadFiles(FPaths::Combine(Directory, TEXT("UiDebuggerSummary.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerSummary.ui.css")));
    if (!TestTrue(TEXT("Compatible summary reload succeeds"), SummaryReload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Compatible summary reload retains view and native summary region identity"),
        Panel->Get_SummaryView() == SummaryView && SummaryView->GetRevision() > SummaryAcceptedRevision
        && SummaryView->GetRegion(TEXT("summary")) == SummaryRegion);
    const int64 AcceptedSummaryRevision = SummaryView->GetRevision();
    TestFalse(TEXT("Malformed summary reload rejects"), SummaryView->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected summary reload preserves accepted view, revision, and native summary region"),
        Panel->Get_SummaryView() == SummaryView && SummaryView->GetRevision() == AcceptedSummaryRevision
        && SummaryView->GetRegion(TEXT("summary")) == SummaryRegion);
    const int64 LayerAcceptedRevision = LayerView->GetRevision();
    const auto LayerReload = LayerView->ReloadFiles(FPaths::Combine(Directory, TEXT("UiDebuggerLayers.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerLayers.ui.css")));
    if (!TestTrue(TEXT("Compatible layer reload succeeds"), LayerReload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Compatible layer reload retains view, collection, tree, and native-tree identity"),
        Panel->Get_LayerView() == LayerView && Panel->Get_LayerCollection() == LayerCollection
        && LayerView->GetRevision() > LayerAcceptedRevision
        && LayerView->GetTree(TEXT("ui-layer-tree")) == LayerTree && LayerTree->GetTree() == NativeLayerTree);
    const int64 AcceptedLayerRevision = LayerView->GetRevision();
    TestFalse(TEXT("Malformed layer reload rejects"), LayerView->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected layer reload preserves accepted revision and retained tree identities"),
        LayerView->GetRevision() == AcceptedLayerRevision
        && Panel->Get_LayerCollection() == LayerCollection
        && LayerView->GetTree(TEXT("ui-layer-tree")) == LayerTree && LayerTree->GetTree() == NativeLayerTree);
    const auto Reload = View->ReloadFiles(FPaths::Combine(Directory, TEXT("UiDebuggerHistory.ui.html")), FPaths::Combine(Directory, TEXT("UiDebuggerHistory.ui.css")));
    if (!TestTrue(TEXT("Compatible history reload succeeds"), Reload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Compatible reload retains exact nested history widgets and collapsed authored inspector"),
        FindTaggedWidget(Region, TEXT("history-records"), TEXT("SCkUiRepeat")) == RepeatWidget
        && FindTaggedWidget(Region, TEXT("history-scroll"), TEXT("SCkUiScrollBox")) == Scroll
        && FindHistoryPanel(Region) == HistoryPanel && !HistoryPanel->Is_Expanded());
    const int64 AcceptedRevision = View->GetRevision();
    TestFalse(TEXT("Malformed history reload rejects"), View->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected reload preserves accepted revision, nested widgets and collapsed inspector"),
        View->GetRevision() == AcceptedRevision
        && FindTaggedWidget(Region, TEXT("history-records"), TEXT("SCkUiRepeat")) == RepeatWidget
        && FindTaggedWidget(Region, TEXT("history-scroll"), TEXT("SCkUiScrollBox")) == Scroll
        && FindHistoryPanel(Region) == HistoryPanel && !HistoryPanel->Is_Expanded());

    HistoryPanel->Set_Expanded(true);
    HostWindow->Resize(FVector2D{360.0f, 420.0f});
    Tick(Slate);
    TestTrue(TEXT("Narrow mounted history scroll remains reachable and contained"),
        Region->GetCachedGeometry().GetLocalSize().X > 0.0f && Region->GetCachedGeometry().GetLocalSize().Y > 0.0f
        && IsInside(Scroll->GetCachedGeometry(), Region->GetCachedGeometry()));
    TestTrue(TEXT("Narrow authored history capture writes"), Capture(
        Slate, Region, FPaths::Combine(CaptureDirectory, TEXT("History-Narrow.png"))));

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    const TWeakPtr<SCkUIDebuggerWindow> WeakPanel = Panel;
    const TWeakPtr<FCkUiView> WeakCommandView = CommandView;
    const TWeakPtr<FCkUiView> WeakSummaryView = SummaryView;
    CommandView.Reset();
    SummaryView.Reset();
    Panel.Reset();
    Tick(Slate);
    TestFalse(TEXT("UI debugger owner releases command and summary views while authored actions are retained"),
        WeakPanel.IsValid() || WeakCommandView.IsValid() || WeakSummaryView.IsValid());
    for (const TSharedPtr<SButton>& HeldCommandAction : HeldCommandActions)
    { HeldCommandAction->SimulateClick(); }
    Region->SlatePrepass();
    TestTrue(TEXT("Held history region and authored command callbacks remain inert after owner release"),
        !WeakPanel.IsValid() && !WeakCommandView.IsValid() && !WeakSummaryView.IsValid()
        && Region->GetCachedGeometry().GetLocalSize().X >= 0.0f);
    return true;
}

#endif
