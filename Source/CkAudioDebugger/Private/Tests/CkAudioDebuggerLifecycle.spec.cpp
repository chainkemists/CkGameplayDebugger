#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"
#include "../../CkAudioDebugger_Module.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkAudioDebuggerLifecycleTestAccess
{
    static auto GetWindow(const FCkAudioDebuggerModule& InModule) -> TSharedPtr<SCkAudioDebuggerWindow>
    { return InModule._DebuggerWindow; }

    static auto GetTab(const FCkAudioDebuggerModule& InModule) -> TSharedPtr<SDockTab>
    { return InModule._DebuggerTab; }

    static auto PreExit(FCkAudioDebuggerModule& InModule) -> void
    { InModule.HandleEnginePreExit(); }

    static auto GetChrome(const SCkAudioDebuggerWindow& InWindow) -> TSharedPtr<SWidget>
    { return StaticCastSharedPtr<SWidget>(InWindow._Chrome); }

    static auto GetActiveOnlyToggle(const SCkAudioDebuggerWindow& InWindow) -> TSharedPtr<SCkDebug_IconToggle>
    { return InWindow._ActiveOnlyToggle; }

    static auto GetShellView(const SCkAudioDebuggerWindow& InWindow) -> TSharedPtr<FCkUiView>
    { return InWindow._AuthoredShellView; }

    static auto IsReleased(const SCkAudioDebuggerWindow& InWindow) -> bool
    { return InWindow._PresentationReleased; }

    static auto GetShowActiveOnly(const SCkAudioDebuggerWindow& InWindow) -> bool
    { return InWindow._ShowActiveOnly; }
};

namespace ck_audio_debugger_lifecycle_test
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindCheckBox(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCheckBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCheckBox"))
        { return StaticCastSharedRef<SCheckBox>(InRoot); }

        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindCheckBox(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
                Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto IsUnder(const TSharedRef<SWidget>& InRoot, const TSharedPtr<SWidget>& InCandidate) -> bool
    {
        if (NOT InCandidate.IsValid() || NOT FSlateApplication::IsInitialized())
        { return false; }

        FWidgetPath Path;
        if (NOT FSlateApplication::Get().GeneratePathToWidgetUnchecked(
            InCandidate.ToSharedRef(), Path, EVisibility::All))
        { return false; }

        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        { if (Path.Widgets[Index].Widget == InRoot) { return true; } }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_ProductionLifecycle,
    "Ck.AudioDebugger.ProductionLifecycle.Release",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_ProductionLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_lifecycle_test;

    auto& Module = FCkAudioDebuggerModule::Get();
    Module.CloseDebugger();
    Module.OpenDebugger();

    const auto Window = FCkAudioDebuggerLifecycleTestAccess::GetWindow(Module);
    const auto Tab = FCkAudioDebuggerLifecycleTestAccess::GetTab(Module);
    const auto Chrome = Window.IsValid() ? FCkAudioDebuggerLifecycleTestAccess::GetChrome(*Window) : nullptr;
    const auto ActiveOnlyToggle = Window.IsValid()
        ? FCkAudioDebuggerLifecycleTestAccess::GetActiveOnlyToggle(*Window) : nullptr;
    const auto ShellView = Window.IsValid() ? FCkAudioDebuggerLifecycleTestAccess::GetShellView(*Window) : nullptr;
    const auto CheckBox = ActiveOnlyToggle.IsValid() ? FindCheckBox(ActiveOnlyToggle.ToSharedRef()) : nullptr;

    if (NOT TestTrue(TEXT("real module path opens the Audio tab, window, authored view, and toolbar control"),
        Window.IsValid() && Tab.IsValid() && Chrome.IsValid() && ActiveOnlyToggle.IsValid()
            && ShellView.IsValid() && ShellView->GetLastResult().Succeeded && CheckBox.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TickSlate(Slate);
    CheckBox->ToggleCheckedState();
    Slate.SetKeyboardFocus(CheckBox, EFocusCause::SetDirectly);
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("real Audio control accepts input and owns focus before close"),
        FCkAudioDebuggerLifecycleTestAccess::GetShowActiveOnly(*Window)
            && IsUnder(Chrome.ToSharedRef(), Slate.GetUserFocusedWidget(0))))
    {
        Module.CloseDebugger();
        return false;
    }

    Module.CloseDebugger();
    // A detached Slate attribute is no longer traversed by the application prepass. Refresh the held control
    // explicitly before observing its live fail-closed enabled state.
    CheckBox->SlatePrepass();
    TestTrue(TEXT("ordinary close marks the retained production Audio presentation released"),
        FCkAudioDebuggerLifecycleTestAccess::IsReleased(*Window));
    TestTrue(TEXT("ordinary close detaches the retained production Audio tab"),
        Tab->GetContent() == SNullWidget::NullWidget);
    TestFalse(TEXT("ordinary close releases the retained production Audio authored shell"),
        FCkAudioDebuggerLifecycleTestAccess::GetShellView(*Window).IsValid());
    TestFalse(TEXT("ordinary close leaves the retained production Audio control enabled"),
        CheckBox->IsEnabled());
    TestFalse(TEXT("ordinary close leaves focus under the retained production Audio chrome"),
        IsUnder(Chrome.ToSharedRef(), Slate.GetUserFocusedWidget(0)));

    CheckBox->ToggleCheckedState();
    TestTrue(TEXT("held Audio toolbar control stays inert after ordinary close"),
        FCkAudioDebuggerLifecycleTestAccess::GetShowActiveOnly(*Window));
    Window->Release_Presentation();
    TestTrue(TEXT("explicit Audio presentation release is idempotent"),
        FCkAudioDebuggerLifecycleTestAccess::IsReleased(*Window));

    Module.OpenDebugger();
    const auto PreExitWindow = FCkAudioDebuggerLifecycleTestAccess::GetWindow(Module);
    const auto PreExitTab = FCkAudioDebuggerLifecycleTestAccess::GetTab(Module);
    if (NOT TestTrue(TEXT("reopen creates a distinct Audio presentation"),
        PreExitWindow.IsValid() && PreExitWindow != Window && PreExitTab.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }

    FCkAudioDebuggerLifecycleTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit releases and detaches the reopened Audio presentation"),
        FCkAudioDebuggerLifecycleTestAccess::IsReleased(*PreExitWindow)
            && PreExitTab->GetContent() == SNullWidget::NullWidget
            && NOT FCkAudioDebuggerLifecycleTestAccess::GetWindow(Module).IsValid()
            && NOT FCkAudioDebuggerLifecycleTestAccess::GetTab(Module).IsValid());
    Module.CloseDebugger();
    return true;
}

#endif
