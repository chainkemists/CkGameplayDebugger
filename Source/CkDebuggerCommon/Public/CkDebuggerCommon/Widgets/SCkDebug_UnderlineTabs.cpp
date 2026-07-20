#include "SCkDebug_UnderlineTabs.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_UnderlineTabs::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto ActiveTabId = InArgs._ActiveTabId;
    const auto OnSelected  = InArgs._OnTabSelected;
    const auto FontSize    = InArgs._FontSize > 0 ? InArgs._FontSize : CkStyle::FontSizeBody();

    auto Row = SNew(SHorizontalBox);

    for (const auto& Tab : InArgs._Tabs)
    {
        const auto TabId = Tab.Id;
        const auto IsActive = [ActiveTabId, TabId] { return ActiveTabId.Get(NAME_None) == TabId; };

        auto LabelRow = SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(Tab.Label)
                .Font(CkStyle::BoldFont(FontSize))
                .ColorAndOpacity_Lambda([IsActive]
                {
                    return FSlateColor{IsActive() ? CkStyle::Text() : CkStyle::TextDim()};
                })
            ];

        if (Tab.CountText.IsSet() || Tab.CountText.IsBound())
        {
            const auto CountText = Tab.CountText;
            LabelRow->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush_Small())
                    .BorderBackgroundColor(FSlateColor{CkStyle::NeutralDim()})
                    .Padding(FMargin{5.0f, 1.0f})
                    .Visibility_Lambda([CountText]
                    {
                        return CountText.Get(FText::GetEmpty()).IsEmpty()
                            ? EVisibility::Collapsed
                            : EVisibility::SelfHitTestInvisible;
                    })
                    [
                        SNew(STextBlock)
                        .Text(CountText)
                        .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    ]
                ];
        }

        {
            const auto ShowWarnDot = Tab.ShowWarnDot;
            LabelRow->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(6.0f)
                    .HeightOverride(6.0f)
                    .Visibility_Lambda([ShowWarnDot]
                    {
                        return ShowWarnDot.Get(false) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
                    })
                    [
                        SNew(SImage)
                        .Image(CkStyle::GetRoundedBrush_Pill())
                        .ColorAndOpacity(FSlateColor{CkStyle::Warn()})
                    ]
                ];
        }

        Row->AddSlot()
            .AutoWidth()
            [
                SNew(SBox)
                .Visibility(Tab.Visibility)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SButton)
                        .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                        .ContentPadding(InArgs._TabPadding)
                        .OnClicked_Lambda([OnSelected, TabId]
                        {
                            OnSelected.ExecuteIfBound(TabId);
                            return FReply::Handled();
                        })
                        [
                            LabelRow
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .HeightOverride(2.0f)
                        [
                            SNew(SImage)
                            .Image(CkStyle::GetFilledBrush())
                            .ColorAndOpacity_Lambda([IsActive]
                            {
                                return FSlateColor{IsActive() ? CkStyle::Accent() : FLinearColor::Transparent};
                            })
                        ]
                    ]
                ]
            ];
    }

    ChildSlot
    [
        Row
    ];
}

// ====================================================================================================================
