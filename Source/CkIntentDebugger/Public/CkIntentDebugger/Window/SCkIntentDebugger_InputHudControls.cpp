#include "CkIntentDebugger/Window/SCkIntentDebugger_InputHudControls.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "HAL/IConsoleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_intent_debugger_input_hud_controls
{
    constexpr auto LabelWidth = 124.0f;

    auto
        Make_Row(
            const TCHAR*         InLabel,
            const TCHAR*         InTooltip,
            TSharedRef<SWidget> InValue)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            .ToolTipText(FText::FromString(InTooltip))

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .WidthOverride(LabelWidth)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(InLabel))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
            ]

            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                InValue
            ];
    }

    auto
        Make_SectionLabel(
            const TCHAR* InLabel)
        -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InLabel))
            .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
            .ColorAndOpacity(FSlateColor{CkStyle::Accent()});
    }

    auto
        Get_CVarFloat(
            const TCHAR* InName,
            float        InFallback)
        -> float
    {
        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr ? CVar->GetFloat() : InFallback;
    }

    auto
        Get_CVarInt(
            const TCHAR* InName,
            int32        InFallback)
        -> int32
    {
        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr ? CVar->GetInt() : InFallback;
    }

    auto
        Set_CVarFloat(
            const TCHAR* InName,
            float        InValue)
        -> void
    {
        if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        { CVar->Set(InValue, ECVF_SetByConsole); }
    }

    auto
        Set_CVarInt(
            const TCHAR* InName,
            int32        InValue)
        -> void
    {
        if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        { CVar->Set(InValue, ECVF_SetByConsole); }
    }

    auto Save_ProjectSettings() -> void
    {
        GetMutableDefault<UCk_InputHud_Settings>()->SaveConfig();
        UCk_InputHud_UserSettings::Get_Mutable()->NotifyChanged();
    }
}

// ====================================================================================================================

auto
    SCkIntentDebugger_InputHudControls::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SBox)
            .WidthOverride(480.0f)
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush_Large())
                    .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
                    .Padding(FMargin{CkStyle::SpaceM})
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()
                        [
                            ck_intent_debugger_input_hud_controls::Make_SectionLabel(TEXT("READOUT"))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                        [
                            Build_ReadoutControls()
                        ]

                        + SVerticalBox::Slot().AutoHeight()
                        [
                            ck_intent_debugger_input_hud_controls::Make_SectionLabel(TEXT("SESSION"))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                        [
                            Build_SessionControls()
                        ]

                        + SVerticalBox::Slot().AutoHeight()
                        [
                            ck_intent_debugger_input_hud_controls::Make_SectionLabel(TEXT("PROJECT BEHAVIOR"))
                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            Build_ProjectControls()
                        ]
                    ]
            ]
    ];
}

auto
    SCkIntentDebugger_InputHudControls::
    Build_ReadoutControls()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            ck_intent_debugger_input_hud_controls::Make_Row(
                TEXT("Metadata"),
                TEXT("Keys = state only; Compact = duration + route; Full = frames, sticks and all routing detail."),
                SNew(SSegmentedControl<ECk_InputHud_MetadataMode>)
                    .Value_Lambda([]() { return UCk_InputHud_UserSettings::Get_MetadataMode(); })
                    .OnValueChanged_Lambda([](ECk_InputHud_MetadataMode InValue)
                    { UCk_InputHud_UserSettings::Get_Mutable()->Set_MetadataMode(InValue); })
                    + SSegmentedControl<ECk_InputHud_MetadataMode>::Slot(ECk_InputHud_MetadataMode::Keys).Text(FText::FromString(TEXT("Keys")))
                    + SSegmentedControl<ECk_InputHud_MetadataMode>::Slot(ECk_InputHud_MetadataMode::Compact).Text(FText::FromString(TEXT("Compact")))
                    + SSegmentedControl<ECk_InputHud_MetadataMode>::Slot(ECk_InputHud_MetadataMode::Full).Text(FText::FromString(TEXT("Full"))))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            ck_intent_debugger_input_hud_controls::Make_Row(
                TEXT("Frame notation"),
                TEXT("Press frame, compact start+delta, or full start-end range when Full metadata is active."),
                SNew(SSegmentedControl<ECk_InputHud_FrameNotation>)
                    .Value_Lambda([]() { return UCk_InputHud_UserSettings::Get_FrameNotation(); })
                    .OnValueChanged_Lambda([](ECk_InputHud_FrameNotation InValue)
                    { UCk_InputHud_UserSettings::Get_Mutable()->Set_FrameNotation(InValue); })
                    + SSegmentedControl<ECk_InputHud_FrameNotation>::Slot(ECk_InputHud_FrameNotation::Press).Text(FText::FromString(TEXT("Press")))
                    + SSegmentedControl<ECk_InputHud_FrameNotation>::Slot(ECk_InputHud_FrameNotation::Delta).Text(FText::FromString(TEXT("Delta")))
                    + SSegmentedControl<ECk_InputHud_FrameNotation>::Slot(ECk_InputHud_FrameNotation::Range).Text(FText::FromString(TEXT("Range"))))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Reset readout")))
                .ToolTipText(FText::FromString(TEXT("Restore Compact metadata and Press frame notation.")))
                .OnClicked(FOnClicked::CreateSP(this, &SCkIntentDebugger_InputHudControls::OnResetReadout))
        ];
}

