#include "SCkDebug_AlertRow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_AlertRow::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto Tone     = InArgs._Tone;
    const auto ToneColor = CkStyle::GetToneColor(Tone);
    const auto DimColor  = CkStyle::GetToneDimColor(Tone);

    auto Row = SNew(SHorizontalBox);

    if (NOT InArgs._Glyph.IsEmpty())
    {
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(STextBlock)
                .Text(InArgs._Glyph)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                .ColorAndOpacity(FSlateColor{ToneColor})
            ];
    }

    if (NOT InArgs._LeadText.IsEmpty())
    {
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(STextBlock)
                .Text(InArgs._LeadText)
                .Font(CkStyle::BoldFont(CkStyle::FontSizeBody()))
                .ColorAndOpacity(FSlateColor{ToneColor})
            ];
    }

    Row->AddSlot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(InArgs._BodyText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor{ToneColor})
            .AutoWrapText(true)
        ];

    if (NOT InArgs._FixText.IsEmpty())
    {
        auto FixColor = ToneColor; FixColor.A = 0.8f;
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(InArgs._FixText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{FixColor})
            ];
    }

    if (NOT InArgs._ActionText.IsEmpty())
    {
        const auto OnAction = InArgs._OnAction;
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(FSlateColor{ToneColor})
                .Padding(FMargin{1.0f})
                [
                    SNew(SButton)
                    .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                    .ContentPadding(FMargin{8.0f, 2.0f})
                    .OnClicked_Lambda([OnAction]
                    {
                        OnAction.ExecuteIfBound();
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(InArgs._ActionText)
                        .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                        .ColorAndOpacity(FSlateColor{ToneColor})
                    ]
                ]
            ];
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor{DimColor})
        .Padding(FMargin{16.0f, 6.0f})
        [
            Row
        ]
    ];
}

// ====================================================================================================================
