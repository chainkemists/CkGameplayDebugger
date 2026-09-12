#include "CkStyleLabDebugger/Window/SCkStyleLabWindow.h"

#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_style_lab_window_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto TickUntil(FSlateApplication& InSlate, TFunctionRef<bool()> InPredicate) -> bool
    {
        const double Deadline = FPlatformTime::Seconds() + 1.0;
        do
        {
            Tick(InSlate);
            if (InPredicate()) { return true; }
            FPlatformProcess::SleepNoStats(0.005f);
        }
        while (FPlatformTime::Seconds() < Deadline);
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWindow>& InWindow, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        InSlate.SetCursorPos(Position);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftButtonDown{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::Invalid, 0, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, LeftButtonDown, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, NoButtons, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        InSlate.ProcessMouseMoveEvent(Move, true);
        const bool bHandled = InSlate.ProcessMouseButtonDownEvent(InWindow->GetNativeWindow(), Down);
        InSlate.ProcessMouseButtonUpEvent(Up);
        Tick(InSlate);
        return bHandled;
    }

    auto FindChrome(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkDebug_WindowChrome>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_WindowChrome")) { return StaticCastSharedRef<SCkDebug_WindowChrome>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkDebug_WindowChrome> Found = FindChrome(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindScroll(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SScrollBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SScrollBox")) { return StaticCastSharedRef<SScrollBox>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SScrollBox> Found = FindScroll(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindAllTonesCheckBox(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCheckBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCheckBox")
            && InRoot->GetAccessibleText().ToString() == TEXT("All Tones"))
        { return StaticCastSharedRef<SCheckBox>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCheckBox> Found = FindAllTonesCheckBox(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindSamplePane(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkStyleLab_SamplePane>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkStyleLab_SamplePane")) { return StaticCastSharedRef<SCkStyleLab_SamplePane>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkStyleLab_SamplePane> Found = FindSamplePane(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto Capture(FSlateApplication& InSlate, const TSharedRef<SWidget>& InRoot, const FString& InPath) -> bool
    {
        TArray<FColor> Pixels;
        FIntVector Size = FIntVector::ZeroValue;
        if (!InSlate.TakeScreenshot(InRoot, Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y) { return false; }
        const FImageView Image(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8);
        return FImageUtils::SaveImageByExtension(*InPath, Image);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkStyleLab_WindowAuthored,
    "Ck.UiAuthoring.StyleLab.Window",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_WindowAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_style_lab_window_authored_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Style Lab window test requires Slate.")); return false; }
    UCkDebuggerStyleSettings* Settings = UCkDebuggerStyleSettings::Get_Mutable();
    if (!TestNotNull(TEXT("Style settings exist"), Settings)) { return false; }
    const FCkDebuggerStyleSelection OriginalSelection = Settings->Selection;
    const FString OriginalName = Settings->ActiveProfileName;
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        Settings->Selection = OriginalSelection;
        Settings->ActiveProfileName = OriginalName;
        Settings->NotifyChanged();
        if (HostWindow.IsValid()) { FSlateApplication::Get().DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    auto& Slate = FSlateApplication::Get();
    const TSharedRef<SCkStyleLabWindow> StyleLabWindow = SNew(SCkStyleLabWindow);
    HostWindow = SNew(SWindow).ClientSize(FVector2D(1200, 820)).CreateTitleBar(false)[StyleLabWindow];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    Tick(Slate);

    const TSharedPtr<SCkDebug_WindowChrome> Chrome = FindChrome(StyleLabWindow);
    const TSharedPtr<SScrollBox> Scroll = FindScroll(StyleLabWindow);
    const TSharedPtr<SCheckBox> AllTonesCheckBox = FindAllTonesCheckBox(StyleLabWindow);
    const TSharedPtr<SCkStyleLab_SamplePane> Sample = FindSamplePane(StyleLabWindow);
    if (!TestTrue(TEXT("Production Style Lab mounts its shared window chrome"), Chrome.IsValid())
        || !TestTrue(TEXT("Production Style Lab mounts its scrollable content"), Scroll.IsValid())
        || !TestTrue(TEXT("Production chrome mounts the All Tones action"), AllTonesCheckBox.IsValid())
        || !TestTrue(TEXT("Production content mounts a Style Lab sample"), Sample.IsValid())) { return false; }

    TestFalse(TEXT("All Tones starts disabled in the mounted window"), Sample->Get_ShowAllTones());
    TestFalse(TEXT("All Tones checkbox starts unchecked"), AllTonesCheckBox->IsChecked());
    TestTrue(TEXT("All Tones receives a routed physical mouse click"), Click(Slate, HostWindow.ToSharedRef(), AllTonesCheckBox.ToSharedRef()));
    TestTrue(TEXT("Routed All Tones click updates the production sample"), Sample->Get_ShowAllTones());
    TestTrue(TEXT("Routed All Tones click updates its physical checkbox"), AllTonesCheckBox->IsChecked());

    const FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StyleLabWindow"));
    IFileManager::Get().MakeDirectory(*Output, true);
    TestTrue(TEXT("Wide full Style Lab window capture writes"), Capture(Slate, StyleLabWindow, FPaths::Combine(Output, TEXT("Window-Wide.png"))));
    HostWindow->Resize(FVector2D(426, 640));
    Tick(Slate);
    TestTrue(TEXT("Narrow full Style Lab window capture writes"), Capture(Slate, StyleLabWindow, FPaths::Combine(Output, TEXT("Window-Narrow.png"))));

    TSharedPtr<SWidget> SampleRoot;
    TSharedPtr<SWidget> BeforeRefreshBody;
    if (Sample->GetChildren()->Num() > 0)
    {
        SampleRoot = ConstCastSharedRef<SWidget>(Sample->GetChildren()->GetChildAt(0));
        if (SampleRoot->GetChildren()->Num() > 0)
        { BeforeRefreshBody = ConstCastSharedRef<SWidget>(SampleRoot->GetChildren()->GetChildAt(0)); }
    }
    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();
    if (!TestTrue(TEXT("Style Lab has an alternate real profile for external revision refresh"), Profiles.Num() >= 2)
        || !TestTrue(TEXT("Style Lab sample exposes a retained root and initial body"), SampleRoot.IsValid() && BeforeRefreshBody.IsValid())) { return false; }
    const auto Alternate = Profiles[0].Name == Settings->ActiveProfileName ? Profiles[1] : Profiles[0];
    Settings->Selection = Alternate.Selection;
    Settings->ActiveProfileName = Alternate.Name;
    const uint32 RevisionBefore = Settings->Get_Revision();
    Settings->NotifyChanged();
    TestTrue(TEXT("External style mutation advances the shared revision"), Settings->Get_Revision() > RevisionBefore);
    TestTrue(TEXT("Mounted Style Lab window retains its sample root and rebuilds its body after external style revision"), TickUntil(Slate, [&Sample, &SampleRoot, &BeforeRefreshBody]()
    {
        return Sample->GetChildren()->Num() > 0
            && &Sample->GetChildren()->GetChildAt(0).Get() == SampleRoot.Get()
            && SampleRoot->GetChildren()->Num() > 0
            && &SampleRoot->GetChildren()->GetChildAt(0).Get() != BeforeRefreshBody.Get();
    }));
    return true;
}

#endif
