#include "../../CkSaveDebugger_Module.h"
#include "CkSaveDebugger/Window/SCkSaveDebuggerWindow.h"
#include "CkSaveDebugger/Visualizer/CkSaveDebugger_Visualizer.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkSaveDebuggerAuthoredShellTestAccess
{
    static auto PreExit(FCkSaveDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
    static auto GetTab(const FCkSaveDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
    static auto GetWindow(const FCkSaveDebuggerModule& InModule) -> TSharedPtr<SCkSaveDebuggerWindow> { return InModule._DebuggerWindow; }
    static auto HasSixPorts(const SCkSaveDebuggerWindow& InWindow) -> bool
    { return InWindow._SummaryMount.IsValid() && InWindow._EntityNavigationMount.IsValid() && InWindow._EntityDetailMount.IsValid()
        && InWindow._PayloadMount.IsValid() && InWindow._RightColumnMount.IsValid() && InWindow._DiagnosticsMount.IsValid(); }
    static auto NativePorts(const SCkSaveDebuggerWindow& InWindow) -> TArray<TSharedPtr<SWidget>>
    { return {InWindow._SummaryMount, InWindow._EntityNavigationMount, InWindow._EntityDetailMount,
        InWindow._PayloadMount, InWindow._RightColumnMount, InWindow._DiagnosticsMount}; }
    static auto FallbackHosts(const SCkSaveDebuggerWindow& InWindow) -> TArray<TSharedPtr<SBox>>
    { return {InWindow._FallbackSummaryHost, InWindow._FallbackEntityNavigationHost,
        InWindow._FallbackEntityDetailHost, InWindow._FallbackPayloadHost, InWindow._FallbackRightColumnHost,
        InWindow._FallbackDiagnosticsHost}; }
    static auto PortsHaveFallbackOwners(const SCkSaveDebuggerWindow& InWindow) -> bool
    {
        const TArray<TSharedPtr<SWidget>> Ports = NativePorts(InWindow);
        const TArray<TSharedPtr<SBox>> Hosts = FallbackHosts(InWindow);
        if (Ports.Num() != Hosts.Num()) { return false; }
        for (int32 Index = 0; Index < Ports.Num(); ++Index)
        {
            if (!Ports[Index].IsValid() || !Hosts[Index].IsValid()
                || Ports[Index]->GetParentWidget().Get() != Hosts[Index].Get()
                || Hosts[Index]->GetChildren()->Num() != 1
                || &Hosts[Index]->GetChildren()->GetChildAt(0).Get() != Ports[Index].Get())
            { return false; }
        }
        return true;
    }
    static auto PortsHaveAuthoredOwners(const SCkSaveDebuggerWindow& InWindow) -> bool
    {
        const TArray<TSharedPtr<SWidget>> Ports = NativePorts(InWindow);
        const TArray<TSharedPtr<SBox>> Hosts = FallbackHosts(InWindow);
        if (Ports.Num() != Hosts.Num()) { return false; }
        for (int32 Index = 0; Index < Ports.Num(); ++Index)
        {
            if (!Ports[Index].IsValid() || !Hosts[Index].IsValid() || !Ports[Index]->GetParentWidget().IsValid()
                || Ports[Index]->GetParentWidget().Get() == Hosts[Index].Get()
                || (Hosts[Index]->GetChildren()->Num() == 1
                    && &Hosts[Index]->GetChildren()->GetChildAt(0).Get() == Ports[Index].Get()))
            { return false; }
        }
        return true;
    }
    static auto Poll(SCkSaveDebuggerWindow& InWindow) -> void { InWindow.DoPoll_AuthoredBody(2.0, 0.0f); }
    static auto IsMountedAuthored(const SCkSaveDebuggerWindow& InWindow) -> bool
    { return InWindow._AuthoredBodyView.IsValid() && InWindow._BodyHost.IsValid()
        && InWindow._BodyHost->GetChildren()->Num() == 1
        && InWindow._BodyHost->GetChildren()->GetChildAt(0) == InWindow._AuthoredBodyView->GetRegion(TEXT("main")); }
    static auto IsPresentationReleased(const SCkSaveDebuggerWindow& InWindow) -> bool { return InWindow._PresentationReleased; }
    static auto OpenOwnedTestMenu(SCkSaveDebuggerWindow& InWindow) -> bool
    {
        InWindow.DoOpenOwnedContextMenu(InWindow._BodyHost,
            SNew(STextBlock).Text(FText::FromString(TEXT("Save popup lifecycle probe"))));
        return InWindow._ContextMenu.IsValid();
    }
    static auto HasOwnedMenu(const SCkSaveDebuggerWindow& InWindow) -> bool { return InWindow._ContextMenu.IsValid(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSaveDebuggerAuthoredShell,
    "Ck.UiAuthoring.SaveDebugger.AuthoredShell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebuggerAuthoredShell::RunTest(const FString&)
{
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Save authored shell requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT TestTrue(TEXT("CkDebugger plugin is installed"), Plugin.IsValid())) { return false; }
    const FString Installed = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    FString Markup, Stylesheet;
    if (NOT TestTrue(TEXT("installed Save shell resources load"),
        FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Installed, TEXT("SaveDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Installed, TEXT("SaveDebuggerShell.ui.css"))))) { return false; }

    const TSharedPtr<SCkSaveDebuggerWindow> Window = SNew(SCkSaveDebuggerWindow);
    const TSharedPtr<SWindow> Host = SNew(SWindow).ClientSize(FVector2D(1000, 700)).CreateTitleBar(false).HasCloseButton(false)[Window.ToSharedRef()];
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };
    Slate.AddWindow(Host.ToSharedRef(), true); Slate.Tick();
    const TSharedPtr<FCkUiView> Body = Window->Get_AuthoredBody();
    const bool bInstalledBodyAdmitted = Body.IsValid() && !Window->IsUsingNativeBodyFallback();
    if (NOT TestTrue(TEXT("installed production window admits the authored body"), bInstalledBodyAdmitted))
    {
        const FString Errors = Body.IsValid() ? FString::Join(Body->GetLastResult().Errors, TEXT("\n")) : TEXT("The authored view was not created.");
        AddError(FString::Printf(TEXT("Save authored-body admission errors:\n%s"), *Errors));
        return false;
    }
    const TArray<TSharedPtr<SWidget>> Ports = FCkSaveDebuggerAuthoredShellTestAccess::NativePorts(*Window);
    TestTrue(TEXT("six exact stable native ports mount"), FCkSaveDebuggerAuthoredShellTestAccess::HasSixPorts(*Window)
        && !Ports.ContainsByPredicate([](const TSharedPtr<SWidget>& Port) { return !Port.IsValid(); }));
    TestTrue(TEXT("authored shell is the sole parent of every retained port"),
        FCkSaveDebuggerAuthoredShellTestAccess::PortsHaveAuthoredOwners(*Window));
    TestTrue(TEXT("authored splitters mount"), Body->GetSplitter(TEXT("save-main")).IsValid() && Body->GetSplitter(TEXT("save-columns")).IsValid());
    const TSharedRef<SWidget> Main = Body->GetRegion(TEXT("main")); const int64 Revision = Body->GetRevision();
    TestTrue(TEXT("compatible reload succeeds"), Body->TryReload(Markup, Stylesheet, TEXT("Save compatible reload")).Succeeded);
    TestTrue(TEXT("compatible reload preserves main and six port identities"), Body->GetRevision() > Revision && Body->GetRegion(TEXT("main")) == Main
        && FCkSaveDebuggerAuthoredShellTestAccess::NativePorts(*Window) == Ports);
    TestFalse(TEXT("missing required port rejects atomically"), Body->TryReload(Markup.Replace(TEXT("bind=\"right-column\""), TEXT("bind=\"right-missing\"")), Stylesheet, TEXT("Save missing port")).Succeeded);
    TestTrue(TEXT("rejection preserves committed topology and sole authored port ownership"), Body->GetRegion(TEXT("main")) == Main
        && FCkSaveDebuggerAuthoredShellTestAccess::NativePorts(*Window) == Ports
        && FCkSaveDebuggerAuthoredShellTestAccess::PortsHaveAuthoredOwners(*Window));

    const FString RecoveryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SaveAuthoredRecovery"));
    IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RecoveryDirectory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true); };
    const FString RecoveryMarkup = FPaths::Combine(RecoveryDirectory, TEXT("SaveDebuggerShell.ui.html"));
    const FString RecoveryStylesheet = FPaths::Combine(RecoveryDirectory, TEXT("SaveDebuggerShell.ui.css"));
    if (NOT TestTrue(TEXT("malformed Save recovery fixture writes"), FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\">"), *RecoveryMarkup)
        && FFileHelper::SaveStringToFile(Stylesheet, *RecoveryStylesheet))) { return false; }
    const TSharedPtr<SCkSaveDebuggerWindow> RecoveryWindow = SNew(SCkSaveDebuggerWindow).TestResourceDirectory(RecoveryDirectory);
    const TSharedPtr<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D(1000, 700)).CreateTitleBar(false).HasCloseButton(false)[RecoveryWindow.ToSharedRef()];
    ON_SCOPE_EXIT { if (RecoveryHost.IsValid()) { Slate.DestroyWindowImmediately(RecoveryHost.ToSharedRef()); } };
    Slate.AddWindow(RecoveryHost.ToSharedRef(), true); Slate.Tick();
    const TArray<TSharedPtr<SWidget>> FallbackPorts = FCkSaveDebuggerAuthoredShellTestAccess::NativePorts(*RecoveryWindow);
    TestTrue(TEXT("malformed startup keeps complete native fallback"), RecoveryWindow->IsUsingNativeBodyFallback()
        && !FallbackPorts.ContainsByPredicate([](const TSharedPtr<SWidget>& Port) { return !Port.IsValid(); })
        && FCkSaveDebuggerAuthoredShellTestAccess::PortsHaveFallbackOwners(*RecoveryWindow));
    if (NOT TestTrue(TEXT("same paths are repaired with the installed document"), FFileHelper::SaveStringToFile(Markup, *RecoveryMarkup)
        && FFileHelper::SaveStringToFile(Stylesheet, *RecoveryStylesheet))) { return false; }
    FCkSaveDebuggerAuthoredShellTestAccess::Poll(*RecoveryWindow);
    TestTrue(TEXT("valid same-path repair admits exactly the retained authored shell and preserves all ports"),
        !RecoveryWindow->IsUsingNativeBodyFallback() && FCkSaveDebuggerAuthoredShellTestAccess::IsMountedAuthored(*RecoveryWindow)
        && RecoveryWindow->Get_AuthoredBody().IsValid() && FCkSaveDebuggerAuthoredShellTestAccess::NativePorts(*RecoveryWindow) == FallbackPorts
        && FCkSaveDebuggerAuthoredShellTestAccess::PortsHaveAuthoredOwners(*RecoveryWindow));

    FCkSaveDebuggerModule& Module = FModuleManager::LoadModuleChecked<FCkSaveDebuggerModule>(TEXT("CkSaveDebugger"));
    Module.CloseDebugger(); Module.OpenDebugger();
    Slate.PumpMessages(); Slate.Tick();
    const TSharedPtr<SCkSaveDebuggerWindow> ModuleWindow = FCkSaveDebuggerAuthoredShellTestAccess::GetWindow(Module);
    const TSharedPtr<SDockTab> Tab = FCkSaveDebuggerAuthoredShellTestAccess::GetTab(Module);
    if (NOT TestTrue(TEXT("real loaded module opens Save tab"), ModuleWindow.IsValid() && Tab.IsValid())) { return false; }
    const TSharedPtr<FCkUiView> HeldView = ModuleWindow->Get_AuthoredBody();
    int32 VisualizerCallbackCount = 0;
    ck::save_debugger_viz::Register_OnRowClicked([&VisualizerCallbackCount](const uint32) { ++VisualizerCallbackCount; });
    ck::save_debugger_viz::Notify_RowClicked(1);
    TestEqual(TEXT("held visualizer callback is live before close"), VisualizerCallbackCount, 1);
    TestTrue(TEXT("mounted production Save window owns its popup menu"),
        FCkSaveDebuggerAuthoredShellTestAccess::OpenOwnedTestMenu(*ModuleWindow));
    Module.CloseDebugger();
    TestTrue(TEXT("ordinary close releases held window routes before detaching its tab"), !FCkSaveDebuggerAuthoredShellTestAccess::GetWindow(Module).IsValid()
        && FCkSaveDebuggerAuthoredShellTestAccess::IsPresentationReleased(*ModuleWindow)
        && !FCkSaveDebuggerAuthoredShellTestAccess::HasOwnedMenu(*ModuleWindow)
        && HeldView.IsValid()
        && Tab->GetContent() == SNullWidget::NullWidget
        && !ModuleWindow->TryReload_EntityNavigationLayout(Markup, Stylesheet).Succeeded);
    TestFalse(TEXT("released Save window cannot reopen an owned popup"),
        FCkSaveDebuggerAuthoredShellTestAccess::OpenOwnedTestMenu(*ModuleWindow));
    ck::save_debugger_viz::Notify_RowClicked(2);
    TestTrue(TEXT("held visualizer callback and published rows are inert after close"), VisualizerCallbackCount == 1
        && !ck::save_debugger_viz::Get_Rows().IsValid());
    Module.OpenDebugger();
    const TSharedPtr<SCkSaveDebuggerWindow> ReopenedWindow = FCkSaveDebuggerAuthoredShellTestAccess::GetWindow(Module);
    const TSharedPtr<SDockTab> Reopened = FCkSaveDebuggerAuthoredShellTestAccess::GetTab(Module);
    TestTrue(TEXT("reopen creates a fresh live presentation"), ReopenedWindow.IsValid()
        && !FCkSaveDebuggerAuthoredShellTestAccess::IsPresentationReleased(*ReopenedWindow));
    FCkSaveDebuggerAuthoredShellTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit releases and detaches reopened production tab"), !FCkSaveDebuggerAuthoredShellTestAccess::GetWindow(Module).IsValid()
        && Reopened.IsValid() && Reopened->GetContent() == SNullWidget::NullWidget
        && FCkSaveDebuggerAuthoredShellTestAccess::IsPresentationReleased(*ReopenedWindow));
    return true;
}
#endif
