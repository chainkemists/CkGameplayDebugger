#include "CkStyleLabDebugger/Widgets/SCkStyleLab_InputHudControls.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "InputCoreTypes.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_style_lab_input_hud_controls
{
    constexpr auto LabelWidth = 112.0f;
    constexpr auto ValueWidth = 116.0f;

    template <typename TEnum>
    auto Cycle(TEnum InValue, int32 InDirection, int32 InCount) -> TEnum
    {
        const auto Current = static_cast<int32>(InValue);
        const auto Next = ((Current + InDirection) % InCount + InCount) % InCount;
        return static_cast<TEnum>(Next);
    }

    auto
        Make_Row(
            const FText&       InLabel,
            const FText&       InTooltip,
            TSharedRef<SWidget> InValue)
        -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            .ToolTipText(InTooltip)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .WidthOverride(LabelWidth)
                    [
                        SNew(STextBlock)
                            .Text(InLabel)
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
            ]

            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                InValue
            ];
    }

    auto Make_CycleValue(
        TAttribute<FText> InValue,
        FOnClicked InPrevious,
        FOnClicked InNext) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25C0")))
                    .ToolTipText(FText::FromString(TEXT("Previous option")))
                    .OnClicked(InPrevious)
            ]

            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(SBox)
                    .MinDesiredWidth(ValueWidth)
                    [
                        SNew(STextBlock)
                            .Text(InValue)
                            .Justification(ETextJustify::Center)
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                    ]
            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("\x25B6")))
                    .ToolTipText(FText::FromString(TEXT("Next option")))
                    .OnClicked(InNext)
            ];
    }

    auto Get_PaletteLabel(ECk_InputHud_Palette InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_Palette::ArcticSignal:  return FText::FromString(TEXT("Arctic Signal"));
            case ECk_InputHud_Palette::EmberTerminal: return FText::FromString(TEXT("Ember Terminal"));
            case ECk_InputHud_Palette::OrchidSynth:   return FText::FromString(TEXT("Orchid Synth"));
            case ECk_InputHud_Palette::TacticalMint:  return FText::FromString(TEXT("Tactical Mint"));
            default:                                  return FText::FromString(TEXT("Arctic Signal"));
        }
    }

    auto Get_DensityLabel(ECk_InputHud_Density InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_Density::Compact:  return FText::FromString(TEXT("Compact"));
            case ECk_InputHud_Density::Standard: return FText::FromString(TEXT("Standard"));
            case ECk_InputHud_Density::Readable: return FText::FromString(TEXT("Readable"));
            default:                             return FText::FromString(TEXT("Compact"));
        }
    }

    auto Get_CornerLabel(ECk_InputHud_CornerStyle InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_CornerStyle::Sharp:   return FText::FromString(TEXT("Sharp"));
            case ECk_InputHud_CornerStyle::Soft:    return FText::FromString(TEXT("Soft"));
            case ECk_InputHud_CornerStyle::Rounded: return FText::FromString(TEXT("Rounded"));
            default:                                return FText::FromString(TEXT("Rounded"));
        }
    }

    auto Get_MetadataLabel(ECk_InputHud_MetadataMode InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_MetadataMode::Keys:    return FText::FromString(TEXT("Keys"));
            case ECk_InputHud_MetadataMode::Compact: return FText::FromString(TEXT("Compact"));
            case ECk_InputHud_MetadataMode::Full:    return FText::FromString(TEXT("Full"));
            default:                                 return FText::FromString(TEXT("Compact"));
        }
    }

    auto Get_FrameLabel(ECk_InputHud_FrameNotation InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_FrameNotation::Press: return FText::FromString(TEXT("Press"));
            case ECk_InputHud_FrameNotation::Delta: return FText::FromString(TEXT("Delta"));
            case ECk_InputHud_FrameNotation::Range: return FText::FromString(TEXT("Range"));
            default:                                return FText::FromString(TEXT("Press"));
        }
    }
}

// ====================================================================================================================

