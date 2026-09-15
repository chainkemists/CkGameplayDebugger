#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"
#include "../../CkJoltDebugger_Module.h"

#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"
#include "CkJoltDebugger/Window/SCkJoltDebugger_OutlinerPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkJoltDebuggerAuthoredShellTestAccess
{
    static auto Poll(SCkJoltDebuggerWindow& InWindow) -> void
    {
        // Slate's live tick has already advanced the production poll clock past this test's synthetic time.
        // Exercise the same-path recovery operation itself rather than accidentally testing that throttle.
        InWindow._NextAuthoredShellPollSeconds = 0.0;
        InWindow.Poll_AuthoredShell(2.0);
    }
    static auto Ports(const SCkJoltDebuggerWindow& InWindow) -> TArray<TSharedPtr<SWidget>>
    {
        return {
            StaticCastSharedPtr<SWidget>(InWindow._PickerControls),
            StaticCastSharedPtr<SWidget>(InWindow._OutlinerPanel),
            StaticCastSharedPtr<SWidget>(InWindow._Viewport),
            StaticCastSharedPtr<SWidget>(InWindow._DetailPort),
        };
    }
    static auto HasFallbackOwners(const SCkJoltDebuggerWindow& InWindow) -> bool
    {
        const TArray<TSharedPtr<SWidget>> Controls = Ports(InWindow);
        const TArray<TSharedPtr<SBox>> Hosts{InWindow._FallbackPickerHost, InWindow._FallbackOutlinerHost,
            InWindow._FallbackViewportHost, InWindow._FallbackDetailHost};
        for (int32 Index = 0; Index < Controls.Num(); ++Index)
        {
            if (NOT Controls[Index].IsValid() || NOT Hosts.IsValidIndex(Index) || NOT Hosts[Index].IsValid()
                || Controls[Index]->GetParentWidget().Get() != Hosts[Index].Get()) { return false; }
        }
        return true;
    }
    static auto Picker(const SCkJoltDebuggerWindow& InWindow) -> TSharedPtr<FCkDebug_ViewportPicker> { return InWindow._ViewportPicker; }
    static auto RefreshDispatchCount(const SCkJoltDebuggerWindow& InWindow) -> int32 { return InWindow._AuthoredRefreshDispatchCount; }
};

