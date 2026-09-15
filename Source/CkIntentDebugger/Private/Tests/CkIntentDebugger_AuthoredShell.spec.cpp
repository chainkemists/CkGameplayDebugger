#include "CkIntentDebugger/CkIntentDebugger_Module.h"
#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"
#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_DevicesPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_KeyStatePanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_LayerStackPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_NearMissPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_ResolutionPanel.h"
#include "CkIntentDebugger/Window/SCkIntentDebugger_TimelineDock.h"
#include "CkIntentDebugger/Window/SCkIntentDebuggerWindow.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
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
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FCkIntentDebuggerAuthoredTestAccess
{
    static auto PreExit(FCkIntentDebuggerModule& InModule) -> void { InModule.HandleEnginePreExit(); }
    static auto GetTab(const FCkIntentDebuggerModule& InModule) -> TSharedPtr<SDockTab> { return InModule._DebuggerTab; }
    static auto GetChrome(const SCkIntentDebuggerWindow& InWindow) -> TSharedPtr<SWidget> { return StaticCastSharedPtr<SWidget>(InWindow._Chrome); }
    static auto GetInputHudMenu(const SCkIntentDebuggerWindow& InWindow) -> TSharedPtr<SComboButton> { return InWindow._InputHudMenu; }
    static auto SetSources(SCkIntentDebuggerWindow& InWindow) -> void
    {
        FCkIntentDebugger_SourceSnapshot First; First.Label = TEXT("Keyboard");
        FCkIntentDebugger_SourceSnapshot Second; Second.Label = TEXT("Gamepad");
        InWindow._ViewModel->_Snapshot.Sources = {MoveTemp(First), MoveTemp(Second)};
        InWindow._ViewModel->_SelectedSourceIndex = 0;
        InWindow.Refresh_SourceSelector();
    }
    static auto SelectSource(SCkIntentDebuggerWindow& InWindow, const int32 InIndex) -> void { InWindow._ViewModel->Set_SelectedSourceIndex(InIndex); }
    static auto SelectedSource(const SCkIntentDebuggerWindow& InWindow) -> int32 { return InWindow._ViewModel->Get_SelectedSourceIndex(); }
    static auto HasSixPorts(const SCkIntentDebuggerWindow& InWindow) -> bool
    {
        return InWindow._LayerStackMount.IsValid() && InWindow._ResolutionMount.IsValid()
            && InWindow._NearMissMount.IsValid() && InWindow._TimelineMount.IsValid()
            && InWindow._KeyStateMount.IsValid() && InWindow._DevicesMount.IsValid();
    }
    static auto NativePanels(const SCkIntentDebuggerWindow& InWindow) -> TArray<TSharedPtr<SWidget>>
    {
        TArray<TSharedPtr<SWidget>> Result;
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._LayerStackPanel));
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._ResolutionPanel));
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._NearMissPanel));
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._TimelineDock));
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._KeyStatePanel));
        Result.Add(StaticCastSharedPtr<SWidget>(InWindow._DevicesPanel));
        return Result;
    }
    static auto PollAuthoredBody(SCkIntentDebuggerWindow& InWindow) -> void
    { InWindow.Poll_AuthoredBody(InWindow._NextAuthoredBodyPollSeconds); }
    static auto BodyHostOwnsAuthoredMain(const SCkIntentDebuggerWindow& InWindow) -> bool
    {
        return InWindow._AuthoredBody.IsValid() && InWindow._BodyHost.IsValid()
            && InWindow._BodyHost->GetChildren()->Num() == 1
            && InWindow._BodyHost->GetChildren()->GetChildAt(0) == InWindow._AuthoredBody->GetRegion(TEXT("main"));
    }
};