auto
    SCkStyleLab_InputHudControls::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _OnChanged = InArgs._OnChanged;

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
            .Padding(FMargin{CkStyle::SpaceM})
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("INPUT HUD - SIGNAL STRIP")))
                        .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                        .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
                [
                    Build_PaletteRow()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
                [
                    Build_DensityRow()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
                [
                    Build_CornerRow()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
                [
                    Build_MetadataRow()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    Build_FrameNotationRow()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    Build_NumericRows()
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
                [
                    Build_ColorRows()
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Reset Input HUD visuals")))
                        .ToolTipText(FText::FromString(TEXT("Restore the Signal Strip palette, density, borders and visual intensity defaults.")))
                        .OnClicked(FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnResetVisuals))
                ]
            ]
    ];
}

auto
    SCkStyleLab_InputHudControls::
    Build_PaletteRow()
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        FText::FromString(TEXT("Palette")),
        FText::FromString(TEXT("Select the Signal Strip surface and semantic color family. Picking a family resets custom colors.")),
        ck_style_lab_input_hud_controls::Make_CycleValue(
            TAttribute<FText>::CreateLambda([]() { return ck_style_lab_input_hud_controls::Get_PaletteLabel(UCk_InputHud_UserSettings::Get()->Palette); }),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCyclePalette, -1),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCyclePalette, 1)));
}

auto
    SCkStyleLab_InputHudControls::
    Build_DensityRow()
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        FText::FromString(TEXT("Density")),
        FText::FromString(TEXT("Change actual key geometry; independent from the runtime ck.InputOverlay.Scale multiplier.")),
        ck_style_lab_input_hud_controls::Make_CycleValue(
            TAttribute<FText>::CreateLambda([]() { return ck_style_lab_input_hud_controls::Get_DensityLabel(UCk_InputHud_UserSettings::Get_Density()); }),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleDensity, -1),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleDensity, 1)));
}

auto
    SCkStyleLab_InputHudControls::
    Build_CornerRow()
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        FText::FromString(TEXT("Key corners")),
        FText::FromString(TEXT("Shape of the panel and Signal Strip key cells.")),
        ck_style_lab_input_hud_controls::Make_CycleValue(
            TAttribute<FText>::CreateLambda([]() { return ck_style_lab_input_hud_controls::Get_CornerLabel(UCk_InputHud_UserSettings::Get_CornerStyle()); }),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleCorner, -1),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleCorner, 1)));
}

auto
    SCkStyleLab_InputHudControls::
    Build_MetadataRow()
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        FText::FromString(TEXT("Metadata")),
        FText::FromString(TEXT("Keys = state only; Compact = duration + route; Full = frames, sticks and all routing detail. Also exposed in Intent Debugger.")),
        ck_style_lab_input_hud_controls::Make_CycleValue(
            TAttribute<FText>::CreateLambda([]() { return ck_style_lab_input_hud_controls::Get_MetadataLabel(UCk_InputHud_UserSettings::Get_MetadataMode()); }),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleMetadata, -1),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleMetadata, 1)));
}

auto
    SCkStyleLab_InputHudControls::
    Build_FrameNotationRow()
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        FText::FromString(TEXT("Frame notation")),
        FText::FromString(TEXT("Press frame, compact start+delta, or full start-end range when Full metadata is active.")),
        ck_style_lab_input_hud_controls::Make_CycleValue(
            TAttribute<FText>::CreateLambda([]() { return ck_style_lab_input_hud_controls::Get_FrameLabel(UCk_InputHud_UserSettings::Get_FrameNotation()); }),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleFrameNotation, -1),
            FOnClicked::CreateSP(this, &SCkStyleLab_InputHudControls::OnCycleFrameNotation, 1)));
}

