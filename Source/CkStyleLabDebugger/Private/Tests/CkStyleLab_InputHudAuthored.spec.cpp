#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_style_lab_input_hud_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    auto SettleNativePicker(FSlateApplication& InSlate, const TSharedRef<SColorPicker>& InPicker) -> bool
    {
        const double Deadline = FPlatformTime::Seconds() + 2.0;
        while (InPicker->HasActiveTimers() && FPlatformTime::Seconds() < Deadline)
        {
            FPlatformProcess::Sleep(0.01f);
            Tick(InSlate);
        }
        return !InPicker->HasActiveTimers();
    }

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

    auto FindInspector(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkDebug_InspectorPanel>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, TEXT("style-lab-input-hud"));
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel")) { return StaticCastSharedPtr<SCkDebug_InspectorPanel>(Tagged); }
        const FChildren* Children = Tagged->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (Children->GetChildAt(Index)->GetTypeAsString() == TEXT("SCkDebug_InspectorPanel")) { return StaticCastSharedRef<SCkDebug_InspectorPanel>(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); }
        }
        return {};
    }

    auto FindColorPicker(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SColorPicker>
    {
        if (InRoot->GetTypeAsString() == TEXT("SColorPicker")) { return StaticCastSharedRef<SColorPicker>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SColorPicker> Found = FindColorPicker(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindHexEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox"))
        {
            const TSharedRef<SEditableTextBox> Editor = StaticCastSharedRef<SEditableTextBox>(InRoot);
            const FString Text = Editor->GetText().ToString();
            bool bIsHex = Text.Len() == 8;
            for (const TCHAR Character : Text)
            {
                bIsHex &= (Character >= TEXT('0') && Character <= TEXT('9')) || (Character >= TEXT('a') && Character <= TEXT('f')) || (Character >= TEXT('A') && Character <= TEXT('F'));
            }
            if (bIsHex) { return Editor; }
        }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindHexEditor(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText) { return true; }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto FindButtonWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && ContainsText(InRoot, InText)) { return StaticCastSharedRef<SButton>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonWithText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindSamplePane(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkStyleLab_SamplePane>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkStyleLab_SamplePane")) { return StaticCastSharedRef<SCkStyleLab_SamplePane>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkStyleLab_SamplePane> Found = FindSamplePane(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWindow>& InWindow, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftButtonDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftButtonDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const bool Handled = InSlate.ProcessMouseButtonDownEvent(InWindow->GetNativeWindow(), Down);
        InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return Handled;
    }

    auto ReplaceText(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InEditor, const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InEditor, EFocusCause::SetDirectly);
        Tick(InSlate);
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (!InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})) { return false; }
        for (const TCHAR Character : InText)
        {
            if (!InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false})) { return false; }
        }
        Tick(InSlate);
        return true;
    }

    auto ReplaceAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InEditor, const FString& InText) -> bool
    {
        if (!ReplaceText(InSlate, InEditor, InText)) { return false; }
        InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        Tick(InSlate);
        return true;
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        return FImageUtils::SaveImageByExtension(*InPath, FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8));
    }

    struct FRestore final
    {
        explicit FRestore(UCk_InputHud_UserSettings* InSettings, UCk_InputHud_Settings* InProject)
            : Settings(InSettings), Project(InProject), Palette(Settings->Palette), Density(Settings->Density), MetadataMode(Settings->MetadataMode)
            , FrameNotation(Settings->FrameNotation), CornerStyle(Settings->CornerStyle), AnchorCorner(Settings->AnchorCorner)
            , AnchorOffsetX(Settings->AnchorOffsetX), AnchorOffsetY(Settings->AnchorOffsetY), KeyPaddingX(Settings->KeyPaddingX), KeyPaddingY(Settings->KeyPaddingY)
            , KeyCornerRadius(Settings->KeyCornerRadius), OverallOpacity(Settings->OverallOpacity), KeyBorderWidth(Settings->KeyBorderWidth), KeyBorderOpacity(Settings->KeyBorderOpacity)
            , ActiveFillOpacity(Settings->ActiveFillOpacity), ActiveGlowOpacity(Settings->ActiveGlowOpacity), PanelOpacity(Settings->PanelOpacity), PulseScale(Settings->PulseScale)
            , HistoryBrightness(Settings->HistoryBrightness), PressPopScale(Settings->PressPopScale), PressPopDurationMs(Settings->PressPopDurationMs), ReleaseEaseMs(Settings->ReleaseEaseMs)
            , UseCustomColors(Settings->UseCustomColors), CustomKeyBorder(Settings->CustomKeyBorder), CustomContainerOutline(Settings->CustomContainerOutline)
            , CustomActive(Settings->CustomActive), CustomResolved(Settings->CustomResolved), CustomUnrouted(Settings->CustomUnrouted), HoldBarMaxPx(InProject->HoldBarMaxPx) {}
        ~FRestore()
        {
            Settings->Palette = Palette;
            Settings->Density = Density;
            Settings->MetadataMode = MetadataMode;
            Settings->FrameNotation = FrameNotation;
            Settings->CornerStyle = CornerStyle;
            Settings->AnchorCorner = AnchorCorner;
            Settings->AnchorOffsetX = AnchorOffsetX;
            Settings->AnchorOffsetY = AnchorOffsetY;
            Settings->KeyPaddingX = KeyPaddingX;
            Settings->KeyPaddingY = KeyPaddingY;
            Settings->KeyCornerRadius = KeyCornerRadius;
            Settings->OverallOpacity = OverallOpacity;
            Settings->KeyBorderWidth = KeyBorderWidth;
            Settings->KeyBorderOpacity = KeyBorderOpacity;
            Settings->ActiveFillOpacity = ActiveFillOpacity;
            Settings->ActiveGlowOpacity = ActiveGlowOpacity;
            Settings->PanelOpacity = PanelOpacity;
            Settings->PulseScale = PulseScale;
            Settings->HistoryBrightness = HistoryBrightness;
            Settings->PressPopScale = PressPopScale;
            Settings->PressPopDurationMs = PressPopDurationMs;
            Settings->ReleaseEaseMs = ReleaseEaseMs;
            Settings->UseCustomColors = UseCustomColors;
            Settings->CustomKeyBorder = CustomKeyBorder;
            Settings->CustomContainerOutline = CustomContainerOutline;
            Settings->CustomActive = CustomActive;
            Settings->CustomResolved = CustomResolved;
            Settings->CustomUnrouted = CustomUnrouted;
            Project->HoldBarMaxPx = HoldBarMaxPx;
            Settings->SaveConfig();
            Project->SaveConfig();
            Settings->NotifyChanged();
        }
        UCk_InputHud_UserSettings* Settings;
        UCk_InputHud_Settings* Project;
        ECk_InputHud_Palette Palette;
        ECk_InputHud_Density Density;
        ECk_InputHud_MetadataMode MetadataMode;
        ECk_InputHud_FrameNotation FrameNotation;
        ECk_InputHud_CornerStyle CornerStyle;
        ECk_InputHud_AnchorCorner AnchorCorner;
        float AnchorOffsetX, AnchorOffsetY, KeyPaddingX, KeyPaddingY, KeyCornerRadius, OverallOpacity, KeyBorderWidth, KeyBorderOpacity;
        float ActiveFillOpacity, ActiveGlowOpacity, PanelOpacity, PulseScale, HistoryBrightness, PressPopScale, PressPopDurationMs, ReleaseEaseMs;
        bool UseCustomColors;
        FLinearColor CustomKeyBorder, CustomContainerOutline, CustomActive, CustomResolved, CustomUnrouted;
        float HoldBarMaxPx;
    };

    struct FNumberCase final { const TCHAR* Id; float UCk_InputHud_UserSettings::* Field; float Value; };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkStyleLab_InputHudAuthored,
    "Ck.UiAuthoring.StyleLab.InputHudAuthored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_InputHudAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_style_lab_input_hud_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Input HUD authored controls require Slate.")); return false; }
    UCk_InputHud_UserSettings* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    UCk_InputHud_Settings* Project = GetMutableDefault<UCk_InputHud_Settings>();
    if (!TestNotNull(TEXT("Input HUD user settings exist"), Settings) || !TestNotNull(TEXT("Input HUD project settings exist"), Project)) { return false; }
    const FRestore Restore(Settings, Project);

    int32 Notifications = 0;
    TSharedPtr<SCkStyleLab_ControlsPane> Pane = SNew(SCkStyleLab_ControlsPane)
        .OnSelectionChanged(FOnCkStyleLab_SelectionChanged::CreateLambda([&Notifications] { ++Notifications; }));
    const TSharedPtr<FCkUiView> View = Pane->Get_ControlsView();
    if (!TestTrue(TEXT("Input HUD authored controls load"), View.IsValid() && View->GetLastResult().Succeeded)) { return false; }
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const TSharedPtr<SCkDebug_InspectorPanel> Inspector = FindInspector(Region);
    if (!TestTrue(TEXT("Input HUD is mounted in its authored debug-inspector slot"), Inspector.IsValid() && FindTagged(Region,
        TEXT("input-hud-authored-controls")).IsValid())) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    const FVector2D PreviousCursor = Slate.GetCursorPos();
    const float OriginalScale = Slate.GetApplicationScale();
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SWindow> Window = SNew(SWindow).ClientSize(FVector2D{960.0f, 760.0f}).CreateTitleBar(false).HasCloseButton(false)
        [SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot()[Pane.ToSharedRef()]];
    ON_SCOPE_EXIT
    {
        if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        Slate.SetCursorPos(PreviousCursor);
        Slate.SetApplicationScale(OriginalScale);
    };
    Slate.AddWindow(Window.ToSharedRef(), true);
    Tick(Slate);
    Inspector->Set_Expanded(true);
    Tick(Slate);

    const auto GetCycleValue = [Settings](const FString& InCycle) -> int64
    {
        if (InCycle == TEXT("palette")) { return static_cast<int64>(Settings->Palette); }
        if (InCycle == TEXT("density")) { return static_cast<int64>(Settings->Density); }
        if (InCycle == TEXT("corners")) { return static_cast<int64>(Settings->CornerStyle); }
        if (InCycle == TEXT("anchor")) { return static_cast<int64>(Settings->AnchorCorner); }
        if (InCycle == TEXT("metadata")) { return static_cast<int64>(Settings->MetadataMode); }
        return static_cast<int64>(Settings->FrameNotation);
    };
    const TCHAR* Cycles[] = {TEXT("palette"), TEXT("density"), TEXT("corners"), TEXT("anchor"), TEXT("metadata"), TEXT("frame")};
    for (const TCHAR* Cycle : Cycles)
    {
        const FString Id = FString::Printf(TEXT("input-hud-%s-next"), Cycle);
        const TSharedPtr<SButton> Button = FindButton(Region, FName(*Id));
        const FString PreviousId = FString::Printf(TEXT("input-hud-%s-previous"), Cycle);
        if (!TestTrue(*FString::Printf(TEXT("%s authored cycle pair is mounted"), *Id), Button.IsValid() && FindButton(Region, FName(*PreviousId)).IsValid())) { return false; }
        Scroll->ScrollDescendantIntoView(Button.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        const uint32 RevisionBeforeCycle = Settings->Get_Revision();
        const int64 ValueBeforeCycle = GetCycleValue(Cycle);
        if (!TestTrue(*FString::Printf(TEXT("%s cycles through its native authored action"), *Id), Click(Slate, Window.ToSharedRef(), Button.ToSharedRef()))) { return false; }
        TestTrue(*FString::Printf(TEXT("%s writes its intended setting and revision"), *Id), Settings->Get_Revision() > RevisionBeforeCycle && GetCycleValue(Cycle) != ValueBeforeCycle);
    }
    TestEqual(TEXT("Each authored Input HUD cycle callback notifies its controls owner"), Notifications, static_cast<int32>(UE_ARRAY_COUNT(Cycles)));
    const auto PaletteAfterCycle = Settings->Palette;
    const auto DensityAfterCycle = Settings->Density;
    const auto MetadataAfterCycle = Settings->MetadataMode;
    const auto FrameAfterCycle = Settings->FrameNotation;

    const FNumberCase Numbers[] = {
        {TEXT("padding-x"), &UCk_InputHud_UserSettings::KeyPaddingX, 4.0f}, {TEXT("padding-y"), &UCk_InputHud_UserSettings::KeyPaddingY, 2.0f},
        {TEXT("corner-radius"), &UCk_InputHud_UserSettings::KeyCornerRadius, 4.0f}, {TEXT("overall-opacity"), &UCk_InputHud_UserSettings::OverallOpacity, 0.8f},
        {TEXT("anchor-x"), &UCk_InputHud_UserSettings::AnchorOffsetX, 12.0f}, {TEXT("anchor-y"), &UCk_InputHud_UserSettings::AnchorOffsetY, 10.0f},
        {TEXT("border-width"), &UCk_InputHud_UserSettings::KeyBorderWidth, 1.5f}, {TEXT("border-opacity"), &UCk_InputHud_UserSettings::KeyBorderOpacity, 0.6f},
        {TEXT("active-fill-opacity"), &UCk_InputHud_UserSettings::ActiveFillOpacity, 0.8f}, {TEXT("active-glow-opacity"), &UCk_InputHud_UserSettings::ActiveGlowOpacity, 0.2f},
        {TEXT("panel-opacity"), &UCk_InputHud_UserSettings::PanelOpacity, 0.8f}, {TEXT("pulse-scale"), &UCk_InputHud_UserSettings::PulseScale, 1.1f},
        {TEXT("history-brightness"), &UCk_InputHud_UserSettings::HistoryBrightness, 0.7f}, {TEXT("press-pop-scale"), &UCk_InputHud_UserSettings::PressPopScale, 1.1f},
        {TEXT("press-pop-ms"), &UCk_InputHud_UserSettings::PressPopDurationMs, 200.0f}, {TEXT("release-ease-ms"), &UCk_InputHud_UserSettings::ReleaseEaseMs, 280.0f}};
    for (const FNumberCase& Number : Numbers)
    {
        const FString Id = FString::Printf(TEXT("input-hud-%s-input"), Number.Id);
        const TSharedPtr<SEditableTextBox> Editor = FindEditor(Region, FName(*Id));
        if (!TestTrue(*FString::Printf(TEXT("%s native number editor is mounted"), *Id), Editor.IsValid())) { return false; }
        Scroll->ScrollDescendantIntoView(Editor.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
        Tick(Slate);
        const float Target = FMath::IsNearlyEqual(Settings->*(Number.Field), Number.Value) ? Number.Value * 0.5f : Number.Value;
        const uint32 BeforeRevision = Settings->Get_Revision();
        const int32 BeforeNotifications = Notifications;
        if (!TestTrue(*FString::Printf(TEXT("%s commits through focused native editor"), *Id), ReplaceAndCommit(Slate, Editor.ToSharedRef(), FString::SanitizeFloat(Target)))) { return false; }
        TestTrue(*FString::Printf(TEXT("%s updates the intended user setting"), *Id), FMath::IsNearlyEqual(Settings->*(Number.Field), Target) && Settings->Get_Revision() > BeforeRevision && Notifications == BeforeNotifications + 1);
    }
    const TSharedPtr<SEditableTextBox> HoldBar = FindEditor(Region, TEXT("input-hud-hold-bar-max-input"));
    const float HoldTarget = FMath::IsNearlyEqual(Project->HoldBarMaxPx, 24.0f) ? 32.0f : 24.0f;
    const uint32 HoldRevision = Settings->Get_Revision();
    const int32 HoldNotifications = Notifications;
    if (HoldBar.IsValid()) { Scroll->ScrollDescendantIntoView(HoldBar.ToSharedRef(), false, EDescendantScrollDestination::IntoView); Tick(Slate); }
    if (!TestTrue(TEXT("Project-owned hold-bar native editor mounts"), HoldBar.IsValid() && ReplaceAndCommit(Slate, HoldBar.ToSharedRef(), FString::SanitizeFloat(HoldTarget)))) { return false; }
    TestTrue(TEXT("Hold-bar commit updates project settings"), FMath::IsNearlyEqual(Project->HoldBarMaxPx, HoldTarget) && Settings->Get_Revision() > HoldRevision && Notifications == HoldNotifications + 1);

    const float PaddingBeforeInvalid = Settings->KeyPaddingX;
    const TSharedPtr<SEditableTextBox> Padding = FindEditor(Region, TEXT("input-hud-padding-x-input"));
    if (!TestTrue(TEXT("Representative invalid draft routes to the production editor"), Padding.IsValid() && ReplaceAndCommit(Slate, Padding.ToSharedRef(), TEXT("bad")))) { return false; }
    TestTrue(TEXT("Invalid user-setting draft is rejected without mutation"), FMath::IsNearlyEqual(Settings->KeyPaddingX, PaddingBeforeInvalid) && Padding->HasError());
    const float HoldBeforeInvalid = Project->HoldBarMaxPx;
    if (!TestTrue(TEXT("Representative invalid project draft routes to the production editor"), ReplaceAndCommit(Slate, HoldBar.ToSharedRef(), TEXT("bad")))) { return false; }
    TestTrue(TEXT("Invalid project-setting draft is rejected without mutation"), FMath::IsNearlyEqual(Project->HoldBarMaxPx, HoldBeforeInvalid) && HoldBar->HasError());
    const uint32 NoOpRevision = Settings->Get_Revision();
    if (!TestTrue(TEXT("Representative no-op user value routes through native Enter"), ReplaceAndCommit(Slate, Padding.ToSharedRef(), FString::SanitizeFloat(PaddingBeforeInvalid)))) { return false; }
    TestEqual(TEXT("No-op user commit does not republish settings"), Settings->Get_Revision(), NoOpRevision);
    if (!TestTrue(TEXT("Representative no-op project value routes through native Enter"), ReplaceAndCommit(Slate, HoldBar.ToSharedRef(), FString::SanitizeFloat(HoldBeforeInvalid)))) { return false; }
    TestEqual(TEXT("No-op project commit does not republish settings"), Settings->Get_Revision(), NoOpRevision);

    const TCHAR* Colors[] = {TEXT("container-outline"), TEXT("key-outline"), TEXT("active"), TEXT("resolved"), TEXT("unrouted")};
    for (const TCHAR* Color : Colors)
    {
        const FString Id = FString::Printf(TEXT("input-hud-%s-color"), Color);
        const TSharedPtr<SButton> Swatch = FindButton(Region, FName(*Id));
        TestTrue(*FString::Printf(TEXT("%s owned color-picker swatch is mounted"), *Id), Swatch.IsValid());
    }
    const TSharedPtr<SButton> ContainerOutline = FindButton(Region, TEXT("input-hud-container-outline-color"));
    if (ContainerOutline.IsValid()) { Scroll->ScrollDescendantIntoView(ContainerOutline.ToSharedRef(), false, EDescendantScrollDestination::IntoView); Tick(Slate); }
    if (!TestTrue(TEXT("Container-outline swatch opens its owned popup"), ContainerOutline.IsValid() && Click(Slate, Window.ToSharedRef(), ContainerOutline.ToSharedRef()))) { return false; }
    {
        TArray<TSharedRef<SWindow>> VisibleWindows;
        Slate.GetAllVisibleWindowsOrdered(VisibleWindows);
        TSharedPtr<SColorPicker> Picker;
        TSharedPtr<SWindow> PickerWindow;
        for (const TSharedRef<SWindow>& TopLevel : VisibleWindows)
        {
            Picker = FindColorPicker(TopLevel);
            if (Picker.IsValid()) { PickerWindow = TopLevel; break; }
        }
        VisibleWindows.Reset();
        if (!TestTrue(TEXT("Owned Input HUD color popup is discoverable"), Picker.IsValid() && PickerWindow.IsValid())) { return false; }
        const TSharedPtr<SEditableTextBox> HexEditor = FindHexEditor(Picker.ToSharedRef());
        const TSharedPtr<SButton> OkButton = FindButtonWithText(Picker.ToSharedRef(), TEXT("OK"));
        if (!TestTrue(TEXT("Owned Input HUD color popup exposes its native hexadecimal editor and OK button"), HexEditor.IsValid() && OkButton.IsValid())) { return false; }
        if (!TestTrue(TEXT("Native color picker settles its construction timer before hexadecimal editing"), SettleNativePicker(Slate, Picker.ToSharedRef()))) { return false; }
        Slate.SetUserFocus(0, HexEditor.ToSharedRef(), EFocusCause::SetDirectly);
        Tick(Slate);
        FWidgetPath HexEditorFocusPath;
        const TSharedPtr<SWidget> HexEditorFocus = Slate.GetUserFocusedWidget(0);
        const bool bHexEditorFocusPathValid = HexEditorFocus.IsValid() && Slate.GeneratePathToWidgetUnchecked(HexEditorFocus.ToSharedRef(), HexEditorFocusPath);
        bool bPickerOwnsHexEditor = false;
        bool bHexEditorIsFocused = false;
        for (int32 Index = 0; Index < HexEditorFocusPath.Widgets.Num(); ++Index)
        {
            bPickerOwnsHexEditor |= HexEditorFocusPath.Widgets[Index].Widget == Picker;
            bHexEditorIsFocused |= HexEditorFocusPath.Widgets[Index].Widget == HexEditor;
        }
        if (!TestTrue(TEXT("Focused native hexadecimal editor belongs to the owned picker"), bHexEditorFocusPathValid && bPickerOwnsHexEditor && bHexEditorIsFocused)) { return false; }
        if (!TestTrue(TEXT("Native hexadecimal editor changes the color-picker draft"), ReplaceText(Slate, HexEditor.ToSharedRef(), TEXT("FF0000FF")))) { return false; }
        HexEditor->SelectAllText();
        if (!TestEqual(TEXT("Native hexadecimal edit buffer is exact before Enter"), HexEditor->GetSelectedText().ToString(), FString(TEXT("FF0000FF")))) { return false; }
        if (!TestTrue(TEXT("Native hexadecimal editor routes Enter"), Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}))) { return false; }
        Tick(Slate);
        if (!TestEqual(TEXT("Native hexadecimal draft survives Enter"), HexEditor->GetText().ToString(), FString(TEXT("FF0000FF")))) { return false; }
        bool bPickerRetainedAfterEnter = false;
        {
            TArray<TSharedRef<SWindow>> WindowsAfterEnter;
            Slate.GetAllVisibleWindowsOrdered(WindowsAfterEnter);
            for (const TSharedRef<SWindow>& TopLevel : WindowsAfterEnter)
            {
                bPickerRetainedAfterEnter |= &TopLevel.Get() == PickerWindow.Get() && FindColorPicker(TopLevel) == Picker;
            }
        }
        if (!TestTrue(TEXT("Native owned picker remains mounted after hexadecimal Enter"), bPickerRetainedAfterEnter)) { return false; }
        if (!TestTrue(TEXT("Native picker OK button commits the intended semantic color"), Click(Slate, PickerWindow.ToSharedRef(), OkButton.ToSharedRef()))) { return false; }
        Tick(Slate);
        TestTrue(TEXT("Native owned color popup commits the intended semantic setting"), Settings->UseCustomColors
            && UCk_InputHud_UserSettings::Get_PaletteSnapshot().ContainerOutline.Equals(FLinearColor::Red));
        Slate.DismissAllMenus();
    }

    const TSharedPtr<SButton> Reset = FindButton(Region, TEXT("input-hud-reset-visuals"));
    if (Reset.IsValid()) { Scroll->ScrollDescendantIntoView(Reset.ToSharedRef(), false, EDescendantScrollDestination::IntoView); Tick(Slate); }
    if (!TestTrue(TEXT("Authored visual reset is mounted and reachable"), Reset.IsValid() && Click(Slate, Window.ToSharedRef(), Reset.ToSharedRef()))) { return false; }
    TestTrue(TEXT("Visual reset restores visual defaults while preserving readout and project settings"), Settings->Palette == ECk_InputHud_Palette::ArcticSignal
        && Settings->Density == ECk_InputHud_Density::Compact && Settings->MetadataMode == MetadataAfterCycle && Settings->FrameNotation == FrameAfterCycle
        && FMath::IsNearlyEqual(Project->HoldBarMaxPx, HoldBeforeInvalid));

    const TSharedPtr<SEditableTextBox> Draft = FindEditor(Region, TEXT("input-hud-padding-y-input"));
    if (!TestTrue(TEXT("Draft editor is available for compatible reload"), Draft.IsValid())) { return false; }
    Slate.SetUserFocus(0, Draft.ToSharedRef(), EFocusCause::SetDirectly);
    Tick(Slate);
    Slate.ProcessKeyCharEvent(FCharacterEvent{TEXT('5'), FModifierKeysState{}, 0, false});
    const FString DraftText = Draft->GetText().ToString();
    const TSharedPtr<SWidget> Focus = Slate.GetUserFocusedWidget(0);
    const int64 Revision = View->GetRevision();
    const TSharedPtr<SWidget> PreviewPort = FindTagged(Region, TEXT("preview-input-hud"));
    const TSharedPtr<SCkStyleLab_SamplePane> Preview = PreviewPort.IsValid() ? FindSamplePane(PreviewPort.ToSharedRef()) : nullptr;
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Installed Style Lab resources resolve for reload"), Plugin.IsValid())) { return false; }
    const FString Resource = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    if (!TestTrue(TEXT("Compatible authored reload preserves Input HUD draft and preview leaf"), View->ReloadFiles(FPaths::Combine(Resource, TEXT("StyleLabControls.ui.html")), FPaths::Combine(Resource, TEXT("StyleLabControls.ui.css"))).Succeeded)) { return false; }
    Tick(Slate);
    const TSharedPtr<SWidget> ReloadedPreviewPort = FindTagged(Region, TEXT("preview-input-hud"));
    TestTrue(TEXT("Compatible reload advances revision while retaining focused numeric editor and preview"), View->GetRevision() == Revision + 1
        && FindEditor(Region, TEXT("input-hud-padding-y-input")) == Draft && Slate.GetUserFocusedWidget(0) == Focus && Draft->GetText().ToString() == DraftText
        && ReloadedPreviewPort.IsValid() && FindSamplePane(ReloadedPreviewPort.ToSharedRef()) == Preview);

    FString Markup;
    FString Css;
    const bool bResourcesLoaded =
        FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Resource, TEXT("StyleLabControls.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Resource, TEXT("StyleLabControls.ui.css")));
    if (!TestTrue(TEXT("Installed authored resources load for atomic rejection"), bResourcesLoaded)) { return false; }
    TestTrue(TEXT("Rejected fixture corrupts a required Input HUD number binding"), Markup.ReplaceInline(TEXT("value-bind=\"input-hud-padding-x\""), TEXT("value-bind=\"missing-padding\"")) == 1);
    const int64 AcceptedRevision = View->GetRevision();
    TestFalse(TEXT("Rejected Input HUD reload is atomic"), View->TryReload(Markup, Css).Succeeded);
    const TSharedPtr<SWidget> RejectedPreviewPort = FindTagged(Region, TEXT("preview-input-hud"));
    TestTrue(TEXT("Rejected reload retains accepted draft editor preview and revision"), View->GetRevision() == AcceptedRevision
        && FindEditor(Region, TEXT("input-hud-padding-y-input")) == Draft && Draft->GetText().ToString() == DraftText
        && RejectedPreviewPort.IsValid() && FindSamplePane(RejectedPreviewPort.ToSharedRef()) == Preview);

    Window->Resize(FVector2D{420.0f, 520.0f});
    Tick(Slate);
    Scroll->ScrollDescendantIntoView(HoldBar.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const FGeometry ScrollGeometry = Scroll->GetCachedGeometry();
    const FVector2D HoldCenter = ScrollGeometry.AbsoluteToLocal(HoldBar->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D{0.5f, 0.5f}));
    TestTrue(TEXT("Narrow scroll host reaches the seventeenth Input HUD numeric row"), HoldBar->GetCachedGeometry().GetLocalSize().X > 4.0f
        && HoldCenter.X >= 0.0f && HoldCenter.Y >= 0.0f && HoldCenter.X <= ScrollGeometry.GetLocalSize().X && HoldCenter.Y <= ScrollGeometry.GetLocalSize().Y);
    const FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StyleLabInputHud"));
    IFileManager::Get().MakeDirectory(*Output, true);
    TestTrue(TEXT("Narrow Input HUD viewport capture writes"), Capture(Slate, Scroll.ToSharedRef(), FPaths::Combine(Output, TEXT("InputHud-Narrow.png"))));
    Window->Resize(FVector2D{1120.0f, 860.0f});
    Tick(Slate);
    TestTrue(TEXT("Wide Input HUD viewport capture writes"), Capture(Slate, Scroll.ToSharedRef(), FPaths::Combine(Output, TEXT("InputHud-Wide.png"))));
    Slate.SetApplicationScale(1.5f);
    Tick(Slate);
    Scroll->ScrollDescendantIntoView(HoldBar.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Tick(Slate);
    const FGeometry ScaledViewport = Scroll->GetCachedGeometry();
    const FGeometry ScaledEditor = HoldBar->GetCachedGeometry();
    const FVector2D ScaledCenter = ScaledViewport.AbsoluteToLocal(ScaledEditor.GetAbsolutePositionAtCoordinates(FVector2D{0.5f, 0.5f}));
    TestTrue(TEXT("150 percent scale keeps the final Input HUD numeric row reachable"), ScaledEditor.GetLocalSize().X > 4.0f && ScaledEditor.GetLocalSize().Y > 4.0f
        && ScaledCenter.X >= 0.0f && ScaledCenter.Y >= 0.0f && ScaledCenter.X <= ScaledViewport.GetLocalSize().X && ScaledCenter.Y <= ScaledViewport.GetLocalSize().Y);
    TestTrue(TEXT("Scaled Input HUD viewport capture writes"), Capture(Slate, Scroll.ToSharedRef(), FPaths::Combine(Output, TEXT("InputHud-150.png"))));

    const TWeakPtr<SCkStyleLab_ControlsPane> WeakPane = Pane;
    const float PaddingBeforeRelease = Settings->KeyPaddingX;
    Slate.DestroyWindowImmediately(Window.ToSharedRef());
    Window.Reset();
    Scroll.Reset();
    Pane.Reset();
    TestTrue(TEXT("Input HUD controls owner releases"), !WeakPane.IsValid());
    ReplaceAndCommit(Slate, Padding.ToSharedRef(), TEXT("6"));
    TestTrue(TEXT("Held native Input HUD editor is inert after owner release"), FMath::IsNearlyEqual(Settings->KeyPaddingX, PaddingBeforeRelease));
    return true;
}

#endif
