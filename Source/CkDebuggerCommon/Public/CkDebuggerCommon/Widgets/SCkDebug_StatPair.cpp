#include "SCkDebug_StatPair.h"

#include "SCkDebug_SelectableLabel.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

auto
    SCkDebug_StatPair::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto ValueFontSize = InArgs._ValueFontSize > 0 ? InArgs._ValueFontSize : 14;
    const auto LabelFontSize = InArgs._LabelFontSize > 0 ? InArgs._LabelFontSize :  8;

    // Default value color = primary text. Override via attribute when the
    // call-out matters (e.g. amber for cost, red for errors).
    auto ValueColor = InArgs._ValueColor;
    if (NOT ValueColor.IsSet() && NOT ValueColor.IsBound())
    {
        ValueColor = FSlateColor(CkDebugStyle::Text());
    }

    ChildSlot
    [
        SNew(SHorizontalBox)

            // Bold value
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(InArgs._Value)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", ValueFontSize))
                        .ColorAndOpacity(ValueColor)
                ]

            // Uppercase muted label
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(InArgs._Label)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", LabelFontSize))
                        .ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
                ]
    ];
}

// ====================================================================================================================
