#include "CkIntentDebugger/Window/SCkIntentDebugger_InputHudControls.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_intent_debugger_input_hud_controls_tests
{
    struct FSettingsRestore
    {
        UCk_InputHud_UserSettings* User = UCk_InputHud_UserSettings::Get_Mutable();
        UCk_InputHud_Settings* Project = GetMutableDefault<UCk_InputHud_Settings>();
        ECk_InputHud_MetadataMode Metadata = User->MetadataMode;
        ECk_InputHud_FrameNotation Frame = User->FrameNotation;
        ECk_InputHud_AnchorCorner AnchorCorner = User->AnchorCorner;
        float AnchorOffsetX = User->AnchorOffsetX;
        float AnchorOffsetY = User->AnchorOffsetY;
        int32 HistoryCap = Project->HistoryCap;
        float FadeSeconds = Project->FadeLifetimeSeconds;
        float TapHoldMs = Project->TapHoldThresholdMs;
        bool FrameNumbers = Project->ShowFrameNumbers;

        ~FSettingsRestore()
        {
            Project->HistoryCap = HistoryCap;
            Project->FadeLifetimeSeconds = FadeSeconds;
            Project->TapHoldThresholdMs = TapHoldMs;
            Project->ShowFrameNumbers = FrameNumbers;
            Project->SaveConfig();
            User->MetadataMode = Metadata;
            User->FrameNotation = Frame;
            User->AnchorCorner = AnchorCorner;
            User->AnchorOffsetX = AnchorOffsetX;
            User->AnchorOffsetY = AnchorOffsetY;
            User->SaveConfig();
            User->NotifyChanged();
        }

    };

    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SButton")) { return StaticCastSharedPtr<SButton>(Tagged); }
        const FChildren* Children = Tagged->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (Children->GetChildAt(Index)->GetTypeAsString() == TEXT("SButton")) { return StaticCastSharedRef<SButton>(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        }
        return {};
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox")) { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindCheckBox(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCheckBox>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SCheckBox")) { return StaticCastSharedPtr<SCheckBox>(Tagged); }
        const FChildren* Children = Tagged->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (Children->GetChildAt(Index)->GetTypeAsString() == TEXT("SCheckBox")) { return StaticCastSharedRef<SCheckBox>(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        }
        return {};
    }

    auto FocusPathContains(FSlateApplication& InSlate, const int32 InUserIndex, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWidget> Focused = InSlate.GetUserFocusedWidget(InUserIndex);
        if (!Focused.IsValid()) { return false; }
        FWidgetPath Path;
        if (!InSlate.GeneratePathToWidgetUnchecked(Focused.ToSharedRef(), Path)) { return false; }
        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        {
            if (Path.Widgets[Index].Widget == InWidget) { return true; }
        }
        return false;
    }

    auto CollectText(const TSharedRef<SWidget>& InRoot, TArray<FString>& OutTexts) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const FString Text = StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString();
            if (!Text.IsEmpty()) { OutTexts.Add(Text); }
        }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            CollectText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutTexts);
        }
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (!Window.IsValid() || !Window->GetNativeWindow().IsValid() || Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const bool bHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return bHandled;
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        return FImageUtils::SaveImageByExtension(*InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }

    auto ReplaceText(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InEditor, const FString& InText, FString* OutDraft = nullptr) -> bool
    {
        if (!FocusPathContains(InSlate, 0, InEditor)) { InSlate.SetUserFocus(0, InEditor, EFocusCause::SetDirectly); }
        Tick(InSlate);
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (!InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})) { return false; }
        for (const TCHAR Character : InText)
        {
            if (!InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false})) { return false; }
        }
        if (OutDraft != nullptr)
        {
            InEditor->SelectAllText();
            *OutDraft = InEditor->GetSelectedText().ToString();
        }
        return true;
    }

    auto ReplaceAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InEditor, const FString& InText, FString* OutDraftBeforeCommit = nullptr) -> bool
    {
        if (!ReplaceText(InSlate, InEditor, InText, OutDraftBeforeCommit)) { return false; }
        if (!InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0})) { return false; }
        Tick(InSlate);
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkIntentDebugger_InputHudControlsConstruction,
    "Ck.IntentDebugger.InputHudControls.Construction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkIntentDebugger_InputHudControlsConstruction::RunTest(const FString&) -> bool
{
    const auto Controls = SNew(SCkIntentDebugger_InputHudControls);
    Controls->SlatePrepass();

    TestTrue(TEXT("Intent Debugger composes the operational Input HUD tuner popover"),
        Controls->GetDesiredSize().X > 0.0f && Controls->GetDesiredSize().Y > 0.0f);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkIntentDebugger_InputHudControlsAuthored,
    "Ck.UiAuthoring.IntentDebugger.InputHudControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkIntentDebugger_InputHudControlsAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_intent_debugger_input_hud_controls_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Intent HUD controls test requires Slate.")); return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    const FSettingsRestore Restore;
    const bool bSessionCVarsAvailable = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")) != nullptr;
    const TSharedRef<bool> IsMenuOpen = MakeShared<bool>(false);
    TSharedPtr<SCkIntentDebugger_InputHudControls> Controls = SNew(SCkIntentDebugger_InputHudControls)
        .CanDispatchEvents_Lambda([IsMenuOpen] { return *IsMenuOpen; });
    const TWeakPtr<SCkIntentDebugger_InputHudControls> WeakControls = Controls;
    TSharedPtr<SComboButton> MenuHost = SNew(SComboButton)
        .OnMenuOpenChanged_Lambda([IsMenuOpen](const bool bIsOpen)
        {
            *IsMenuOpen = bIsOpen;
        })
        .ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("HUD settings")))]
        .MenuContent()[SNew(SBox).WidthOverride(320.0f)[Controls.ToSharedRef()]];
    TSharedPtr<SWindow> Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D(360.0f, 420.0f)).CreateTitleBar(false).HasCloseButton(false)[MenuHost.ToSharedRef()];
    ON_SCOPE_EXIT { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } };
    Slate.AddWindow(Window.ToSharedRef(), true);
    MenuHost->SetIsOpen(true);
    Tick(Slate);
    const auto EnsureOpen = [&Slate, &MenuHost]() -> bool
    {
        if (!MenuHost->IsOpen()) { MenuHost->SetIsOpen(true); }
        Tick(Slate);
        return MenuHost->IsOpen();
    };

    TSharedPtr<FCkUiView> View = Controls->Get_ControlsView();
    const TWeakPtr<FCkUiView> WeakView = View;
    if (!View.IsValid() || !View->GetLastResult().Succeeded)
    {
        TArray<FString> RenderedTexts;
        CollectText(Controls.ToSharedRef(), RenderedTexts);
        AddError(FString::Printf(TEXT("Intent HUD authored load failed; rendered production fallback: %s"), *FString::Join(RenderedTexts, TEXT(" | "))));
        return false;
    }
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const TSharedPtr<SScrollBox> Scroll = View->GetScroll(TEXT("intent-hud-body"));
    if (!TestTrue(TEXT("Intent HUD document mounts its authored root and scroll body"), Scroll.IsValid() && FindTagged(Region, TEXT("intent-hud-controls-root")).IsValid())) { return false; }

    const TSharedPtr<SButton> ClosedAction = FindButton(Region, TEXT("intent-hud-metadata-next"));
    if (!TestTrue(TEXT("Representative host exposes a mounted authored action"), ClosedAction.IsValid())) { return false; }
    const auto MetadataBeforeClose = UCk_InputHud_UserSettings::Get_MetadataMode();
    MenuHost->SetIsOpen(false);
    Tick(Slate);
    ClosedAction->SimulateClick();
    TestEqual(TEXT("Closed menu gates held authored callbacks"), static_cast<uint8>(UCk_InputHud_UserSettings::Get_MetadataMode()), static_cast<uint8>(MetadataBeforeClose));
    MenuHost->SetIsOpen(true);
    Tick(Slate);
    if (!TestTrue(TEXT("Representative menu reopens before native interaction"), MenuHost->IsOpen())) { return false; }

    const auto GetReadoutValue = [](const FString& InCycle) -> int64
    {
        if (InCycle == TEXT("metadata")) { return static_cast<int64>(UCk_InputHud_UserSettings::Get_MetadataMode()); }
        return static_cast<int64>(UCk_InputHud_UserSettings::Get_FrameNotation());
    };
    const TCHAR* Cycles[] = {TEXT("metadata"), TEXT("frame")};
    for (const TCHAR* Cycle : Cycles)
    {
        const FString NextId = FString::Printf(TEXT("intent-hud-%s-next"), Cycle);
        const FString PreviousId = FString::Printf(TEXT("intent-hud-%s-previous"), Cycle);
        const TSharedPtr<SButton> Next = FindButton(Region, FName(*NextId));
        const TSharedPtr<SButton> Previous = FindButton(Region, FName(*PreviousId));
        if (!TestTrue(*FString::Printf(TEXT("%s authored cycle controls mount"), *NextId), Next.IsValid() && Previous.IsValid())) { return false; }
        Scroll->ScrollDescendantIntoView(Next.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(*FString::Printf(TEXT("%s reopens the combo before dispatch"), *NextId), EnsureOpen())) { return false; }
        const int64 ValueBefore = GetReadoutValue(Cycle);
        const uint32 RevisionBefore = UCk_InputHud_UserSettings::Get_Revision();
        if (!TestTrue(*FString::Printf(TEXT("%s routes through the mounted menu-hosted control"), *NextId), Click(Slate, Next.ToSharedRef()))) { return false; }
        TestTrue(*FString::Printf(TEXT("%s changes and publishes its user setting"), *NextId), GetReadoutValue(Cycle) != ValueBefore && UCk_InputHud_UserSettings::Get_Revision() > RevisionBefore);
    }

    const TSharedPtr<SButton> ModeNext = FindButton(Region, TEXT("intent-hud-mode-next"));
    const TSharedPtr<SButton> CornerNext = FindButton(Region, TEXT("intent-hud-corner-next"));
    const TSharedPtr<SEditableTextBox> SessionScale = FindEditor(Region, TEXT("intent-hud-scale-input"));
    if (!TestTrue(TEXT("Session fallback controls mount"), ModeNext.IsValid() && CornerNext.IsValid() && SessionScale.IsValid())) { return false; }
    if (!bSessionCVarsAvailable)
    {
        const uint32 RevisionBeforeMissingCVar = UCk_InputHud_UserSettings::Get_Revision();
        const auto AnchorBeforeMissingCVar = UCk_InputHud_UserSettings::Get_AnchorCorner();
        const float AnchorXBeforeMissingCVar = UCk_InputHud_UserSettings::Get_AnchorOffsetX();
        const float AnchorYBeforeMissingCVar = UCk_InputHud_UserSettings::Get_AnchorOffsetY();
        Scroll->ScrollDescendantIntoView(ModeNext.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(TEXT("Missing-CVar mode action remains safely dispatchable"), EnsureOpen() && Click(Slate, ModeNext.ToSharedRef()))) { return false; }
        Scroll->ScrollDescendantIntoView(CornerNext.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(TEXT("Missing-CVar corner action remains safely dispatchable"), EnsureOpen() && Click(Slate, CornerNext.ToSharedRef()))) { return false; }
        Scroll->ScrollDescendantIntoView(SessionScale.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(TEXT("Missing-CVar numeric commit remains a valid no-op"), EnsureOpen() && ReplaceAndCommit(Slate, SessionScale.ToSharedRef(), TEXT("1.25")))) { return false; }
        TestTrue(TEXT("Missing-CVar dispatch preserves settings and leaves all session Cvars absent"), !SessionScale->HasError()
            && UCk_InputHud_UserSettings::Get_Revision() == RevisionBeforeMissingCVar
            && UCk_InputHud_UserSettings::Get_AnchorCorner() == AnchorBeforeMissingCVar
            && FMath::IsNearlyEqual(UCk_InputHud_UserSettings::Get_AnchorOffsetX(), AnchorXBeforeMissingCVar)
            && FMath::IsNearlyEqual(UCk_InputHud_UserSettings::Get_AnchorOffsetY(), AnchorYBeforeMissingCVar)
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay")) == nullptr
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay.Scale")) == nullptr
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay.Opacity")) == nullptr
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay.Corner")) == nullptr
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay.OffsetX")) == nullptr
            && IConsoleManager::Get().FindConsoleVariable(TEXT("ck.InputOverlay.OffsetY")) == nullptr);
    }
    else { AddInfo(TEXT("Missing-CVar fallback scenario unavailable because a pre-existing local player registered session Cvars.")); }

    struct FNumberCase { const TCHAR* Id; float Target; TFunction<float()> Get; };
    const FNumberCase Numbers[] = {
        {TEXT("history-cap"), 12.0f, [] { return static_cast<float>(UCk_InputHud_Settings::Get_HistoryCap()); }},
        {TEXT("fade-seconds"), 12.5f, [] { return UCk_InputHud_Settings::Get_FadeLifetimeSeconds(); }},
        {TEXT("tap-hold-ms"), 420.0f, [] { return UCk_InputHud_Settings::Get_TapHoldThresholdMs(); }}};
    for (const FNumberCase& Number : Numbers)
    {
        const FString Id = FString::Printf(TEXT("intent-hud-%s-input"), Number.Id);
        const TSharedPtr<SEditableTextBox> Editor = FindEditor(Region, FName(*Id));
        if (!TestTrue(*FString::Printf(TEXT("%s real number input mounts"), *Id), Editor.IsValid())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(*FString::Printf(TEXT("%s reopens the combo before native Enter"), *Id), EnsureOpen())) { return false; }
        const float Target = FMath::IsNearlyEqual(Number.Get(), Number.Target) ? Number.Target * 0.5f : Number.Target;
        const uint32 RevisionBefore = UCk_InputHud_UserSettings::Get_Revision();
        if (!TestTrue(*FString::Printf(TEXT("%s commits through focused native input"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), FString::SanitizeFloat(Target)))) { return false; }
        TestTrue(*FString::Printf(TEXT("%s updates its production scope"), *Id), FMath::IsNearlyEqual(Number.Get(), Target));
        if (FCString::Strcmp(Number.Id, TEXT("history-cap")) == 0 || FCString::Strcmp(Number.Id, TEXT("fade-seconds")) == 0 || FCString::Strcmp(Number.Id, TEXT("tap-hold-ms")) == 0)
        {
            TestTrue(*FString::Printf(TEXT("%s publishes the project setting"), *Id), UCk_InputHud_UserSettings::Get_Revision() > RevisionBefore);
        }
    }

    struct FProjectRangeCase { const TCHAR* Id; FString Low; FString High; FString OutOfRange; float Clamped; TFunction<float()> Get; };
    const FProjectRangeCase ProjectRanges[] = {
        {TEXT("history-cap"), TEXT("3"), TEXT("20"), TEXT("2"), 3.0f, [] { return static_cast<float>(UCk_InputHud_Settings::Get_HistoryCap()); }},
        {TEXT("fade-seconds"), TEXT("3.0"), TEXT("30.0"), TEXT("31.0"), 30.0f, [] { return UCk_InputHud_Settings::Get_FadeLifetimeSeconds(); }},
        {TEXT("tap-hold-ms"), TEXT("50"), TEXT("2000"), TEXT("49"), 50.0f, [] { return UCk_InputHud_Settings::Get_TapHoldThresholdMs(); }}};
    for (const FProjectRangeCase& Range : ProjectRanges)
    {
        const FString Id = FString::Printf(TEXT("intent-hud-%s-input"), Range.Id);
        const TSharedPtr<SEditableTextBox> Editor = FindEditor(Region, FName(*Id));
        if (!TestTrue(*FString::Printf(TEXT("%s remains mounted for range coverage"), *Id), Editor.IsValid())) { return false; }
        for (const FString& Boundary : {Range.Low, Range.High})
        {
            if (!TestTrue(*FString::Printf(TEXT("%s reopens before boundary commit"), *Id), EnsureOpen())) { return false; }
            Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
            Tick(Slate);
            if (!TestTrue(*FString::Printf(TEXT("%s commits its boundary through the visible native editor"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), Boundary))) { return false; }
            TestTrue(*FString::Printf(TEXT("%s accepts its authored boundary"), *Id), FMath::IsNearlyEqual(Range.Get(), FCString::Atof(*Boundary)));
        }
        const float BeforeOutOfRange = Range.Get();
        const uint32 RevisionBeforeOutOfRange = UCk_InputHud_UserSettings::Get_Revision();
        if (!TestTrue(*FString::Printf(TEXT("%s reopens before out-of-range draft"), *Id), EnsureOpen())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(*FString::Printf(TEXT("%s routes its out-of-range draft through the visible native editor"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), Range.OutOfRange))) { return false; }
        TestTrue(*FString::Printf(TEXT("%s clamps its finite out-of-range draft and publishes only when effective value changes"), *Id), !Editor->HasError()
            && FMath::IsNearlyEqual(Range.Get(), Range.Clamped)
            && UCk_InputHud_UserSettings::Get_Revision() == RevisionBeforeOutOfRange + (FMath::IsNearlyEqual(BeforeOutOfRange, Range.Clamped) ? 0 : 1));
        const float BeforeMalformed = Range.Get();
        const uint32 RevisionBeforeMalformed = UCk_InputHud_UserSettings::Get_Revision();
        if (!TestTrue(*FString::Printf(TEXT("%s reopens before malformed draft"), *Id), EnsureOpen())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        FString MalformedDraft;
        if (!TestTrue(*FString::Printf(TEXT("%s routes its malformed draft through the visible native editor"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), TEXT("not-a-number"), &MalformedDraft))) { return false; }
        TestEqual(*FString::Printf(TEXT("%s native editor retains the exact malformed draft through Enter"), *Id), MalformedDraft, FString(TEXT("not-a-number")));
        TestEqual(*FString::Printf(TEXT("%s malformed draft leaves its normalized setting unchanged"), *Id), Range.Get(), BeforeMalformed);
        TestEqual(*FString::Printf(TEXT("%s malformed draft leaves the settings revision unchanged"), *Id), UCk_InputHud_UserSettings::Get_Revision(), RevisionBeforeMalformed);
        if (!TestTrue(*FString::Printf(TEXT("%s reopens before deferred native error presentation"), *Id), EnsureOpen())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        TestTrue(*FString::Printf(TEXT("%s presents the malformed-draft error after its remounted editor ticks"), *Id), Editor->HasError());
        if (!TestTrue(*FString::Printf(TEXT("%s reopens before canonical no-op"), *Id), EnsureOpen())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        if (!TestTrue(*FString::Printf(TEXT("%s routes its canonical no-op through the visible native editor"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), FString::SanitizeFloat(BeforeMalformed)))) { return false; }
        TestEqual(*FString::Printf(TEXT("%s canonical no-op leaves its normalized setting unchanged"), *Id), Range.Get(), BeforeMalformed);
        TestEqual(*FString::Printf(TEXT("%s canonical no-op leaves the settings revision unchanged"), *Id), UCk_InputHud_UserSettings::Get_Revision(), RevisionBeforeMalformed);
        if (!TestTrue(*FString::Printf(TEXT("%s reopens before cleared native error presentation"), *Id), EnsureOpen())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        TestFalse(*FString::Printf(TEXT("%s canonical no-op clears the native error after its remounted editor ticks"), *Id), Editor->HasError());
    }

    const TSharedPtr<SEditableTextBox> FadePrecision = FindEditor(Region, TEXT("intent-hud-fade-seconds-input"));
    const TSharedPtr<SEditableTextBox> TapPrecision = FindEditor(Region, TEXT("intent-hud-tap-hold-ms-input"));
    if (!TestTrue(TEXT("Project precision editors remain mounted"), FadePrecision.IsValid() && TapPrecision.IsValid())) { return false; }
    if (!TestTrue(TEXT("One-decimal project editor reopens before native Enter"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(FadePrecision.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    if (!TestTrue(TEXT("One-decimal project editor commits through visible native Enter"), ReplaceAndCommit(Slate, FadePrecision.ToSharedRef(), TEXT("12.5")))) { return false; }
    Slate.ClearKeyboardFocus(EFocusCause::Cleared);
    Tick(Slate);
    TestEqual(TEXT("Fade editor presents its authored one-decimal value"), FadePrecision->GetText().ToString(), FString(TEXT("12.5")));
    if (!TestTrue(TEXT("Zero-decimal project editor reopens before native Enter"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(TapPrecision.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    if (!TestTrue(TEXT("Zero-decimal project editor commits through visible native Enter"), ReplaceAndCommit(Slate, TapPrecision.ToSharedRef(), TEXT("420")))) { return false; }
    Slate.ClearKeyboardFocus(EFocusCause::Cleared);
    Tick(Slate);
    TestEqual(TEXT("Tap editor presents its authored zero-decimal value"), TapPrecision->GetText().ToString(), FString(TEXT("420")));

    const TSharedPtr<SCheckBox> Frames = FindCheckBox(Region, TEXT("intent-hud-frames-allowed-input"));
    if (!TestTrue(TEXT("Authored frame-number checkbox mounts"), Frames.IsValid())) { return false; }
    Scroll->ScrollDescendantIntoView(Frames.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    if (!TestTrue(TEXT("Frame-number checkbox reopens the combo before dispatch"), EnsureOpen())) { return false; }
    const bool FramesBefore = UCk_InputHud_Settings::Get_ShowFrameNumbers();
    if (!TestTrue(TEXT("Frame-number checkbox routes through mounted control"), Click(Slate, Frames.ToSharedRef()))) { return false; }
    TestEqual(TEXT("Frame-number checkbox changes project state"), UCk_InputHud_Settings::Get_ShowFrameNumbers(), !FramesBefore);

    const TSharedPtr<SButton> Reset = FindButton(Region, TEXT("intent-hud-reset-readout"));
    const auto AnchorCornerBeforeReset = UCk_InputHud_UserSettings::Get_AnchorCorner();
    const float AnchorXBeforeReset = UCk_InputHud_UserSettings::Get_AnchorOffsetX();
    const float AnchorYBeforeReset = UCk_InputHud_UserSettings::Get_AnchorOffsetY();
    const int32 HistoryBeforeReset = UCk_InputHud_Settings::Get_HistoryCap();
    const float FadeBeforeReset = UCk_InputHud_Settings::Get_FadeLifetimeSeconds();
    const float TapBeforeReset = UCk_InputHud_Settings::Get_TapHoldThresholdMs();
    const bool FramesBeforeReset = UCk_InputHud_Settings::Get_ShowFrameNumbers();
    if (Reset.IsValid()) { Scroll->ScrollDescendantIntoView(Reset.ToSharedRef(), false, EDescendantScrollDestination::IntoView); Tick(Slate); }
    if (!TestTrue(TEXT("Readout reset reopens the combo before dispatch"), EnsureOpen())) { return false; }
    if (!TestTrue(TEXT("Authored readout reset mounts"), Reset.IsValid() && Click(Slate, Reset.ToSharedRef()))) { return false; }
    TestTrue(TEXT("Readout reset restores only readout defaults and preserves other production domains"), UCk_InputHud_UserSettings::Get_MetadataMode() == ECk_InputHud_MetadataMode::Full
        && UCk_InputHud_UserSettings::Get_FrameNotation() == ECk_InputHud_FrameNotation::Delta
        && UCk_InputHud_UserSettings::Get_AnchorCorner() == AnchorCornerBeforeReset
        && FMath::IsNearlyEqual(UCk_InputHud_UserSettings::Get_AnchorOffsetX(), AnchorXBeforeReset)
        && FMath::IsNearlyEqual(UCk_InputHud_UserSettings::Get_AnchorOffsetY(), AnchorYBeforeReset)
        && UCk_InputHud_Settings::Get_HistoryCap() == HistoryBeforeReset
        && FMath::IsNearlyEqual(UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), FadeBeforeReset)
        && FMath::IsNearlyEqual(UCk_InputHud_Settings::Get_TapHoldThresholdMs(), TapBeforeReset)
        && UCk_InputHud_Settings::Get_ShowFrameNumbers() == FramesBeforeReset);

    const TSharedPtr<SEditableTextBox> ReloadDraft = FindEditor(Region, TEXT("intent-hud-fade-seconds-input"));
    if (!TestTrue(TEXT("Draft editor mounts for compatible reload"), ReloadDraft.IsValid())) { return false; }
    if (!TestTrue(TEXT("Reload draft reopens the combo before native editing"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(ReloadDraft.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    Slate.SetUserFocus(0, ReloadDraft.ToSharedRef(), EFocusCause::SetDirectly);
    if (!TestTrue(TEXT("Reload draft editor is in the focused native leaf ancestry before typing"), FocusPathContains(Slate, 0, ReloadDraft.ToSharedRef()))) { return false; }
    ReloadDraft->SelectAllText();
    Slate.ProcessKeyCharEvent(FCharacterEvent{TEXT('1'), FModifierKeysState{}, 0, false});
    Tick(Slate);
    ReloadDraft->SelectAllText();
    const FString DraftText = ReloadDraft->GetSelectedText().ToString();
    if (!TestTrue(TEXT("Reload draft editor retains the controlled active edit buffer before reload"), DraftText == TEXT("1") && !FMath::IsNearlyEqual(UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), 1.0f))) { return false; }
    const TSharedPtr<SWidget> FocusBeforeReload = Slate.GetUserFocusedWidget(0);
    if (!TestTrue(TEXT("Reload captures a non-null focused native leaf"), FocusBeforeReload.IsValid())) { return false; }
    const bool bMenuOpenBeforeReload = MenuHost->IsOpen();
    const int64 AcceptedRevision = View->GetRevision();
    const float FadeModelBeforeReload = UCk_InputHud_Settings::Get_FadeLifetimeSeconds();
    const uint32 SettingsRevisionBeforeReload = UCk_InputHud_UserSettings::Get_Revision();
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Installed Intent HUD resources resolve for reload"), Plugin.IsValid())) { return false; }
    const FString Resource = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    const FString MarkupPath = FPaths::Combine(Resource, TEXT("IntentInputHudControls.ui.html"));
    const FString CssPath = FPaths::Combine(Resource, TEXT("IntentInputHudControls.ui.css"));
    if (!TestTrue(TEXT("Compatible authored reload preserves the native numeric draft"), View->ReloadFiles(MarkupPath, CssPath).Succeeded)) { return false; }
    Tick(Slate);
    ReloadDraft->SelectAllText();
    TestEqual(TEXT("Compatible reload advances the revision once"), View->GetRevision(), AcceptedRevision + 1);
    TestTrue(TEXT("Compatible reload retains numeric editor identity"), FindEditor(Region, TEXT("intent-hud-fade-seconds-input")) == ReloadDraft);
    TestEqual(TEXT("Compatible reload preserves the combo open state"), MenuHost->IsOpen(), bMenuOpenBeforeReload);
    TestTrue(TEXT("Compatible reload retains numeric editor focus"), Slate.GetUserFocusedWidget(0) == FocusBeforeReload);
    TestEqual(TEXT("Compatible reload retains numeric editor active draft"), ReloadDraft->GetSelectedText().ToString(), DraftText);
    TestEqual(TEXT("Compatible reload preserves the fade model while the draft is uncommitted"), UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), FadeModelBeforeReload);
    TestEqual(TEXT("Compatible reload does not publish the uncommitted draft"), UCk_InputHud_UserSettings::Get_Revision(), SettingsRevisionBeforeReload);
    FString Markup;
    FString Css;
    if (!TestTrue(TEXT("Installed Intent HUD resources read for rejected reload"), FFileHelper::LoadFileToString(Markup, *MarkupPath) && FFileHelper::LoadFileToString(Css, *CssPath))) { return false; }
    TestTrue(TEXT("Rejected fixture corrupts a required number binding"), Markup.ReplaceInline(TEXT("value-bind=\"intent-hud-scale\""), TEXT("value-bind=\"missing-scale\"")) == 1);
    const TSharedPtr<SEditableTextBox> RejectedDraft = FindEditor(Region, TEXT("intent-hud-fade-seconds-input"));
    if (!TestTrue(TEXT("Accepted editor remains mounted for rejected reload baseline"), RejectedDraft.IsValid())) { return false; }
    RejectedDraft->SelectAllText();
    const FString RejectedDraftText = RejectedDraft->GetSelectedText().ToString();
    const TSharedPtr<SWidget> FocusBeforeRejectedReload = Slate.GetUserFocusedWidget(0);
    if (!TestTrue(TEXT("Rejected reload baseline has a focused native editor leaf"), FocusBeforeRejectedReload.IsValid() && FocusPathContains(Slate, 0, RejectedDraft.ToSharedRef()))) { return false; }
    const bool bMenuOpenBeforeRejectedReload = MenuHost->IsOpen();
    const int64 RejectedRevision = View->GetRevision();
    TestFalse(TEXT("Rejected Intent HUD reload is atomic"), View->TryReload(Markup, Css).Succeeded);
    RejectedDraft->SelectAllText();
    TestEqual(TEXT("Rejected reload leaves the accepted revision unchanged"), View->GetRevision(), RejectedRevision);
    TestTrue(TEXT("Rejected reload retains accepted numeric editor identity"), FindEditor(Region, TEXT("intent-hud-fade-seconds-input")) == RejectedDraft);
    TestEqual(TEXT("Rejected reload preserves the combo open state"), MenuHost->IsOpen(), bMenuOpenBeforeRejectedReload);
    TestTrue(TEXT("Rejected reload retains accepted numeric editor focus"), Slate.GetUserFocusedWidget(0) == FocusBeforeRejectedReload);
    TestEqual(TEXT("Rejected reload retains accepted numeric editor active draft"), RejectedDraft->GetSelectedText().ToString(), RejectedDraftText);
    TestEqual(TEXT("Rejected reload preserves the fade model while the draft is uncommitted"), UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), FadeModelBeforeReload);
    TestEqual(TEXT("Rejected reload does not publish the uncommitted draft"), UCk_InputHud_UserSettings::Get_Revision(), SettingsRevisionBeforeReload);

    const float EnterCommitTarget = FMath::IsNearlyEqual(FadeModelBeforeReload, 11.5f) ? 12.5f : 11.5f;
    if (!TestTrue(TEXT("Post-rejected Enter commit reopens the combo"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(RejectedDraft.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const uint32 RevisionBeforeEnterCommit = UCk_InputHud_UserSettings::Get_Revision();
    if (!TestTrue(TEXT("Post-rejected native Enter remains dispatchable"), ReplaceAndCommit(Slate, RejectedDraft.ToSharedRef(), FString::SanitizeFloat(EnterCommitTarget)))) { return false; }
    TestEqual(TEXT("Post-rejected native Enter commits the fade model"), UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), EnterCommitTarget);
    TestEqual(TEXT("Post-rejected native Enter publishes once"), UCk_InputHud_UserSettings::Get_Revision(), RevisionBeforeEnterCommit + 1);

    const TSharedPtr<SEditableTextBox> FocusLossTarget = FindEditor(Region, TEXT("intent-hud-tap-hold-ms-input"));
    if (!TestTrue(TEXT("Second native editor mounts for ordinary focus-loss commit"), FocusLossTarget.IsValid())) { return false; }
    const float FocusLossCommitTarget = FMath::IsNearlyEqual(UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), 13.5f) ? 14.5f : 13.5f;
    if (!TestTrue(TEXT("Ordinary focus-loss commit reopens the combo"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(RejectedDraft.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    if (!TestTrue(TEXT("Ordinary focus-loss stages a valid draft in the mounted editor"), ReplaceText(Slate, RejectedDraft.ToSharedRef(), FString::SanitizeFloat(FocusLossCommitTarget)))) { return false; }
    Scroll->ScrollDescendantIntoView(FocusLossTarget.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const uint32 RevisionBeforeFocusLossCommit = UCk_InputHud_UserSettings::Get_Revision();
    Slate.SetUserFocus(0, FocusLossTarget.ToSharedRef(), EFocusCause::SetDirectly);
    Tick(Slate);
    TestTrue(TEXT("Ordinary native focus loss remains within the open combo"), MenuHost->IsOpen() && FocusPathContains(Slate, 0, FocusLossTarget.ToSharedRef()));
    TestEqual(TEXT("Ordinary native focus loss commits the fade model"), UCk_InputHud_Settings::Get_FadeLifetimeSeconds(), FocusLossCommitTarget);
    TestEqual(TEXT("Ordinary native focus loss publishes once"), UCk_InputHud_UserSettings::Get_Revision(), RevisionBeforeFocusLossCommit + 1);

    const TSharedPtr<SButton> CurrentAction = FindButton(Region, TEXT("intent-hud-metadata-next"));
    if (!TestTrue(TEXT("Accepted reload exposes its current mounted metadata action"), CurrentAction.IsValid())) { return false; }
    Scroll->ScrollDescendantIntoView(CurrentAction.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const auto MetadataBeforeLiveAction = UCk_InputHud_UserSettings::Get_MetadataMode();
    if (!TestTrue(TEXT("Current action reopens the combo before live dispatch"), EnsureOpen() && Click(Slate, CurrentAction.ToSharedRef()))) { return false; }
    TestTrue(TEXT("Current post-reload metadata action is live"), UCk_InputHud_UserSettings::Get_MetadataMode() != MetadataBeforeLiveAction);

    const TSharedPtr<SEditableTextBox> HeldEditor = FindEditor(Region, TEXT("intent-hud-tap-hold-ms-input"));
    if (!TestTrue(TEXT("Last numeric editor remains reachable in narrow menu"), HeldEditor.IsValid())) { return false; }
    Window->Resize(FVector2D(260.0f, 260.0f));
    if (!TestTrue(TEXT("Narrow final editor reopens the combo before scrolling"), EnsureOpen())) { return false; }
    Scroll->ScrollDescendantIntoView(HeldEditor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const FGeometry ScrollGeometry = Scroll->GetCachedGeometry();
    const FVector2D EditorCenter = ScrollGeometry.AbsoluteToLocal(HeldEditor->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));
    TestTrue(TEXT("Narrow menu scroll reaches the final authored numeric editor in its visible viewport"), HeldEditor->GetCachedGeometry().GetLocalSize().X > 4.0f
        && HeldEditor->GetCachedGeometry().GetLocalSize().Y > 4.0f && EditorCenter.X >= 0.0f && EditorCenter.Y >= 0.0f
        && EditorCenter.X <= ScrollGeometry.GetLocalSize().X && EditorCenter.Y <= ScrollGeometry.GetLocalSize().Y);
    const FString CaptureDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/IntentInputHud"));
    IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
    TestTrue(TEXT("Narrow Intent HUD menu viewport capture writes"), Capture(Slate, Scroll.ToSharedRef(), FPaths::Combine(CaptureDirectory, TEXT("IntentInputHud-Narrow.png"))));

    const uint32 OwnerReleaseRevision = UCk_InputHud_UserSettings::Get_Revision();
    Slate.DestroyWindowImmediately(Window.ToSharedRef());
    Window.Reset();
    MenuHost.Reset();
    Controls.Reset();
    View.Reset();
    TestFalse(TEXT("Destroyed menu host releases the Input HUD controls pane and view"), WeakControls.IsValid() || WeakView.IsValid());
    CurrentAction->SimulateClick();
    TestEqual(TEXT("Held mounted action is inert after the controls owner expires"), UCk_InputHud_UserSettings::Get_Revision(), OwnerReleaseRevision);
    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