auto
    SCkStyleLab_InputHudControls::
    Build_NumericRows()
    -> TSharedRef<SWidget>
{
    const auto MakeNumeric = [this](
        const TCHAR* InLabel,
        const TCHAR* InTooltip,
        TAttribute<double> InValue,
        double InMin,
        double InMax,
        int32 InDigits,
        FOnCkDebug_NumericCommitted InCommitted) -> TSharedRef<SWidget>
    {
        return ck_style_lab_input_hud_controls::Make_Row(
            FText::FromString(InLabel),
            FText::FromString(InTooltip),
            SNew(SCkDebug_NumericEditor)
                .Value(InValue)
                .MinValue(InMin)
                .MaxValue(InMax > 0.0 ? TOptional<double>{InMax} : TOptional<double>{})
                .FractionalDigits(InDigits)
                .Width(76.0f)
                .OnValueCommitted(InCommitted));
    };

    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Horizontal padding"), TEXT("Independent left/right key-cell padding; long labels expand after this budget."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyPaddingX()); }), 0.0, 12.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyPaddingX(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Vertical padding"), TEXT("Independent top/bottom key-cell padding around label, pulse, and metadata decks."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyPaddingY()); }), 0.0, 6.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyPaddingY(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Corner radius"), TEXT("Numeric key-cell corner radius. The Key corners preset is a shortcut that writes 0, 3, or 6."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyCornerRadius()); }), 0.0, 12.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyCornerRadius(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Overall key opacity"), TEXT("Multiplicative opacity for every key layer: cap, border, text, pulse, metadata, and unrouted marker."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyOpacity()); }), 0.0, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyOpacity(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Key border px"), TEXT("Width of every key outline; modifiers keep the same width with a dashed pattern."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyBorderWidth()); }), 0.0, 2.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyBorderWidth(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Border opacity"), TEXT("Opacity multiplier for both container and key outlines."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_KeyBorderOpacity()); }), 0.0, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_KeyBorderOpacity(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Active fill"), TEXT("Opacity of the active-color fill on physically held keys."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_ActiveFillOpacity()); }), 0.0, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ActiveFillOpacity(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Active glow"), TEXT("Opacity of the retained active-key halo."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_ActiveGlowOpacity()); }), 0.0, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ActiveGlowOpacity(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Panel opacity"), TEXT("Background opacity of the Signal Strip panel; independent from whole-overlay opacity."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_PanelOpacity()); }), 0.15, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PanelOpacity(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Pulse scale"), TEXT("Scales tap/press dots and hold bars without changing key-cell geometry."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_PulseScale()); }), 0.0, 0.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PulseScale(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("History brightness"), TEXT("Brightness multiplier for released input history as it eases away."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_HistoryBrightness()); }), 0.15, 1.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_HistoryBrightness(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Press pop scale"), TEXT("Peak scale of a new key cell before it settles back to its regular size."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_PressPopScale()); }), 0.0, 0.0, 2,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PressPopScale(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Press pop ms"), TEXT("Duration of the new-key cell pop; zero disables the animation."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_PressPopDurationMs()); }), 0.0, 500.0, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_PressPopDurationMs(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [MakeNumeric(TEXT("Release ease ms"), TEXT("Additional easing window for released history; zero uses the existing direct fade."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_UserSettings::Get_ReleaseEaseMs()); }), 0.0, 1000.0, 0,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue) { UCk_InputHud_UserSettings::Get_Mutable()->Set_ReleaseEaseMs(static_cast<float>(InValue)); NotifyChanged(); }))]
        + SVerticalBox::Slot().AutoHeight()
        [MakeNumeric(TEXT("Hold bar max"), TEXT("Shared project width cap for the growing hold pulse, before the per-user pulse scale."),
            TAttribute<double>::CreateLambda([]() { return static_cast<double>(UCk_InputHud_Settings::Get_HoldBarMaxPx()); }), 8.0, 64.0, 1,
            FOnCkDebug_NumericCommitted::CreateLambda([this](double InValue)
            {
                auto* ProjectSettings = GetMutableDefault<UCk_InputHud_Settings>();
                ProjectSettings->HoldBarMaxPx = static_cast<float>(InValue);
                ProjectSettings->SaveConfig();
                UCk_InputHud_UserSettings::Get_Mutable()->NotifyChanged();
                NotifyChanged();
            }))];
}

auto
    SCkStyleLab_InputHudControls::
    Build_ColorRows()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)[Build_ColorRow(FText::FromString(TEXT("Container outline")), ECk_InputHud_ColorRole::ContainerOutline)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)[Build_ColorRow(FText::FromString(TEXT("Key outline")), ECk_InputHud_ColorRole::KeyBorder)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)[Build_ColorRow(FText::FromString(TEXT("Active fill")), ECk_InputHud_ColorRole::Active)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)[Build_ColorRow(FText::FromString(TEXT("Bound signal")), ECk_InputHud_ColorRole::Resolved)]
        + SVerticalBox::Slot().AutoHeight()[Build_ColorRow(FText::FromString(TEXT("Unbound signal")), ECk_InputHud_ColorRole::Unrouted)];
}

