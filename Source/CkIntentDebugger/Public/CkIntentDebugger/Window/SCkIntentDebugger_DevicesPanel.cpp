#include "CkIntentDebugger/Window/SCkIntentDebugger_DevicesPanel.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkDebuggerCommon/Devices/SCkDebug_DeviceKeyboard.h"
#include "CkDebuggerCommon/Devices/SCkDebug_DeviceMouse.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_devices
{
    constexpr auto PadS = 4.0f;
    constexpr auto PadM = 8.0f;

    constexpr auto KeyboardMaxWidth = 760.0f;
    constexpr auto KeyboardMaxHeight = 230.0f;

    constexpr auto MouseMaxWidth = 135.0f;
    constexpr auto MouseMaxHeight = 205.0f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_DevicesPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    // One snapshot serves every device — each widget looks up only its own keys, and a device with no producer
    // (or none attached) renders dimmed rather than absent: the maintainer's rule is that every device is always
    // on screen, greyed when unavailable.
    const auto SnapshotLambda = [WeakViewModel = TWeakPtr<FCkIntentDebugger_ViewModel>{_ViewModel}]()
        -> const FCkDebug_DeviceSnapshot*
    {
        const auto ViewModel = WeakViewModel.Pin();
        return ViewModel.IsValid() ? &ViewModel->Get_DeviceSnapshot() : nullptr;
    };

    ChildSlot
    [
        SNew(SScrollBox)

        + SScrollBox::Slot().Padding(ck_intent_debugger_devices::PadM, ck_intent_debugger_devices::PadS)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                [
                    SNew(SBox)
                        .MaxDesiredWidth(ck_intent_debugger_devices::KeyboardMaxWidth)
                        .MaxDesiredHeight(ck_intent_debugger_devices::KeyboardMaxHeight)
                    [
                        SNew(SCkDebug_DeviceKeyboard)
                            .Snapshot_Lambda(SnapshotLambda)
                    ]
                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                    .Padding(ck_intent_debugger_devices::PadM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                        .MaxDesiredWidth(ck_intent_debugger_devices::MouseMaxWidth)
                        .MaxDesiredHeight(ck_intent_debugger_devices::MouseMaxHeight)
                    [
                        SNew(SCkDebug_DeviceMouse)
                            .Snapshot_Lambda(SnapshotLambda)
                    ]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, ck_intent_debugger_devices::PadM, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT(
                        "Every device edge is captured ungated; sub-frame taps coalesce to one row. Gamepad joins in the next slice.")))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::TextMute())
            ]
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
