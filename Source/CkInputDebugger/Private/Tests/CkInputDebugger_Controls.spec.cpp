#include "CkInputDebugger/Window/SCkInputDebuggerWindow.h"

#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiStyledButton.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_input_debugger_controls_tests
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

    auto FindDescendantByType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == InType) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindDescendantByType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType))
            { return Found; }
        }
        return {};
    }

    auto FindTaggedDescendantByType(const TSharedRef<SWidget>& InRoot, const FName InTag, const FString& InType) -> TSharedPtr<SWidget>
    {
        const TSharedPtr<SWidget> Tagged = FindTaggedWidget(InRoot, InTag);
        return Tagged.IsValid() ? FindDescendantByType(Tagged.ToSharedRef(), InType) : TSharedPtr<SWidget>{};
    }

    auto FindText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SCkFlexText>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString() == InText) { return Text; }
        }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkFlexText> Found = FindText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            { return Found; }
        }
        return {};
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (int32 Index = 0; Index < InPath.Widgets.Num(); ++Index)
        {
            if (InPath.Widgets[Index].Widget == InWidget) { return true; }
        }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        Tick(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent MoveEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent DownEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent UpEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(MoveEvent, true);
        const FWidgetPath TargetPath = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (!WidgetPathContains(TargetPath, InWidget)) { return false; }
        const bool bDownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        const bool bUpHandled = InSlate.ProcessMouseButtonUpEvent(UpEvent);
        Tick(InSlate);
        return bDownHandled && bUpHandled;
    }

    auto SaveCapture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), true);
        return FImageUtils::SaveImageByExtension(*InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }

    auto PumpUntil(FSlateApplication& InSlate, TFunctionRef<bool()> InCondition, const double InTimeoutSeconds = 0.75) -> bool
    {
        const double Deadline = FPlatformTime::Seconds() + InTimeoutSeconds;
        do
        {
            Tick(InSlate);
            if (InCondition()) { return true; }
        }
        while (FPlatformTime::Seconds() < Deadline);
        return false;
    }

    auto ReplaceText(FSlateApplication& InSlate, const TSharedRef<SEditableText>& InInput, const FString& InText) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InInput);
        if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) { return false; }
        if (!Click(InSlate, InInput)) { return false; }
        if (InSlate.GetUserFocusedWidget(0) != InInput) { return false; }
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (!InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})) { return false; }
        for (const TCHAR Character : InText)
        {
            if (!InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false})) { return false; }
        }
        const bool bEnterHandled = InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        Tick(InSlate);
        return bEnterHandled && InInput->GetText().ToString() == InText;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputDebugger_Controls,
    "Ck.UiAuthoring.DebuggerMigration.InputControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInputDebugger_Controls::RunTest(const FString&) -> bool
{
    using namespace ck_input_debugger_controls_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Input debugger controls test requires Slate.")); return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SCkInputDebuggerWindow> Panel = SNew(SCkInputDebuggerWindow);
    TSharedPtr<SWindow> HostWindow = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{1200.0f, 760.0f})
        .CreateTitleBar(false).HasCloseButton(false)[Panel.ToSharedRef()];
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    const TSharedPtr<FCkUiView> View = Panel->Get_ControlsView();
    if (!View.IsValid() || !View->GetLastResult().Succeeded)
    {
        const FString LoadErrors = View.IsValid() ? FString::Join(View->GetLastResult().Errors, TEXT("\n"))
            : TEXT("Input Debugger did not retain its controls view.");
        AddError(FString::Printf(TEXT("Mounted production Input Debugger controls did not load installed resources: %s"), *LoadErrors));
        return false;
    }
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("controls"));
    const TArray<FName> ControlIds{TEXT("input-controls-root"), TEXT("input-active-actions"), TEXT("input-overlay"),
        TEXT("input-filter"), TEXT("input-highlight"), TEXT("input-bindings-all"), TEXT("input-bindings-rebound"), TEXT("input-bindings-default")};
    for (const FName Id : ControlIds)
    { TestTrue(*FString::Printf(TEXT("Mounted authored Input Debugger control '%s' exists"), *Id.ToString()), FindTaggedWidget(Region, Id).IsValid()); }

    const TSharedPtr<SWidget> FilterWidget = FindTaggedDescendantByType(Region, TEXT("input-filter"), TEXT("SSearchBox"));
    const TSharedPtr<SWidget> HighlightWidget = FindTaggedDescendantByType(Region, TEXT("input-highlight"), TEXT("SSearchBox"));
    const TSharedPtr<SWidget> ActiveWidget = FindTaggedDescendantByType(Region, TEXT("input-active-actions"), TEXT("SCkUiStyledButton"));
    const TSharedPtr<SWidget> OverlayWidget = FindTaggedDescendantByType(Region, TEXT("input-overlay"), TEXT("SCkUiStyledButton"));
    const TSharedPtr<SWidget> AllWidget = FindTaggedDescendantByType(Region, TEXT("input-bindings-all"), TEXT("SCkUiStyledButton"));
    const TSharedPtr<SWidget> ReboundWidget = FindTaggedDescendantByType(Region, TEXT("input-bindings-rebound"), TEXT("SCkUiStyledButton"));
    const TSharedPtr<SWidget> DefaultWidget = FindTaggedDescendantByType(Region, TEXT("input-bindings-default"), TEXT("SCkUiStyledButton"));
    if (!TestTrue(TEXT("Mounted controls have their expected concrete Slate types"),
        FilterWidget.IsValid() && FilterWidget->GetTypeAsString() == TEXT("SSearchBox")
        && HighlightWidget.IsValid() && HighlightWidget->GetTypeAsString() == TEXT("SSearchBox")
        && ActiveWidget.IsValid() && ActiveWidget->GetTypeAsString() == TEXT("SCkUiStyledButton")
        && OverlayWidget.IsValid() && OverlayWidget->GetTypeAsString() == TEXT("SCkUiStyledButton")
        && AllWidget.IsValid() && AllWidget->GetTypeAsString() == TEXT("SCkUiStyledButton")
        && ReboundWidget.IsValid() && ReboundWidget->GetTypeAsString() == TEXT("SCkUiStyledButton")
        && DefaultWidget.IsValid() && DefaultWidget->GetTypeAsString() == TEXT("SCkUiStyledButton"))) { return false; }
    const TSharedPtr<SSearchBox> Filter = StaticCastSharedPtr<SSearchBox>(FilterWidget);
    const TSharedPtr<SSearchBox> Highlight = StaticCastSharedPtr<SSearchBox>(HighlightWidget);
    const TSharedPtr<SCkUiStyledButton> Active = StaticCastSharedPtr<SCkUiStyledButton>(ActiveWidget);
    const TSharedPtr<SCkUiStyledButton> Overlay = StaticCastSharedPtr<SCkUiStyledButton>(OverlayWidget);
    const TSharedPtr<SCkUiStyledButton> All = StaticCastSharedPtr<SCkUiStyledButton>(AllWidget);
    const TSharedPtr<SCkUiStyledButton> Rebound = StaticCastSharedPtr<SCkUiStyledButton>(ReboundWidget);
    const TSharedPtr<SCkUiStyledButton> Default = StaticCastSharedPtr<SCkUiStyledButton>(DefaultWidget);
    const TSharedPtr<SWidget> FilterEditorWidget = FindDescendantByType(Filter.ToSharedRef(), TEXT("SEditableText"));
    const TSharedPtr<SWidget> HighlightEditorWidget = FindDescendantByType(Highlight.ToSharedRef(), TEXT("SEditableText"));
    if (!TestTrue(TEXT("Mounted search boxes expose their physical editable-text receivers"),
        FilterEditorWidget.IsValid() && HighlightEditorWidget.IsValid())) { return false; }
    const TSharedPtr<SEditableText> FilterEditor = StaticCastSharedPtr<SEditableText>(FilterEditorWidget);
    const TSharedPtr<SEditableText> HighlightEditor = StaticCastSharedPtr<SEditableText>(HighlightEditorWidget);

    TestTrue(TEXT("Physical routed filter input updates authoritative state"), ReplaceText(Slate, FilterEditor.ToSharedRef(), TEXT("Jump"))
        && PumpUntil(Slate, [Panel]() { return Panel->Get_FilterString() == TEXT("Jump"); }));
    TestTrue(TEXT("Physical routed highlight input updates authoritative state"), ReplaceText(Slate, HighlightEditor.ToSharedRef(), TEXT("Look"))
        && PumpUntil(Slate, [Panel]() { return Panel->Get_HighlightString() == TEXT("Look"); }));
    TestTrue(TEXT("Physical routed Active-only action updates state and its bound label"), Click(Slate, Active.ToSharedRef())
        && Panel->Get_ShowActiveActionsOnly()
        && FindText(Region, TEXT("Active only: ON")).IsValid());
    TestTrue(TEXT("Physical routed Rebound action updates authoritative binding mode"), Click(Slate, Rebound.ToSharedRef())
        && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::ReboundOnly);
    const TSharedPtr<SCkFlexText> ReboundLabel = FindText(Region, TEXT("Rebound"));
    TestTrue(TEXT("Selected Rebound action applies its authored accent color"), ReboundLabel.IsValid()
        && ReboundLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(CkStyle::Accent()));
    TestTrue(TEXT("Physical routed Default action updates authoritative binding mode"), Click(Slate, Default.ToSharedRef())
        && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::DefaultOnly);
    TestTrue(TEXT("Physical routed All action restores authoritative binding mode"), Click(Slate, All.ToSharedRef())
        && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::All);

    IConsoleVariable* OverlayCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay"));
    const int32 OriginalOverlay = OverlayCVar != nullptr ? OverlayCVar->GetInt() : 0;
    ON_SCOPE_EXIT
    {
        if (OverlayCVar != nullptr) { OverlayCVar->Set(OriginalOverlay, ECVF_SetByConsole); }
    };
    if (OverlayCVar != nullptr)
    {
        TestTrue(TEXT("Physical routed overlay action changes the available overlay CVar"), Click(Slate, Overlay.ToSharedRef())
            && OverlayCVar->GetInt() != OriginalOverlay);
    }
    else { TestFalse(TEXT("Unavailable real overlay is disabled"), Overlay->IsEnabled()); }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Debugger plugin resolves authored control resources"), Plugin.IsValid())) { return false; }
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    const FString CaptureDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/InputDebugger"));
    TestTrue(TEXT("Wide mounted controls capture writes PNG"), SaveCapture(Slate, Panel.ToSharedRef(),
        FPaths::Combine(CaptureDirectory, TEXT("Controls-Wide.png"))));
    HostWindow->Resize(FVector2D{426.0f, 426.0f});
    Tick(Slate);
    TestTrue(TEXT("Narrow mounted controls capture writes PNG"), SaveCapture(Slate, Panel.ToSharedRef(),
        FPaths::Combine(CaptureDirectory, TEXT("Controls-Narrow.png"))));
    HostWindow->Resize(FVector2D{1200.0f, 760.0f});
    Tick(Slate);
    const int64 Revision = View->GetRevision();
    if (!TestTrue(TEXT("Compatible controls reload succeeds"), View->ReloadFiles(
        FPaths::Combine(Directory, TEXT("InputDebuggerControls.ui.html")),
        FPaths::Combine(Directory, TEXT("InputDebuggerControls.ui.css"))).Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Compatible reload retains production view, controls, drafts, state, and callback binding"),
        Panel->Get_ControlsView() == View && View->GetRevision() > Revision
        && FindTaggedDescendantByType(Region, TEXT("input-filter"), TEXT("SSearchBox")) == Filter
        && FindTaggedDescendantByType(Region, TEXT("input-highlight"), TEXT("SSearchBox")) == Highlight
        && Filter->GetText().ToString() == TEXT("Jump") && Highlight->GetText().ToString() == TEXT("Look")
        && Panel->Get_ShowActiveActionsOnly() && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::All);
    const TSharedPtr<SWidget> ReloadedAllWidget = FindTaggedDescendantByType(Region, TEXT("input-bindings-all"), TEXT("SCkUiStyledButton"));
    const TSharedPtr<SWidget> ReloadedReboundWidget = FindTaggedDescendantByType(Region, TEXT("input-bindings-rebound"), TEXT("SCkUiStyledButton"));
    if (!TestTrue(TEXT("Compatible reload mounts fresh physical binding buttons"),
        ReloadedAllWidget.IsValid() && ReloadedReboundWidget.IsValid()
        && ReloadedAllWidget != All && ReloadedReboundWidget != Rebound)) { return false; }
    const TSharedPtr<SCkUiStyledButton> ReloadedAll = StaticCastSharedPtr<SCkUiStyledButton>(ReloadedAllWidget);
    const TSharedPtr<SCkUiStyledButton> ReloadedRebound = StaticCastSharedPtr<SCkUiStyledButton>(ReloadedReboundWidget);
    const int64 AcceptedRevision = View->GetRevision();
    TestFalse(TEXT("Rejected controls reload fails"), View->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected reload preserves live view, state, drafts, and callbacks"),
        Panel->Get_ControlsView() == View && View->GetRevision() == AcceptedRevision
        && Filter->GetText().ToString() == TEXT("Jump") && Highlight->GetText().ToString() == TEXT("Look")
        && FindTaggedDescendantByType(Region, TEXT("input-bindings-all"), TEXT("SCkUiStyledButton")) == ReloadedAll
        && FindTaggedDescendantByType(Region, TEXT("input-bindings-rebound"), TEXT("SCkUiStyledButton")) == ReloadedRebound);
    TestTrue(TEXT("Accepted physical Rebound action remains live after rejected reload"), Click(Slate, ReloadedRebound.ToSharedRef())
        && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::ReboundOnly);
    TestTrue(TEXT("Accepted physical All action remains live after rejected reload"), Click(Slate, ReloadedAll.ToSharedRef())
        && Panel->Get_BindingsFilterMode() == ECkInputDebugger_BindingsFilterMode::All);

    const FString ReleasedFilter = Panel->Get_FilterString();
    const FString ReleasedHighlight = Panel->Get_HighlightString();
    const bool ReleasedActiveOnly = Panel->Get_ShowActiveActionsOnly();
    const ECkInputDebugger_BindingsFilterMode ReleasedBindingsMode = Panel->Get_BindingsFilterMode();
    const TOptional<int32> ReleasedOverlayValue = OverlayCVar != nullptr
        ? TOptional<int32>{OverlayCVar->GetInt()} : TOptional<int32>{};

    const TWeakPtr<SCkInputDebuggerWindow> WeakPanel = Panel;
    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    Panel.Reset();
    Tick(Slate);
    Active->SimulateClick();
    Overlay->SimulateClick();
    Rebound->SimulateClick();
    Default->SimulateClick();
    All->SimulateClick();
    Filter->SetText(FText::FromString(TEXT("released")));
    Highlight->SetText(FText::FromString(TEXT("released")));
    TestTrue(TEXT("Owner release expires the production window"), !WeakPanel.IsValid());
    TestTrue(TEXT("Held authored controls remain inert after owner release"), !WeakPanel.IsValid()
        && ReleasedFilter == TEXT("Jump") && ReleasedHighlight == TEXT("Look") && ReleasedActiveOnly
        && ReleasedBindingsMode == ECkInputDebugger_BindingsFilterMode::All
        && (!ReleasedOverlayValue.IsSet() || (OverlayCVar != nullptr && OverlayCVar->GetInt() == ReleasedOverlayValue.GetValue())));
    return true;
}

#endif