auto
    SCkStyleLab_InputHudControls::
    Build_ColorRow(
        const FText&           InLabel,
        ECk_InputHud_ColorRole InRole)
    -> TSharedRef<SWidget>
{
    return ck_style_lab_input_hud_controls::Make_Row(
        InLabel,
        FText::FromString(TEXT("Click the swatch to create a custom semantic color override. Selecting a palette resets overrides.")),
        SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SColorBlock)
                    .Color_Lambda([this, InRole]() { return Get_Color(InRole); })
                    .Size(FVector2D{64.0, 18.0})
                    .CornerRadius(FVector4{3.0f})
                    .ShowBackgroundForAlpha(false)
                    .OnMouseButtonDown(FPointerEventHandler::CreateSP(this, &SCkStyleLab_InputHudControls::OnColorMouseDown, InRole))
            ]);
}

auto
    SCkStyleLab_InputHudControls::
    OnCyclePalette(
        int32 InDirection)
    -> FReply
{
    constexpr auto Count = 4;
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Set_Palette(ck_style_lab_input_hud_controls::Cycle(Settings->Palette, InDirection, Count));
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnCycleDensity(
        int32 InDirection)
    -> FReply
{
    constexpr auto Count = 3;
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Set_Density(ck_style_lab_input_hud_controls::Cycle(Settings->Density, InDirection, Count));
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnCycleCorner(
        int32 InDirection)
    -> FReply
{
    constexpr auto Count = 3;
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Set_CornerStyle(ck_style_lab_input_hud_controls::Cycle(Settings->CornerStyle, InDirection, Count));
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnCycleMetadata(
        int32 InDirection)
    -> FReply
{
    constexpr auto Count = 3;
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Set_MetadataMode(ck_style_lab_input_hud_controls::Cycle(Settings->MetadataMode, InDirection, Count));
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnCycleFrameNotation(
        int32 InDirection)
    -> FReply
{
    constexpr auto Count = 3;
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Set_FrameNotation(ck_style_lab_input_hud_controls::Cycle(Settings->FrameNotation, InDirection, Count));
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnResetVisuals()
    -> FReply
{
    UCk_InputHud_UserSettings::Get_Mutable()->Reset_VisualTuning();
    NotifyChanged();
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnColorMouseDown(
        const FGeometry&,
        const FPointerEvent&   InEvent,
        ECk_InputHud_ColorRole InRole)
    -> FReply
{
    if (InEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    auto Args = FColorPickerArgs{
        Get_Color(InRole),
        FOnLinearColorValueChanged::CreateSP(this, &SCkStyleLab_InputHudControls::OnColorCommitted, InRole)};
    Args.ParentWidget = SharedThis(this);
    Args.bUseAlpha = false;
    Args.bOnlyRefreshOnMouseUp = true;
    Args.bClampValue = true;
    Args.bOpenAsMenu = true;
    OpenColorPicker(Args);
    return FReply::Handled();
}

auto
    SCkStyleLab_InputHudControls::
    OnColorCommitted(
        FLinearColor           InColor,
        ECk_InputHud_ColorRole InRole)
    -> void
{
    UCk_InputHud_UserSettings::Get_Mutable()->Set_CustomColor(InRole, InColor);
    NotifyChanged();
}

auto
    SCkStyleLab_InputHudControls::
    Get_Color(
        ECk_InputHud_ColorRole InRole) const
    -> FLinearColor
{
    const auto Colors = UCk_InputHud_UserSettings::Get_PaletteSnapshot();
    switch (InRole)
    {
        case ECk_InputHud_ColorRole::ContainerOutline: return Colors.ContainerOutline;
        case ECk_InputHud_ColorRole::KeyBorder: return Colors.KeyBorder;
        case ECk_InputHud_ColorRole::Active:    return Colors.Active;
        case ECk_InputHud_ColorRole::Resolved:  return Colors.Resolved;
        case ECk_InputHud_ColorRole::Unrouted:  return Colors.Unrouted;
        default:                                return Colors.KeyBorder;
    }
}

auto
    SCkStyleLab_InputHudControls::
    NotifyChanged()
    -> void
{
    if (_OnChanged.IsBound())
    { _OnChanged.Execute(); }
}

// ====================================================================================================================
