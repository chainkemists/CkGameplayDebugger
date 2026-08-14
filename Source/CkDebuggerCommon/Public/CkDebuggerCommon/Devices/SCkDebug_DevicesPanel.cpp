#include "CkDebuggerCommon/Devices/SCkDebug_DevicesPanel.h"

#include "CkDebuggerCommon/Devices/SCkDebug_DeviceKeyboard.h"
#include "CkDebuggerCommon/Devices/SCkDebug_DeviceMouse.h"
#include "CkDebuggerCommon/Devices/SCkDebug_DeviceGamepad.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_devices_panel
{
    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;

    constexpr auto KeyboardMaxWidth = 760.0f;
    constexpr auto KeyboardMaxHeight = 230.0f;

    constexpr auto MouseMaxWidth = 135.0f;
    constexpr auto MouseMaxHeight = 205.0f;

    constexpr auto GamepadMaxWidth = 320.0f;
    constexpr auto GamepadMaxHeight = 205.0f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DevicesPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    using namespace ck_debug_devices_panel;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::BgRoot())
        [
            SNew(SScrollBox)

            + SScrollBox::Slot().Padding(PadM, PadS)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                    [
                        SNew(SBox)
                            .MaxDesiredWidth(KeyboardMaxWidth)
                            .MaxDesiredHeight(KeyboardMaxHeight)
                        [
                            SNew(SCkDebug_DeviceKeyboard)
                                .Snapshot(InArgs._Snapshot)
                                .OnKeyClicked(InArgs._OnKeyClicked)
                                .KeyTooltip(InArgs._KeyTooltip)
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(PadM, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                            .MaxDesiredWidth(MouseMaxWidth)
                            .MaxDesiredHeight(MouseMaxHeight)
                        [
                            SNew(SCkDebug_DeviceMouse)
                                .Snapshot(InArgs._Snapshot)
                                .OnKeyClicked(InArgs._OnKeyClicked)
                                .KeyTooltip(InArgs._KeyTooltip)
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(PadM, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                            .MaxDesiredWidth(GamepadMaxWidth)
                            .MaxDesiredHeight(GamepadMaxHeight)
                        [
                            SNew(SCkDebug_DeviceGamepad)
                                .Snapshot(InArgs._Snapshot)
                                .OnKeyClicked(InArgs._OnKeyClicked)
                                .KeyTooltip(InArgs._KeyTooltip)
                        ]
                    ]
                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, PadM, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(InArgs._NoteText)
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                        .ColorAndOpacity(CkStyle::TextMute())
                        .Visibility(InArgs._NoteText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
                ]
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