struct FCkJoltDebuggerModuleTestAccess
{
    static auto Window(FCkJoltDebuggerModule& InModule) -> TSharedPtr<SCkJoltDebuggerWindow> { return InModule._DebuggerWindow; }
    static auto Tab(FCkJoltDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
    static auto PreExit(FCkJoltDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
};

namespace ck_jolt_debugger_authored_shell_tests
{
    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }
    auto FindButtonByTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTag() == InTag
            && (InRoot->GetTypeAsString() == TEXT("SButton") || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton")))
        { return StaticCastSharedRef<SButton>(InRoot); }

        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonByTag(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
                Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto ReadInstalledShell(FString& OutMarkup, FString& OutCss) -> bool
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (NOT Plugin.IsValid()) { return false; }
        const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        return FFileHelper::LoadFileToString(OutMarkup, *FPaths::Combine(Directory, TEXT("JoltDebugger.ui.html")))
            && FFileHelper::LoadFileToString(OutCss, *FPaths::Combine(Directory, TEXT("JoltDebugger.ui.css")));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkJoltDebuggerAuthoredShellPortsAndReload,
    "Ck.JoltDebugger.Authored.PortsAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerAuthoredShellPortsAndReload::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_authored_shell_tests;
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Jolt authored-shell test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedRef<SCkJoltDebuggerWindow> Window = SNew(SCkJoltDebuggerWindow);
    const TSharedRef<SWindow> Host = SNew(SWindow).ClientSize(FVector2D{1280.0f, 900.0f})[Window];
    ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(Host); };
    Slate.AddWindow(Host, true); Tick(Slate);
    const TSharedPtr<FCkUiView> View = Window->Get_AuthoredShellView();
    if (NOT TestTrue(TEXT("production Jolt shell admits one retained authored root"), View.IsValid() && View->GetLastResult().Succeeded
        && NOT Window->IsUsingNativeShellFallback())) { AddError(Window->Get_AuthoredShellLoadFailure()); return false; }
    const TArray<TSharedPtr<SWidget>> Ports = FCkJoltDebuggerAuthoredShellTestAccess::Ports(*Window);
    TestTrue(TEXT("Jolt authored root exposes the stable four-port topology"), View->GetSplitter(TEXT("jolt-topology")).IsValid());
    FString Markup; FString Css;
    if (NOT TestTrue(TEXT("installed Jolt shell resources are readable"), ReadInstalledShell(Markup, Css))) { return false; }
    const int64 AcceptedRevision = View->GetRevision();
    TestTrue(TEXT("compatible Jolt reload preserves port identities"), View->TryReload(Markup, Css, TEXT("Jolt compatible candidate")).Succeeded
        && View->GetRevision() > AcceptedRevision && Ports == FCkJoltDebuggerAuthoredShellTestAccess::Ports(*Window));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const int64 RejectedRevision = View->GetRevision();
    const bool MissingPort = NOT View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><native bind=\"missing-port\"/></region></ui>"), TEXT(""), TEXT("Jolt missing port")).Succeeded;
    const bool MissingAction = NOT View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><button action=\"missing-action\">x</button></region></ui>"), TEXT(""), TEXT("Jolt missing action")).Succeeded;
    const bool MissingField = NOT View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><text bind=\"missing-field\"/></region></ui>"), TEXT(""), TEXT("Jolt missing field")).Succeeded;
    TestTrue(TEXT("missing port/action/field reloads retain the committed Jolt mount"), MissingPort && MissingAction && MissingField
        && View->GetRevision() == RejectedRevision && View->GetRegion(TEXT("main")) == Main
        && Ports == FCkJoltDebuggerAuthoredShellTestAccess::Ports(*Window));

    const FString RecoveryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/JoltDebuggerAuthoredRecovery"));
    IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RecoveryDirectory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true); };
    TestTrue(TEXT("malformed Jolt recovery fixture writes"), FFileHelper::SaveStringToFile(
        TEXT("<ui version=\"1\"><region name=\"main\">"), *FPaths::Combine(RecoveryDirectory, TEXT("JoltDebugger.ui.html"))));
    TestTrue(TEXT("Jolt recovery stylesheet writes"), FFileHelper::SaveStringToFile(Css, *FPaths::Combine(RecoveryDirectory, TEXT("JoltDebugger.ui.css"))));
    const TSharedRef<SCkJoltDebuggerWindow> RecoveryWindow = SNew(SCkJoltDebuggerWindow).TestResourceDirectory(RecoveryDirectory);
    const TSharedRef<SWindow> RecoveryHost = SNew(SWindow).ClientSize(FVector2D{1280.0f, 900.0f})[RecoveryWindow];
    ON_SCOPE_EXIT { Slate.DestroyWindowImmediately(RecoveryHost); };
    Slate.AddWindow(RecoveryHost, true); Tick(Slate);
    const TArray<TSharedPtr<SWidget>> RecoveryPorts = FCkJoltDebuggerAuthoredShellTestAccess::Ports(*RecoveryWindow);
    TestTrue(TEXT("malformed startup retains the complete Jolt native fallback"), RecoveryWindow->IsUsingNativeShellFallback()
        && FCkJoltDebuggerAuthoredShellTestAccess::HasFallbackOwners(*RecoveryWindow));
    if (TestTrue(TEXT("same Jolt resource paths repair"), FFileHelper::SaveStringToFile(Markup,
        *FPaths::Combine(RecoveryDirectory, TEXT("JoltDebugger.ui.html")))))
    {
        FCkJoltDebuggerAuthoredShellTestAccess::Poll(*RecoveryWindow);
        TestTrue(TEXT("same-path repair adopts authored Jolt topology without replacing ports"),
            NOT RecoveryWindow->IsUsingNativeShellFallback() && RecoveryWindow->Get_AuthoredShellView().IsValid()
                && RecoveryWindow->Get_AuthoredShellView()->GetLastResult().Succeeded
                && RecoveryPorts == FCkJoltDebuggerAuthoredShellTestAccess::Ports(*RecoveryWindow));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkJoltDebuggerAuthoredShellLifecycle,
    "Ck.JoltDebugger.Authored.CloseReopenPreExit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerAuthoredShellLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_authored_shell_tests;
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Jolt authored lifecycle test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FCkJoltDebuggerModule& Module = FCkJoltDebuggerModule::Get();
    Module.CloseDebugger(); Module.OpenDebugger(); Tick(Slate);
    const TSharedPtr<SCkJoltDebuggerWindow> First = FCkJoltDebuggerModuleTestAccess::Window(Module);
    const TSharedPtr<FCkUiView> HeldView = First.IsValid() ? First->Get_AuthoredShellView() : nullptr;
    const TSharedPtr<FCkDebug_ViewportPicker> HeldPicker = First.IsValid() ? FCkJoltDebuggerAuthoredShellTestAccess::Picker(*First) : nullptr;
    if (NOT TestTrue(TEXT("Jolt module opens a live authored shell"), Module.IsDebuggerOpen() && HeldView.IsValid() && HeldPicker.IsValid())) { return false; }
    const TSharedPtr<SButton> FirstRefresh = FindButtonByTag(HeldView->GetRegion(TEXT("main")), TEXT("jolt-refresh"));
    if (NOT TestTrue(TEXT("Jolt authored shell exposes its physical Refresh action"), FirstRefresh.IsValid())) { return false; }
    FirstRefresh->SimulateClick();
    TestEqual(TEXT("live Jolt authored Refresh dispatches once"), FCkJoltDebuggerAuthoredShellTestAccess::RefreshDispatchCount(*First), 1);
    Module.CloseDebugger(); Tick(Slate);
    TestTrue(TEXT("close makes held Jolt picker inert"), NOT HeldPicker->IsActive());
    TestFalse(TEXT("close releases the production authored shell reference"), First->Get_AuthoredShellView().IsValid());
    FString Markup; FString Css;
    if (TestTrue(TEXT("held Jolt view resources are readable"), ReadInstalledShell(Markup, Css)))
    {
        TestTrue(TEXT("held released Jolt view may reload for diagnostics"), HeldView->TryReload(Markup, Css, TEXT("Jolt held close view")).Succeeded);
        if (const TSharedPtr<SButton> ReloadedRefresh = FindButtonByTag(HeldView->GetRegion(TEXT("main")), TEXT("jolt-refresh")); ReloadedRefresh.IsValid())
        { ReloadedRefresh->SimulateClick(); }
        TestEqual(TEXT("reloaded held Jolt action stays inert after close"), FCkJoltDebuggerAuthoredShellTestAccess::RefreshDispatchCount(*First), 1);
    }
    Module.OpenDebugger(); Tick(Slate);
    const TSharedPtr<SCkJoltDebuggerWindow> ReopenedWindow = FCkJoltDebuggerModuleTestAccess::Window(Module);
    const TSharedPtr<FCkUiView> ReopenedView = ReopenedWindow.IsValid() ? ReopenedWindow->Get_AuthoredShellView() : nullptr;
    if (NOT TestTrue(TEXT("Jolt module reopens a fresh authored shell after close"), Module.IsDebuggerOpen() && ReopenedWindow.IsValid() && ReopenedView.IsValid())) { return false; }
    const TSharedPtr<SButton> ReopenedRefresh = FindButtonByTag(ReopenedView->GetRegion(TEXT("main")), TEXT("jolt-refresh"));
    if (NOT TestTrue(TEXT("reopened Jolt shell exposes its Refresh action"), ReopenedRefresh.IsValid())) { return false; }
    ReopenedRefresh->SimulateClick();
    TestEqual(TEXT("reopened Jolt Refresh dispatches once"), FCkJoltDebuggerAuthoredShellTestAccess::RefreshDispatchCount(*ReopenedWindow), 1);
    FCkJoltDebuggerModuleTestAccess::PreExit(Module); Tick(Slate);
    TestFalse(TEXT("pre-exit drops the reopened Jolt tab"), Module.IsDebuggerOpen());
    if (TestTrue(TEXT("held Jolt pre-exit resources are readable"), ReadInstalledShell(Markup, Css)))
    {
        TestTrue(TEXT("held pre-exit Jolt view may reload for diagnostics"), ReopenedView->TryReload(Markup, Css, TEXT("Jolt held pre-exit view")).Succeeded);
        if (const TSharedPtr<SButton> ReloadedRefresh = FindButtonByTag(ReopenedView->GetRegion(TEXT("main")), TEXT("jolt-refresh")); ReloadedRefresh.IsValid())
        { ReloadedRefresh->SimulateClick(); }
        TestEqual(TEXT("reloaded held Jolt action stays inert after pre-exit"), FCkJoltDebuggerAuthoredShellTestAccess::RefreshDispatchCount(*ReopenedWindow), 1);
    }
    return true;
}

#endif
