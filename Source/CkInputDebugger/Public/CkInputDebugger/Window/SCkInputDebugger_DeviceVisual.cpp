#include "CkInputDebugger/Window/SCkInputDebugger_DeviceVisual.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger_device_visual
{
    constexpr auto KeyCellHeight = 24.0f;
    constexpr auto KeyCellBaseWidth = 26.0f;

    struct FKeyCellDef
    {
        const TCHAR* Label;
        FKey  Key;
        float WidthFactor = 1.0f;
    };

    // A cell with no key is a spacer; width factors mirror a compact 60% board.
    auto Get_KeyboardRows() -> const TArray<TArray<FKeyCellDef>>&
    {
        static const TArray<TArray<FKeyCellDef>> Rows = {
            { {TEXT("ESC"), EKeys::Escape, 1.3f}, {TEXT("F1"), EKeys::F1}, {TEXT("F2"), EKeys::F2}, {TEXT("F3"), EKeys::F3}, {TEXT("F4"), EKeys::F4}, {TEXT("F5"), EKeys::F5}, {TEXT("F6"), EKeys::F6}, {TEXT("F7"), EKeys::F7}, {TEXT("F8"), EKeys::F8}, {TEXT("F9"), EKeys::F9}, {TEXT("F10"), EKeys::F10}, {TEXT("F11"), EKeys::F11}, {TEXT("F12"), EKeys::F12} },
            { {TEXT("`"), EKeys::Tilde}, {TEXT("1"), EKeys::One}, {TEXT("2"), EKeys::Two}, {TEXT("3"), EKeys::Three}, {TEXT("4"), EKeys::Four}, {TEXT("5"), EKeys::Five}, {TEXT("6"), EKeys::Six}, {TEXT("7"), EKeys::Seven}, {TEXT("8"), EKeys::Eight}, {TEXT("9"), EKeys::Nine}, {TEXT("0"), EKeys::Zero}, {TEXT("-"), EKeys::Hyphen}, {TEXT("="), EKeys::Equals}, {TEXT("BKSP"), EKeys::BackSpace, 1.6f} },
            { {TEXT("TAB"), EKeys::Tab, 1.5f}, {TEXT("Q"), EKeys::Q}, {TEXT("W"), EKeys::W}, {TEXT("E"), EKeys::E}, {TEXT("R"), EKeys::R}, {TEXT("T"), EKeys::T}, {TEXT("Y"), EKeys::Y}, {TEXT("U"), EKeys::U}, {TEXT("I"), EKeys::I}, {TEXT("O"), EKeys::O}, {TEXT("P"), EKeys::P}, {TEXT("["), EKeys::LeftBracket}, {TEXT("]"), EKeys::RightBracket}, {TEXT("\\"), EKeys::Backslash, 1.1f} },
            { {TEXT("CAPS"), EKeys::CapsLock, 1.8f}, {TEXT("A"), EKeys::A}, {TEXT("S"), EKeys::S}, {TEXT("D"), EKeys::D}, {TEXT("F"), EKeys::F}, {TEXT("G"), EKeys::G}, {TEXT("H"), EKeys::H}, {TEXT("J"), EKeys::J}, {TEXT("K"), EKeys::K}, {TEXT("L"), EKeys::L}, {TEXT(";"), EKeys::Semicolon}, {TEXT("'"), EKeys::Apostrophe}, {TEXT("ENTER"), EKeys::Enter, 1.8f} },
            { {TEXT("SHIFT"), EKeys::LeftShift, 2.3f}, {TEXT("Z"), EKeys::Z}, {TEXT("X"), EKeys::X}, {TEXT("C"), EKeys::C}, {TEXT("V"), EKeys::V}, {TEXT("B"), EKeys::B}, {TEXT("N"), EKeys::N}, {TEXT("M"), EKeys::M}, {TEXT(","), EKeys::Comma}, {TEXT("."), EKeys::Period}, {TEXT("/"), EKeys::Slash}, {TEXT("SHIFT"), EKeys::RightShift, 2.3f} },
            { {TEXT("CTRL"), EKeys::LeftControl, 1.4f}, {TEXT("ALT"), EKeys::LeftAlt, 1.4f}, {TEXT("SPACE"), EKeys::SpaceBar, 6.5f}, {TEXT("ALT"), EKeys::RightAlt, 1.4f}, {TEXT("CTRL"), EKeys::RightControl, 1.4f}, {TEXT("◀"), EKeys::Left}, {TEXT("▲▼"), EKeys::Up}, {TEXT("▶"), EKeys::Right} },
        };
        return Rows;
    }

    auto Get_MouseRows() -> const TArray<TArray<FKeyCellDef>>&
    {
        static const TArray<TArray<FKeyCellDef>> Rows = {
            { {TEXT("LMB"), EKeys::LeftMouseButton, 1.4f}, {TEXT("MMB"), EKeys::MiddleMouseButton}, {TEXT("RMB"), EKeys::RightMouseButton, 1.4f} },
            { {TEXT("WH▲"), EKeys::MouseScrollUp, 1.2f}, {TEXT("WH▼"), EKeys::MouseScrollDown, 1.2f} },
            { {TEXT("M4"), EKeys::ThumbMouseButton}, {TEXT("M5"), EKeys::ThumbMouseButton2} },
        };
        return Rows;
    }

    auto Get_GamepadRows() -> const TArray<TArray<FKeyCellDef>>&
    {
        static const TArray<TArray<FKeyCellDef>> Rows = {
            { {TEXT("LT"), EKeys::Gamepad_LeftTrigger}, {TEXT("LB"), EKeys::Gamepad_LeftShoulder}, {TEXT("SEL"), EKeys::Gamepad_Special_Left}, {TEXT("STA"), EKeys::Gamepad_Special_Right}, {TEXT("RB"), EKeys::Gamepad_RightShoulder}, {TEXT("RT"), EKeys::Gamepad_RightTrigger} },
            { {TEXT("D▲"), EKeys::Gamepad_DPad_Up}, {TEXT("D▼"), EKeys::Gamepad_DPad_Down}, {TEXT("D◀"), EKeys::Gamepad_DPad_Left}, {TEXT("D▶"), EKeys::Gamepad_DPad_Right}, {TEXT("A"), EKeys::Gamepad_FaceButton_Bottom}, {TEXT("B"), EKeys::Gamepad_FaceButton_Right} },
            { {TEXT("LS"), EKeys::Gamepad_Left2D}, {TEXT("L3"), EKeys::Gamepad_LeftThumbstick}, {TEXT("RS"), EKeys::Gamepad_Right2D}, {TEXT("R3"), EKeys::Gamepad_RightThumbstick}, {TEXT("X"), EKeys::Gamepad_FaceButton_Left}, {TEXT("Y"), EKeys::Gamepad_FaceButton_Top} },
        };
        return Rows;
    }

    auto Font_Cell() -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall() - 1); }
    auto Font_Label() -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall() - 1); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebugger_DeviceVisual::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _IsKeyPressed  = InArgs._IsKeyPressed;
    _IsKeyMapped   = InArgs._IsKeyMapped;
    _IsKeyRebound  = InArgs._IsKeyRebound;
    _IsKeyFiltered = InArgs._IsKeyFiltered;
    _KeyTooltip    = InArgs._KeyTooltip;
    _OnKeyClicked  = InArgs._OnKeyClicked;

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [ BuildDeviceBox(TEXT("KEYBOARD"), BuildKeyboard()) ]

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [ SNew(SBox).WidthOverride(150.0f) [ BuildDeviceBox(TEXT("MOUSE"), BuildMouse()) ] ]

            + SHorizontalBox::Slot().AutoWidth()
                [ SNew(SBox).WidthOverride(230.0f) [ BuildDeviceBox(TEXT("GAMEPAD"), BuildGamepad()) ] ]
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [ BuildLegend() ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebugger_DeviceVisual::
    BuildDeviceBox(
        const FString& InLabel,
        const TSharedRef<SWidget>& InContent)
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush_Large())
        .BorderBackgroundColor(CkStyle::Bg2())
        .Padding(FMargin(CkStyle::SpaceS))
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                        .Font_Static(&ck_input_debugger_device_visual::Font_Label)
                        .Text(FText::FromString(InLabel))
                        .ColorAndOpacity(CkStyle::TextMute())
                ]

            + SVerticalBox::Slot().AutoHeight()
                [ InContent ]
        ];
}

