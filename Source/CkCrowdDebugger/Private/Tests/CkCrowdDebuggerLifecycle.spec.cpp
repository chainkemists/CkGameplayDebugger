#include "CkCrowdDebugger/CkCrowdDebugger_Module.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkCrowdDebuggerLifecycleTestAccess
{
    static auto GetWindow(const FCkCrowdDebuggerModule& InModule) -> TSharedPtr<SCkCrowdDebuggerWindow>
    { return InModule._Window; }
    static auto GetTab(const FCkCrowdDebuggerModule& InModule) -> TSharedPtr<SDockTab>
    { return InModule._Tab; }

    static auto PreExit(FCkCrowdDebuggerModule& InModule) -> void
    { InModule.HandleEnginePreExit(); }

    static auto GetViewport(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<SCkCrowdDebugger_3dViewport>
    { return InWindow._ViewportPanel; }
    static auto GetChrome(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<SWidget>
    { return InWindow._Chrome; }
    static auto GetDiagnosticsCombo(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<SComboButton>
    { return InWindow._DiagnosticsCombo; }
    static auto GetDiagnosticsMenuRoot(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<SWidget>
    { return InWindow._ComboMenuRoots.IsValidIndex(0) ? InWindow._ComboMenuRoots[0] : nullptr; }
    static auto GetLivePieSourceButton(const SCkCrowdDebuggerWindow& InWindow) -> TSharedPtr<SButton>
    { return InWindow._LivePieSourceButton; }
    static auto GetVoxelSource(const SCkCrowdDebuggerWindow& InWindow) -> ECkCrowdDebugger_VoxelSource
    { return InWindow._VoxelSource; }
    static auto IsPickerActive(const SCkCrowdDebuggerWindow& InWindow) -> bool
    { return InWindow._ViewportPicker.IsValid() && InWindow._ViewportPicker->IsActive(); }
};

namespace ck_crowd_debugger_lifecycle_test
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    auto IsUnder(const TSharedRef<SWidget>& InRoot, const TSharedPtr<SWidget>& InCandidate) -> bool
    {
        if (NOT InCandidate.IsValid() || NOT FSlateApplication::IsInitialized()) { return false; }
        FWidgetPath Path;
        if (NOT FSlateApplication::Get().GeneratePathToWidgetUnchecked(InCandidate.ToSharedRef(), Path, EVisibility::All)) { return false; }
        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        { if (Path.Widgets[Index].Widget == InRoot) { return true; } }
        return false;
    }

    auto FindFocusable(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SWidget>
    {
        if (InRoot->SupportsKeyboardFocus()) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindFocusable(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
                Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto ClickRetainedButton(const TSharedRef<SButton>& InButton) -> void
    {
        const auto Geometry = FGeometry::MakeRoot(FVector2D{120.0f, 24.0f}, FSlateLayoutTransform{});
        const auto Position = FVector2D{60.0f, 12.0f};
        const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
        InButton->OnMouseButtonDown(Geometry, FPointerEvent(0, Position, Position, PressedButtons,
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}));
        InButton->OnMouseButtonUp(Geometry, FPointerEvent(0, Position, Position, TSet<FKey>{},
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger_ProductionLifecycle,
    "Ck.CrowdDebugger.ProductionLifecycle.Release",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkCrowdDebugger_ProductionLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_crowd_debugger_lifecycle_test;
    FCkCrowdDebuggerModule& Module = FCkCrowdDebuggerModule::Get();
    Module.CloseDebugger();
    Module.OpenDebugger();
    const TSharedPtr<SCkCrowdDebuggerWindow> Window = FCkCrowdDebuggerLifecycleTestAccess::GetWindow(Module);
    const TSharedPtr<SDockTab> Tab = FCkCrowdDebuggerLifecycleTestAccess::GetTab(Module);
    const TSharedPtr<SCkCrowdDebugger_3dViewport> Viewport = Window.IsValid() ? FCkCrowdDebuggerLifecycleTestAccess::GetViewport(*Window) : nullptr;
    if (NOT TestTrue(TEXT("real module path opens Crowd"), Window.IsValid() && Tab.IsValid() && Viewport.IsValid())) { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    TickSlate(Slate);
    const TSharedPtr<SWidget> Chrome = FCkCrowdDebuggerLifecycleTestAccess::GetChrome(*Window);
    const TSharedPtr<SComboButton> Diagnostics = FCkCrowdDebuggerLifecycleTestAccess::GetDiagnosticsCombo(*Window);
    const TSharedPtr<SWidget> DiagnosticsMenuRoot = FCkCrowdDebuggerLifecycleTestAccess::GetDiagnosticsMenuRoot(*Window);
    const TSharedPtr<SButton> LivePieSourceButton = FCkCrowdDebuggerLifecycleTestAccess::GetLivePieSourceButton(*Window);
    if (NOT TestTrue(TEXT("real chrome retains an owned diagnostics popup and source action"),
        Chrome.IsValid() && Diagnostics.IsValid() && DiagnosticsMenuRoot.IsValid() && LivePieSourceButton.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }
    Diagnostics->SetIsOpen(true);
    TickSlate(Slate);
    const TSharedPtr<SWidget> PopupFocusTarget = FindFocusable(DiagnosticsMenuRoot.ToSharedRef());
    if (NOT TestTrue(TEXT("real diagnostics popup contains a focusable owned control"), PopupFocusTarget.IsValid()))
    {
        Module.CloseDebugger();
        return false;
    }
    Slate.SetKeyboardFocus(PopupFocusTarget, EFocusCause::SetDirectly);
    const TSharedPtr<SWidget> PopupFocusedWidget = Slate.GetUserFocusedWidget(0);
    TestTrue(TEXT("owned popup and its focus are armed before real close"),
        Diagnostics->IsOpen() && PopupFocusedWidget == PopupFocusTarget
            && IsUnder(DiagnosticsMenuRoot.ToSharedRef(), PopupFocusedWidget));
    Module.CloseDebugger();
    TestTrue(TEXT("ordinary close releases the retained Crowd preview world"),
        Viewport.IsValid() && Viewport->Get_PreviewWorld_ForTests() == nullptr);
    TestTrue(TEXT("ordinary close detaches the retained Crowd tab"),
        Tab->GetContent() == SNullWidget::NullWidget);
    TestFalse(TEXT("ordinary close leaves the retained Crowd diagnostics popup open"),
        Diagnostics->IsOpen());
    TestTrue(TEXT("ordinary close releases the retained Crowd popup focus"),
        Slate.GetUserFocusedWidget(0) != PopupFocusedWidget);
    TestFalse(TEXT("ordinary close leaves focus under the retained Crowd chrome"),
        IsUnder(Chrome.ToSharedRef(), Slate.GetUserFocusedWidget(0)));
    TestFalse(TEXT("ordinary close leaves the retained Crowd picker active"),
        FCkCrowdDebuggerLifecycleTestAccess::IsPickerActive(*Window));
    const auto VoxelSourceBeforeRetainedClick = FCkCrowdDebuggerLifecycleTestAccess::GetVoxelSource(*Window);
    ClickRetainedButton(LivePieSourceButton.ToSharedRef());
    TestEqual(TEXT("retained production source action is inert after release"),
        FCkCrowdDebuggerLifecycleTestAccess::GetVoxelSource(*Window), VoxelSourceBeforeRetainedClick);
    Window->Release_Presentation();
    TestTrue(TEXT("explicit release is idempotent for default and already-released state"),
        Viewport->Get_PreviewWorld_ForTests() == nullptr);

    Module.OpenDebugger();
    const TSharedPtr<SCkCrowdDebuggerWindow> PreExitWindow = FCkCrowdDebuggerLifecycleTestAccess::GetWindow(Module);
    const TSharedPtr<SDockTab> PreExitTab = FCkCrowdDebuggerLifecycleTestAccess::GetTab(Module);
    if (NOT TestTrue(TEXT("reopen creates a new instance"), PreExitWindow.IsValid() && PreExitWindow != Window && PreExitTab.IsValid()))
    { return false; }
    FCkCrowdDebuggerLifecycleTestAccess::PreExit(Module);
    TestTrue(TEXT("pre-exit detaches a held tab without close request"),
        PreExitTab->GetContent() == SNullWidget::NullWidget);
    Module.CloseDebugger();
    return true;
}

#endif
