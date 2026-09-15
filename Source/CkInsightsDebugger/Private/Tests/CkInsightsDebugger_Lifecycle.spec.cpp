#include "../../CkInsightsDebugger_Module.h"
#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkInsightsLifecycleTestAccess
{
    static auto Window(const FCkInsightsDebuggerModule& InModule) -> TSharedPtr<SCkInsightsAnalyzerTab>
    { return InModule._DebuggerWindow; }
    static auto Tab(const FCkInsightsDebuggerModule& InModule) -> TSharedPtr<SDockTab>
    { return InModule._DebuggerTab; }
    static auto PreExit(FCkInsightsDebuggerModule& InModule) -> void
    { InModule.HandleEnginePreExit(); }
    static auto DepthControl(const SCkInsightsAnalyzerTab& InWindow) -> TSharedPtr<STextComboBox>
    { return InWindow._DepthCombo; }
    static auto IsReleased(const SCkInsightsAnalyzerTab& InWindow) -> bool
    { return InWindow._PresentationReleased; }
    static auto HasOwnedAsyncUi(const SCkInsightsAnalyzerTab& InWindow) -> bool
    {
        return InWindow._CaptureUiTickerHandle.IsValid()
            || InWindow._AutoOpenTickerHandle.IsValid()
            || InWindow._LoadingTickerHandle.IsValid();
    }
    static auto HasStableAuthoredPorts(const SCkInsightsAnalyzerTab& InWindow) -> bool
    {
        return InWindow._AuthoredShell.IsValid() && !InWindow._UsingNativeShellFallback
            && InWindow._SummaryMount.IsValid() && InWindow._FrameBarChartMount.IsValid()
            && InWindow._ResultsMount.IsValid() && InWindow._RawReportMount.IsValid();
    }
    static auto Ports(const SCkInsightsAnalyzerTab& InWindow) -> TArray<TSharedPtr<SWidget>>
    { return {InWindow._SummaryMount, InWindow._FrameBarChartMount, InWindow._ResultsMount, InWindow._RawReportMount}; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInsightsDebuggerLifecycle,
    "Ck.InsightsDebugger.Lifecycle.ReleasePresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsDebuggerLifecycle::RunTest(const FString&)
{
    FCkInsightsDebuggerModule& Module =
        FModuleManager::LoadModuleChecked<FCkInsightsDebuggerModule>(TEXT("CkInsightsDebugger"));
    Module.CloseDebugger();
    Module.OpenDebugger();

    const TSharedPtr<SCkInsightsAnalyzerTab> Window = FCkInsightsLifecycleTestAccess::Window(Module);
    const TSharedPtr<SDockTab> Tab = FCkInsightsLifecycleTestAccess::Tab(Module);
    const TSharedPtr<STextComboBox> HeldDepth = Window.IsValid()
        ? FCkInsightsLifecycleTestAccess::DepthControl(*Window) : nullptr;
    if (NOT TestTrue(TEXT("real module creates Insights window, tab, and retained control"),
        Window.IsValid() && Tab.IsValid() && HeldDepth.IsValid()))
    { return false; }
    const FCkUiLoadResult& InitialLoad = Window->Get_AuthoredShell()->GetLastResult();
    if (NOT InitialLoad.Succeeded)
    { AddInfo(TEXT("Insights initial authored-load errors: ") + FString::Join(InitialLoad.Errors, TEXT(" | "))); }
    TestTrue(TEXT("installed Insights shell atomically mounts all retained native ports"),
        FCkInsightsLifecycleTestAccess::HasStableAuthoredPorts(*Window));
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    if (NOT TestTrue(TEXT("installed Insights shell resources load"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/InsightsDebugger.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/InsightsDebugger.ui.css")))))
    { return false; }
    const TSharedRef<SWidget> Main = Window->Get_AuthoredShell()->GetRegion(TEXT("main"));
    const TArray<TSharedPtr<SWidget>> Ports = FCkInsightsLifecycleTestAccess::Ports(*Window);
    const auto CompatibleReload = Window->Get_AuthoredShell()->TryReload(
        Markup, Stylesheet, TEXT("Insights compatible reload"));
    if (NOT CompatibleReload.Succeeded)
    { AddInfo(TEXT("Insights compatible-reload errors: ") + FString::Join(CompatibleReload.Errors, TEXT(" | "))); }
    TestTrue(TEXT("compatible Insights reload succeeds"), CompatibleReload.Succeeded);
    TestTrue(TEXT("compatible Insights reload preserves the committed shell"),
        Window->Get_AuthoredShell()->GetRegion(TEXT("main")) == Main);
    TestTrue(TEXT("compatible Insights reload preserves every native port"),
        FCkInsightsLifecycleTestAccess::Ports(*Window) == Ports);
    TestFalse(TEXT("missing Insights port rejects without replacing the committed shell"),
        Window->Get_AuthoredShell()->TryReload(Markup.Replace(TEXT("bind=\"results\""), TEXT("bind=\"missing-results\"")),
            Stylesheet, TEXT("Insights missing port")).Succeeded);
    TestTrue(TEXT("rejected Insights reload retains every stable port"),
        Window->Get_AuthoredShell()->GetRegion(TEXT("main")) == Main
        && FCkInsightsLifecycleTestAccess::Ports(*Window) == Ports);

    FSlateApplication& Slate = FSlateApplication::Get();
    Slate.SetKeyboardFocus(HeldDepth, EFocusCause::SetDirectly);
    const FCkInsightsCaptureController* CaptureOwner = &Module.Get_CaptureController();

    Module.CloseDebugger();
    TestTrue(TEXT("ordinary close releases UI without destroying module capture ownership"),
        FCkInsightsLifecycleTestAccess::IsReleased(*Window)
        && !FCkInsightsLifecycleTestAccess::HasOwnedAsyncUi(*Window)
        && Tab->GetContent() == SNullWidget::NullWidget
        && Slate.GetUserFocusedWidget(0) != HeldDepth
        && CaptureOwner == &Module.Get_CaptureController());
    Window->Release_Presentation();
    TestTrue(TEXT("retained Insights release is idempotent"),
        FCkInsightsLifecycleTestAccess::IsReleased(*Window));

    Module.OpenDebugger();
    const TSharedPtr<SCkInsightsAnalyzerTab> ReopenedWindow = FCkInsightsLifecycleTestAccess::Window(Module);
    const TSharedPtr<SDockTab> ReopenedTab = FCkInsightsLifecycleTestAccess::Tab(Module);
    FCkInsightsLifecycleTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit releases and detaches the reopened Insights UI"),
        ReopenedWindow.IsValid() && ReopenedWindow != Window
        && FCkInsightsLifecycleTestAccess::IsReleased(*ReopenedWindow)
        && ReopenedTab.IsValid() && ReopenedTab->GetContent() == SNullWidget::NullWidget
        && !FCkInsightsLifecycleTestAccess::Window(Module).IsValid());
    return true;
}

#endif