namespace ck_input_debugger_device_visual
{
    auto BuildGrid(
        const TArray<TArray<FKeyCellDef>>& InRows,
        TFunctionRef<TSharedRef<SWidget>(const FString&, const FKey&, float)> InBuildCell)
        -> TSharedRef<SWidget>
    {
        auto Grid = SNew(SVerticalBox);

        for (const auto& Row : InRows)
        {
            auto RowBox = SNew(SHorizontalBox);

            for (const auto& Cell : Row)
            {
                RowBox->AddSlot()
                    .FillWidth(Cell.WidthFactor)
                    .Padding(1.0f)
                    [ InBuildCell(Cell.Label, Cell.Key, Cell.WidthFactor) ];
            }

            Grid->AddSlot().AutoHeight() [ RowBox ];
        }

        return Grid;
    }
}

auto
    SCkInputDebugger_DeviceVisual::
    BuildKeyboard()
    -> TSharedRef<SWidget>
{
    using namespace ck_input_debugger_device_visual;
    return BuildGrid(Get_KeyboardRows(),
        [this](const FString& InLabel, const FKey& InKey, float InWidth) { return BuildKeyCell(InLabel, InKey, InWidth); });
}

auto
    SCkInputDebugger_DeviceVisual::
    BuildMouse()
    -> TSharedRef<SWidget>
{
    using namespace ck_input_debugger_device_visual;
    return BuildGrid(Get_MouseRows(),
        [this](const FString& InLabel, const FKey& InKey, float InWidth) { return BuildKeyCell(InLabel, InKey, InWidth); });
}

