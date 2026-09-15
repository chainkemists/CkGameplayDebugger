#include "CkAudioDebugger/Window/SCkAudioDebugger_Radar.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_radar_visual_test
{
    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto Capture(
        FSlateApplication& InSlate,
        const TSharedRef<SWidget>& InRoot,
        const FString& InPath,
        FIntVector& OutSize,
        int32& OutUniqueColors) -> bool
    {
        auto Pixels = TArray<FColor>{};
        OutSize = FIntVector::ZeroValue;
        if (NOT InSlate.TakeScreenshot(InRoot, Pixels, OutSize)
            || OutSize.X <= 0 || OutSize.Y <= 0 || Pixels.Num() < OutSize.X * OutSize.Y)
        { return false; }

        auto Colors = TSet<FColor>{};
        for (const FColor& Pixel : Pixels)
        { Colors.Add(Pixel); }
        OutUniqueColors = Colors.Num();

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), true);
        return FImageUtils::SaveImageByExtension(
            *InPath, FImageView(Pixels.GetData(), OutSize.X, OutSize.Y, ERawImageFormat::BGRA8));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_RadarVisual,
    "Ck.AudioDebugger.Radar.Visual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_RadarVisual::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_radar_visual_test;
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Audio Radar visual gate requires Slate."));
        return false;
    }

    const auto View = MakeShared<FCkAudioDebugger_SpatialView>();
    View->HasSelection = true;
    View->HasSpatialData = true;
    View->TrackName = TEXT("Rain Roof Loop");
    View->ListenerSource = TEXT("Audio device listener 0");
    View->DistanceCm = 640.0f;
    View->InnerRadiusCm = 350.0f;
    View->FalloffCm = 900.0f;
    View->MaxFalloffCm = 1250.0f;
    View->BearingDegrees = 35.0f;
    View->IsAttenuated = true;
    View->IsSpatialized = true;
    View->RadarRangeCm = 2000.0f;
    View->Blips = {
        FCkAudioDebugger_SpatialBlip{TEXT("Rain Roof Loop"), 35.0f, 640.0f, true, false, false, true},
        FCkAudioDebugger_SpatialBlip{TEXT("Virtualized Ambience"), -72.0f, 1180.0f, false, true, false, false},
        FCkAudioDebugger_SpatialBlip{TEXT("Out-of-range Beacon"), 142.0f, 2600.0f, false, false, true, false}};

    TSharedPtr<SCkAudioDebugger_Radar> Radar;
    TSharedPtr<SWindow> HostWindow;
    auto& Slate = FSlateApplication::Get();
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid())
        { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    HostWindow = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{320.0f, 320.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [
            SNew(SBorder)
            .Padding(16.0f)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(CkStyle::Bg2())
            [
                SNew(SBox)
                .WidthOverride(288.0f)
                .HeightOverride(288.0f)
                [
                    SAssignNew(Radar, SCkAudioDebugger_Radar)
                    .View(View)
                ]
            ]
        ];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    auto Size = FIntVector::ZeroValue;
    auto UniqueColors = int32{0};
    const FString CapturePath = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Automation/AudioDebuggerRadar-Visual.png"));
    TestTrue(TEXT("production Audio Radar writes its real-RHI visual artifact"),
        Capture(Slate, Radar.ToSharedRef(), CapturePath, Size, UniqueColors));
    TestTrue(TEXT("Radar artifact has meaningful geometry and painted color variation"),
        Size.X >= 240 && Size.Y >= 240 && UniqueColors >= 8);
    AddInfo(FString::Printf(TEXT("Audio Radar capture: %s (%dx%d, %d unique colors)"),
        *CapturePath, Size.X, Size.Y, UniqueColors));
    return true;
}

#endif