auto
    SCkIntentDebugger_InputHudControls::
    Build_SessionControls()
    -> TSharedRef<SWidget>
{
    const auto MakeNumeric = [](const TCHAR* InLabel, const TCHAR* InTooltip, TAttribute<double> InValue,
        TOptional<double> InMin, TOptional<double> InMax, int32 InDigits,
        FOnCkDebug_NumericCommitted InCommitted) -> TSharedRef<SWidget>
    {
        return ck_intent_debugger_input_hud_controls::Make_Row(
            InLabel, InTooltip,
            SNew(SCkDebug_NumericEditor)
                .Value(InValue)
                .MinValue(InMin)
                .MaxValue(InMax)
                .FractionalDigits(InDigits)
                .Width(80.0f)
                .OnValueCommitted(InCommitted));
    };

    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            ck_intent_debugger_input_hud_controls::Make_Row(
                TEXT("Mode"), TEXT("Keyboard-only (1) or automatic keyboard/gamepad readout (2). Off remains the toolbar toggle."),
                SNew(SSegmentedControl<int32>)
                    .Value_Lambda([]() { return FMath::Clamp(ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay"), 2), 1, 2); })
                    .OnValueChanged_Lambda([](int32 InValue) { ck_intent_debugger_input_hud_controls::Set_CVarInt(TEXT("ck.InputOverlay"), InValue); })
                    + SSegmentedControl<int32>::Slot(1).Text(FText::FromString(TEXT("Keyboard")))
                    + SSegmentedControl<int32>::Slot(2).Text(FText::FromString(TEXT("Auto"))))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Scale"), TEXT("Unbounded whole-overlay render multiplier for the current QA session; zero hides it."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.Scale"), 1.0f)); }),
            TOptional<double>{0.0}, TOptional<double>{}, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.Scale"), static_cast<float>(InValue)); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Opacity"), TEXT("Whole-overlay opacity for the current QA session."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.Opacity"), 1.0f)); }),
            TOptional<double>{0.15}, TOptional<double>{1.0}, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.Opacity"), static_cast<float>(InValue)); }))]
        + SVerticalBox::Slot().AutoHeight()
        [
            ck_intent_debugger_input_hud_controls::Make_Row(
                TEXT("Corner"), TEXT("Viewport anchor: top-left, top-right, bottom-left, bottom-right. PERSISTS across runs, including packaged builds -- unlike Scale and Opacity, which are session-only."),
                SNew(SSegmentedControl<int32>)
                    .Value_Lambda([]() { return FMath::Clamp(ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay.Corner"), 1), 0, 3); })
                    .OnValueChanged_Lambda([](int32 InValue) { ck_intent_debugger_input_hud_controls::Set_CVarInt(TEXT("ck.InputOverlay.Corner"), InValue); })
                    + SSegmentedControl<int32>::Slot(0).Text(FText::FromString(TEXT("TL")))
                    + SSegmentedControl<int32>::Slot(1).Text(FText::FromString(TEXT("TR")))
                    + SSegmentedControl<int32>::Slot(2).Text(FText::FromString(TEXT("BL")))
                    + SSegmentedControl<int32>::Slot(3).Text(FText::FromString(TEXT("BR"))))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [MakeNumeric(TEXT("Offset X"), TEXT("Distance inward from the anchored corner. Always positive - it pushes the overlay away from whichever corner it is pinned to, so changing corners mirrors the placement. Persists."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.OffsetX"), 0.0f)); }),
            TOptional<double>{0.0}, TOptional<double>{512.0}, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.OffsetX"), static_cast<float>(InValue)); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [MakeNumeric(TEXT("Offset Y"), TEXT("Distance inward from the anchored corner, vertically. See Offset X. Persists."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.OffsetY"), 0.0f)); }),
            TOptional<double>{0.0}, TOptional<double>{512.0}, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.OffsetY"), static_cast<float>(InValue)); }))];
}

auto
    SCkIntentDebugger_InputHudControls::
    Build_ProjectControls()
    -> TSharedRef<SWidget>
{
    const auto MakeNumeric = [](const TCHAR* InLabel, const TCHAR* InTooltip, TAttribute<double> InValue,
        ECkDebug_NumericKind InKind, double InMin, double InMax, int32 InDigits,
        FOnCkDebug_NumericCommitted InCommitted) -> TSharedRef<SWidget>
    {
        return ck_intent_debugger_input_hud_controls::Make_Row(
            InLabel, InTooltip,
            SNew(SCkDebug_NumericEditor)
                .Value(InValue)
                .Kind(InKind)
                .MinValue(InMin)
                .MaxValue(InMax)
                .FractionalDigits(InDigits)
                .Width(80.0f)
                .OnValueCommitted(InCommitted));
    };

    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("History cap"), TEXT("Shared project default for released events retained by the Signal Strip."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_Settings::Get_HistoryCap()); }),
            ECkDebug_NumericKind::Integer, 3.0, 20.0, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { GetMutableDefault<UCk_InputHud_Settings>()->HistoryCap = static_cast<int32>(InValue); ck_intent_debugger_input_hud_controls::Save_ProjectSettings(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Fade seconds"), TEXT("Shared project lifetime for released events before pruning."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_Settings::Get_FadeLifetimeSeconds()); }),
            ECkDebug_NumericKind::Float, 3.0, 30.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { GetMutableDefault<UCk_InputHud_Settings>()->FadeLifetimeSeconds = static_cast<float>(InValue); ck_intent_debugger_input_hud_controls::Save_ProjectSettings(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Tap / hold ms"), TEXT("Shared project threshold where a press becomes a growing hold bar."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_Settings::Get_TapHoldThresholdMs()); }),
            ECkDebug_NumericKind::Float, 50.0, 2000.0, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([](double InValue)
            { GetMutableDefault<UCk_InputHud_Settings>()->TapHoldThresholdMs = static_cast<float>(InValue); ck_intent_debugger_input_hud_controls::Save_ProjectSettings(); }))]
        + SVerticalBox::Slot().AutoHeight()
        [
            ck_intent_debugger_input_hud_controls::Make_Row(
                TEXT("Frames allowed"), TEXT("Shared project master switch. Full metadata cannot show frames while this is Off."),
                SNew(SSegmentedControl<int32>)
                    .Value_Lambda([]() { return UCk_InputHud_Settings::Get_ShowFrameNumbers() ? 1 : 0; })
                    .OnValueChanged_Lambda([](int32 InValue)
                    { GetMutableDefault<UCk_InputHud_Settings>()->ShowFrameNumbers = InValue != 0; ck_intent_debugger_input_hud_controls::Save_ProjectSettings(); })
                    + SSegmentedControl<int32>::Slot(0).Text(FText::FromString(TEXT("Off")))
                    + SSegmentedControl<int32>::Slot(1).Text(FText::FromString(TEXT("On"))))
        ];
}

auto
    SCkIntentDebugger_InputHudControls::
    OnResetReadout()
    -> FReply
{
    UCk_InputHud_UserSettings::Get_Mutable()->Reset_ReadoutTuning();
    return FReply::Handled();
}

// ====================================================================================================================