auto
    SCkInputDebugger_DeviceVisual::
    BuildGamepad()
    -> TSharedRef<SWidget>
{
    using namespace ck_input_debugger_device_visual;
    return BuildGrid(Get_GamepadRows(),
        [this](const FString& InLabel, const FKey& InKey, float InWidth) { return BuildKeyCell(InLabel, InKey, InWidth); });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebugger_DeviceVisual::
    BuildKeyCell(
        const FString& InLabel,
        const FKey& InKey,
        float InWidthFactor)
    -> TSharedRef<SWidget>
{
    using namespace ck_input_debugger_device_visual;

    const auto Key = InKey;

    return SNew(SBox)
        .HeightOverride(KeyCellHeight)
        .MinDesiredWidth(KeyCellBaseWidth * InWidthFactor)
        [
            SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Small())
                .BorderBackgroundColor_Lambda([this, Key]() { return Get_CellBackground(Key); })
                .ToolTipText_Lambda([this, Key]() -> FText
                {
                    if (_KeyTooltip.IsBound())
                    { return _KeyTooltip.Execute(Key); }
                    return FText::GetEmpty();
                })
                .OnMouseButtonDown_Lambda([this, Key](const FGeometry&, const FPointerEvent&) -> FReply
                {
                    if (_OnKeyClicked.IsBound())
                    {
                        _OnKeyClicked.Execute(Key);
                        return FReply::Handled();
                    }
                    return FReply::Unhandled();
                })
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Font_Static(&Font_Cell)
                        .Text(FText::FromString(InLabel))
                        .ColorAndOpacity_Lambda([this, Key]() { return Get_CellForeground(Key); })
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebugger_DeviceVisual::
    Get_CellBackground(
        FKey InKey) const
    -> FSlateColor
{
    const auto IsPressed  = _IsKeyPressed.IsBound() && _IsKeyPressed.Execute(InKey);
    const auto IsFiltered = _IsKeyFiltered.IsBound() && _IsKeyFiltered.Execute(InKey);

    if (IsPressed)
    { return CkStyle::Accent().CopyWithNewOpacity(0.28f); }

    if (IsFiltered)
    { return CkStyle::AccentDim(); }

    return CkStyle::BgRoot();
}

auto
    SCkInputDebugger_DeviceVisual::
    Get_CellForeground(
        FKey InKey) const
    -> FSlateColor
{
    const auto IsPressed = _IsKeyPressed.IsBound() && _IsKeyPressed.Execute(InKey);

    if (IsPressed)
    { return CkStyle::Accent(); }

    if (_IsKeyRebound.IsBound() && _IsKeyRebound.Execute(InKey))
    { return CkStyle::Warn(); }

    if (_IsKeyMapped.IsBound() && _IsKeyMapped.Execute(InKey))
    { return CkStyle::TextDim(); }

    return CkStyle::TextMute().CopyWithNewOpacity(0.55f);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputDebugger_DeviceVisual::
    BuildLegend()
    -> TSharedRef<SWidget>
{
    using namespace ck_input_debugger_device_visual;

    const auto MakeEntry = [](const FString& InLabel, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SBox).WidthOverride(11.0f).HeightOverride(11.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush_Small())
                            .BorderBackgroundColor(InColor)
                    ]
                ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Font_Static(&Font_Label)
                        .Text(FText::FromString(InLabel))
                        .ColorAndOpacity(CkStyle::TextDim())
                ];
    };

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ MakeEntry(TEXT("pressed"), CkStyle::Accent().CopyWithNewOpacity(0.5f)) ]
        + SHorizontalBox::Slot().AutoWidth() [ MakeEntry(TEXT("rebound"), CkStyle::Warn()) ]
        + SHorizontalBox::Slot().AutoWidth() [ MakeEntry(TEXT("mapped"), CkStyle::TextDim()) ]
        + SHorizontalBox::Slot().AutoWidth() [ MakeEntry(TEXT("unmapped"), CkStyle::TextMute().CopyWithNewOpacity(0.4f)) ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(STextBlock)
                    .Font_Static(&Font_Label)
                    .Text(FText::FromString(TEXT("hover a key = bound actions · click = filter the panes below")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
}

// --------------------------------------------------------------------------------------------------------------------
