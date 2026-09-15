#include "CkSmDebugger/CkSmDebugger_Module.h"
#include "CkSmDebugger/Window/SCkSmDebuggerWindow.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FCkSmDebuggerAuthoredShellTestAccess
{
    static auto GetShell(const SCkSmDebuggerWindow& InWindow) -> TSharedPtr<FCkUiView> { return InWindow._AuthoredShell; }
    static auto IsFallback(const SCkSmDebuggerWindow& InWindow) -> bool { return InWindow._UsingNativeShellFallback; }
    static auto HasSixPorts(const SCkSmDebuggerWindow& InWindow) -> bool
    {
        return InWindow._GraphMount.IsValid() && InWindow._TimelineMount.IsValid() && InWindow._HistoryMount.IsValid()
            && InWindow._DetailMount.IsValid() && InWindow._PreviewMount.IsValid() && InWindow._PreviewPickerMount.IsValid();
    }
    static auto TogglePreview(SCkSmDebuggerWindow& InWindow) -> void { InWindow._IsPreviewOpen = NOT InWindow._IsPreviewOpen; }
    static auto IsPreviewEffectivelyVisible(const SCkSmDebuggerWindow& InWindow) -> bool
    {
        TSharedPtr<SWidget> Widget = InWindow._PreviewMount;
        while (Widget.IsValid())
        {
            const EVisibility Visibility = Widget->GetVisibility();
            if (Visibility == EVisibility::Collapsed || Visibility == EVisibility::Hidden) { return false; }
            Widget = Widget->GetParentWidget();
        }
        return true;
    }
    static auto Poll(SCkSmDebuggerWindow& InWindow) -> void { InWindow.PollAuthoredShell(0.0, 0.0f); }
    static auto PreExit(FCkSmDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
    static auto Tab(const FCkSmDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmDebuggerAuthoredShell,
    "Ck.UiAuthoring.SmDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkSmDebuggerAuthoredShell::RunTest(const FString&) -> bool
{
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("SM authored shell requires Slate.")); return false; }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("CkDebugger plugin is installed"), Plugin.IsValid())) { return false; }
    FString Markup; FString Stylesheet;
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    if (!TestTrue(TEXT("installed SM shell resources load"),
        FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("SmDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Directory, TEXT("SmDebuggerShell.ui.css"))))) { return false; }

    const TSharedPtr<SCkSmDebuggerWindow> Window = SNew(SCkSmDebuggerWindow);
    const TSharedPtr<FCkUiView> Shell = FCkSmDebuggerAuthoredShellTestAccess::GetShell(*Window);
    if (!TestTrue(TEXT("reachable production window admits the authored shell"), Shell.IsValid())) { return false; }
    TestTrue(TEXT("six persistent native ports exist"), FCkSmDebuggerAuthoredShellTestAccess::HasSixPorts(*Window));
    Shell->GetRegion(TEXT("main"))->SlatePrepass();
    TestFalse(TEXT("authored preview pane starts collapsed"),
        FCkSmDebuggerAuthoredShellTestAccess::IsPreviewEffectivelyVisible(*Window));
    FCkSmDebuggerAuthoredShellTestAccess::TogglePreview(*Window);
    Shell->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("opening preview exposes the retained authored preview pane"),
        FCkSmDebuggerAuthoredShellTestAccess::IsPreviewEffectivelyVisible(*Window));
    FCkSmDebuggerAuthoredShellTestAccess::TogglePreview(*Window);
    Shell->GetRegion(TEXT("main"))->SlatePrepass();
    TestFalse(TEXT("closing preview collapses the retained authored preview pane again"),
        FCkSmDebuggerAuthoredShellTestAccess::IsPreviewEffectivelyVisible(*Window));
    const TSharedRef<SWidget> Main = Shell->GetRegion(TEXT("main"));
    const int64 Revision = Shell->GetRevision();
    TestTrue(TEXT("compatible reload commits"), Window->TryReload_AuthoredShell(Markup, Stylesheet).Succeeded);
    TestTrue(TEXT("compatible reload preserves the committed main region"), Shell->GetRevision() > Revision && Shell->GetRegion(TEXT("main")) == Main);
    TestFalse(TEXT("malformed markup rejects without replacing the tree"), Window->TryReload_AuthoredShell(TEXT("<ui version=\"1\"><region name=\"main\">"), Stylesheet).Succeeded);
    TestTrue(TEXT("malformed reload preserves the committed tree"), Shell->GetRegion(TEXT("main")) == Main);
    TestFalse(TEXT("missing native port rejects atomically"), Window->TryReload_AuthoredShell(Markup.Replace(TEXT("bind=\"preview-picker\""), TEXT("bind=\"preview-picker-missing\"")), Stylesheet).Succeeded);
    TestTrue(TEXT("missing port preserves the committed tree"), Shell->GetRegion(TEXT("main")) == Main);
    TestFalse(TEXT("missing field rejects atomically"), Window->TryReload_AuthoredShell(Markup.Replace(TEXT("<text id=\"sm-history-heading\""), TEXT("<text id=\"bad-field\" bind=\"missing-text\"/><text id=\"sm-history-heading\"")), Stylesheet).Succeeded);
    TestTrue(TEXT("missing field preserves the committed tree"), Shell->GetRegion(TEXT("main")) == Main);
    TestFalse(TEXT("unknown action rejects atomically"), Window->TryReload_AuthoredShell(Markup.Replace(TEXT("<native id=\"sm-timeline\""), TEXT("<button id=\"bad\" action=\"missing-action\">bad</button><native id=\"sm-timeline\"")), Stylesheet).Succeeded);
    TestTrue(TEXT("unknown action preserves the committed tree"), Shell->GetRegion(TEXT("main")) == Main);
    const TSharedPtr<SCkSmDebuggerWindow> MissingResourceWindow = SNew(SCkSmDebuggerWindow).TestResourceDirectory(TEXT("C:/missing-sm-debugger-shell"));
    TestTrue(TEXT("missing startup resource retains the complete native fallback"),
        FCkSmDebuggerAuthoredShellTestAccess::IsFallback(*MissingResourceWindow) && FCkSmDebuggerAuthoredShellTestAccess::HasSixPorts(*MissingResourceWindow));
    Window->Release_Presentation();
    Window->Release_Presentation();
    TestTrue(TEXT("released shell fails closed"), FCkSmDebuggerAuthoredShellTestAccess::GetShell(*Window) == nullptr);

    FCkSmDebuggerModule& Module = FModuleManager::LoadModuleChecked<FCkSmDebuggerModule>(TEXT("CkSmDebugger"));
    Module.CloseDebugger(); Module.OpenDebugger();
    const TSharedPtr<SDockTab> OpenTab = FCkSmDebuggerAuthoredShellTestAccess::Tab(Module);
    TestTrue(TEXT("real production tab opens"), OpenTab.IsValid());
    Module.CloseDebugger();
    TestTrue(TEXT("ordinary close releases the retained tab content"), !OpenTab.IsValid() || OpenTab->GetContent() == SNullWidget::NullWidget);
    Module.OpenDebugger();
    const TSharedPtr<SDockTab> PreExitTab = FCkSmDebuggerAuthoredShellTestAccess::Tab(Module);
    FCkSmDebuggerAuthoredShellTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit releases the live tab before module shutdown"), !PreExitTab.IsValid() || PreExitTab->GetContent() == SNullWidget::NullWidget);
    return true;
}

#endif