namespace ck_intent_debugger_authored_shell_test
{
    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == TEXT("SButton")) { return StaticCastSharedRef<SButton>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return {};
    }
    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        auto bTargeted = false;
        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        { bTargeted |= Path.Widgets[Index].Widget == InWidget; }
        if (!bTargeted) { return false; }
        const bool Handled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        InSlate.ProcessMouseButtonUpEvent(Up);
        InSlate.Tick();
        return Handled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkIntentDebuggerAuthoredShell,
    "Ck.UiAuthoring.IntentDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkIntentDebuggerAuthoredShell::RunTest(const FString&) -> bool
{
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Intent authored shell requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT TestTrue(TEXT("CkDebugger plugin is installed"), Plugin.IsValid())) { return false; }
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    FString Markup;
    FString Stylesheet;
    if (NOT TestTrue(TEXT("installed Intent resources load"),
        FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("IntentDebugger.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Directory, TEXT("IntentDebugger.ui.css"))))) { return false; }

    TSharedPtr<SCkIntentDebuggerWindow> IntentWindow = SNew(SCkIntentDebuggerWindow);
    TSharedPtr<SWindow> Host = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D(900.0f, 620.0f)).CreateTitleBar(false).HasCloseButton(false)[IntentWindow.ToSharedRef()];
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };
    Slate.AddWindow(Host.ToSharedRef(), true);
    Slate.Tick();
    const TSharedPtr<FCkUiView> Body = IntentWindow->Get_AuthoredBody();
    if (NOT TestTrue(TEXT("mounted production window admits its body"), Body.IsValid())) { return false; }
    const TSharedRef<SWidget> Actions = Body->GetRegion(TEXT("actions"));
    const TSharedRef<SWidget> Main = Body->GetRegion(TEXT("main"));
    TestTrue(TEXT("both authored regions mount"), Actions->GetChildren()->Num() > 0 && Main->GetChildren()->Num() > 0);
    TestTrue(TEXT("six exact native ports mount"), FCkIntentDebuggerAuthoredTestAccess::HasSixPorts(*IntentWindow));
    TestTrue(TEXT("all authored splitter topology mounts"), Body->GetSplitter(TEXT("intent-top-split")).IsValid()
        && Body->GetSplitter(TEXT("intent-bottom-split")).IsValid() && Body->GetSplitter(TEXT("intent-detail-split")).IsValid());
    FCkIntentDebuggerAuthoredTestAccess::SetSources(*IntentWindow);
    FCkIntentDebuggerAuthoredTestAccess::SelectSource(*IntentWindow, 1);
    const TSharedPtr<SCkUiRepeat> SourceRepeat = Body->GetRepeat(TEXT("intent-sources"));
    if (NOT TestTrue(TEXT("authored source collection owns its keyed repeat"), SourceRepeat.IsValid())) { return false; }
    if (NOT TestTrue(TEXT("authored source repeat materializes the published records"), SourceRepeat->TryRefresh()))
    {
        AddError(SourceRepeat->GetLastFailure());
        return false;
    }
    TestTrue(TEXT("authored source repeat contains both injected records before layout"),
        SourceRepeat->GetItemWidget(TEXT("0")).IsValid() && SourceRepeat->GetItemWidget(TEXT("1")).IsValid());
    // SetSources deliberately injects a value-only snapshot. Keep the production window's collector tick from
    // replacing that fixture snapshot while Slate performs only the geometry pass needed for global hit testing.
    const bool WindowCanTick = IntentWindow->GetCanTick();
    IntentWindow->SetCanTick(false);
    Slate.PumpMessages();
    Slate.Tick();
    IntentWindow->SetCanTick(WindowCanTick);
    const TSharedPtr<SWidget> FirstSourceItem = SourceRepeat->GetItemWidget(TEXT("0"));
    if (NOT TestTrue(TEXT("authored source repeat materializes the first keyed record"), FirstSourceItem.IsValid())) { return false; }
    const TSharedPtr<SButton> FirstSource = ck_intent_debugger_authored_shell_test::FindButton(FirstSourceItem.ToSharedRef(), TEXT("intent-source"));
    if (NOT TestTrue(TEXT("authored source action is mounted after safe value-only records"), FirstSource.IsValid())) { return false; }
    TestTrue(TEXT("global Slate hit testing routes the authored source action"), ck_intent_debugger_authored_shell_test::Click(Slate, FirstSource.ToSharedRef()));
    TestEqual(TEXT("physical authored source action selects its bound source"), FCkIntentDebuggerAuthoredTestAccess::SelectedSource(*IntentWindow), 0);
    const int64 Revision = Body->GetRevision();
    TestTrue(TEXT("compatible reload accepts installed production document"), Body->TryReload(Markup, Stylesheet, TEXT("Intent compatible reload")).Succeeded);
    TestTrue(TEXT("compatible reload preserves region identity"), Body->GetRevision() > Revision && Body->GetRegion(TEXT("main")) == Main && Body->GetRegion(TEXT("actions")) == Actions);
    TestFalse(TEXT("missing required port rejects atomically"), Body->TryReload(Markup.Replace(TEXT("intent-devices-native"), TEXT("intent-devices-missing")), Stylesheet, TEXT("Intent missing port")).Succeeded);
    TestTrue(TEXT("rejected reload retains both committed regions"), Body->GetRegion(TEXT("main")) == Main && Body->GetRegion(TEXT("actions")) == Actions);

    const FString RecoveryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/IntentAuthoredRecovery"));
    IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RecoveryDirectory, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*RecoveryDirectory, false, true); };
    const FString RecoveryMarkup = FPaths::Combine(RecoveryDirectory, TEXT("IntentDebugger.ui.html"));
    const FString RecoveryStylesheet = FPaths::Combine(RecoveryDirectory, TEXT("IntentDebugger.ui.css"));
    if (NOT TestTrue(TEXT("recovery fixture writes malformed initial resources"),
        FFileHelper::SaveStringToFile(TEXT("<ui version=\"1\"><region name=\"main\">"), *RecoveryMarkup)
        && FFileHelper::SaveStringToFile(Stylesheet, *RecoveryStylesheet))) { return false; }
    const TSharedPtr<SCkIntentDebuggerWindow> RecoveryWindow = SNew(SCkIntentDebuggerWindow).TestResourceDirectory(RecoveryDirectory);
    const TSharedPtr<SWindow> RecoveryHost = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D(900.0f, 620.0f)).CreateTitleBar(false).HasCloseButton(false)[RecoveryWindow.ToSharedRef()];
    ON_SCOPE_EXIT { if (RecoveryHost.IsValid()) { Slate.DestroyWindowImmediately(RecoveryHost.ToSharedRef()); } };
    Slate.AddWindow(RecoveryHost.ToSharedRef(), true);
    Slate.Tick();
    const TArray<TSharedPtr<SWidget>> FallbackPanels = FCkIntentDebuggerAuthoredTestAccess::NativePanels(*RecoveryWindow);
    TestTrue(TEXT("malformed startup retains native fallback and all six panels"), RecoveryWindow->IsUsingNativeBodyFallback()
        && FallbackPanels.Num() == 6 && NOT FallbackPanels.ContainsByPredicate([](const TSharedPtr<SWidget>& Panel) { return NOT Panel.IsValid(); }));
    if (NOT TestTrue(TEXT("recovery fixture atomically replaces the same resource paths"),
        FFileHelper::SaveStringToFile(Markup, *RecoveryMarkup) && FFileHelper::SaveStringToFile(Stylesheet, *RecoveryStylesheet))) { return false; }
    FCkIntentDebuggerAuthoredTestAccess::PollAuthoredBody(*RecoveryWindow);
    const TArray<TSharedPtr<SWidget>> RecoveredPanels = FCkIntentDebuggerAuthoredTestAccess::NativePanels(*RecoveryWindow);
    TestTrue(TEXT("valid same-path repair admits authored body without replacing native panels"),
        NOT RecoveryWindow->IsUsingNativeBodyFallback() && RecoveryWindow->Get_AuthoredBody().IsValid()
        && RecoveredPanels.Num() == FallbackPanels.Num() && RecoveredPanels == FallbackPanels
        && FCkIntentDebuggerAuthoredTestAccess::HasSixPorts(*RecoveryWindow)
        && FCkIntentDebuggerAuthoredTestAccess::BodyHostOwnsAuthoredMain(*RecoveryWindow));

    FCkIntentDebuggerModule& Module = FModuleManager::LoadModuleChecked<FCkIntentDebuggerModule>(TEXT("CkIntentDebugger"));
    Module.CloseDebugger();
    Module.OpenDebugger();
    Slate.PumpMessages();
    Slate.Tick();
    const TSharedPtr<SCkIntentDebuggerWindow> ModuleWindow = Module.Get_DebuggerWindow();
    const TSharedPtr<SDockTab> ModuleTab = FCkIntentDebuggerAuthoredTestAccess::GetTab(Module);
    const TSharedPtr<SWidget> ModuleChrome = ModuleWindow.IsValid() ? FCkIntentDebuggerAuthoredTestAccess::GetChrome(*ModuleWindow) : nullptr;
    const TSharedPtr<SComboButton> InputHudMenu = ModuleWindow.IsValid() ? FCkIntentDebuggerAuthoredTestAccess::GetInputHudMenu(*ModuleWindow) : nullptr;
    if (NOT TestTrue(TEXT("module open creates the real tab, chrome, and HUD menu"), ModuleWindow.IsValid() && ModuleTab.IsValid() && ModuleChrome.IsValid() && InputHudMenu.IsValid())) { return false; }
    InputHudMenu->SetIsOpen(true);
    Slate.Tick();
    TestTrue(TEXT("owned HUD popup is armed before ordinary close"), InputHudMenu->IsOpen());
    Module.CloseDebugger();
    TestTrue(TEXT("ordinary close detaches retained tab and closes owned HUD popup"),
        NOT Module.Get_DebuggerWindow().IsValid() && ModuleTab->GetContent() == SNullWidget::NullWidget && NOT InputHudMenu->IsOpen());
    ModuleWindow->Release_Presentation();
    TestTrue(TEXT("retained production window release remains idempotent"), ModuleTab->GetContent() == SNullWidget::NullWidget);
    Module.OpenDebugger();
    const TSharedPtr<SCkIntentDebuggerWindow> ReopenedWindow = Module.Get_DebuggerWindow();
    const TSharedPtr<SDockTab> ReopenedTab = FCkIntentDebuggerAuthoredTestAccess::GetTab(Module);
    if (NOT TestTrue(TEXT("module reopens with a new production window"),
        ReopenedWindow.IsValid() && ReopenedWindow != ModuleWindow && ReopenedTab.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }
    FCkIntentDebuggerAuthoredTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit detaches held production tab"), NOT Module.Get_DebuggerWindow().IsValid() && ReopenedTab->GetContent() == SNullWidget::NullWidget);
    return true;
}

#endif
