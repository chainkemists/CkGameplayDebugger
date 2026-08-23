#include "SCkDebug_StageStrip.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_StageCard::Construct(const FArguments& InArgs) -> void
{
    const auto Stage = InArgs._Stage;
    const auto* Icon = FCkIconStyle::Get_Brush(Stage.IconId, ECk_Icon_BrushSize::Size_16x16);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage_Lambda([] { return ck::debug_axes::Get_SurfaceBrush(2); })
        .BorderBackgroundColor_Lambda([Tone = Stage.Tone]
        { return FSlateColor{CkStyle::GetToneDimColor(Tone.Get(ECk_Tone::Neutral))}; })
        .Padding(CkStyle::SpaceM)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
                [
                    SNew(SCkDebug_Icon).Brush(Icon).Meaning(Stage.Label)
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(Stage.Label)
                    .TransformPolicy(ETextTransformPolicy::ToUpper)
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity_Lambda([Tone = Stage.Tone]
                    { return FSlateColor{CkStyle::GetToneColor(Tone.Get(ECk_Tone::Neutral))}; })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(Stage.Value)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(Stage.Meta)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextMute())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ]
    ];
}

auto SCkDebug_StageStrip::Construct(const FArguments& InArgs) -> void
{
    auto Grid = SNew(SUniformGridPanel).SlotPadding(FMargin{CkStyle::SpaceXS});
    for (auto Index = 0; Index < InArgs._Stages.Num(); ++Index)
    {
        Grid->AddSlot(Index, 0)
        [
            SNew(SCkDebug_StageCard).Stage(InArgs._Stages[Index])
        ];
    }
    ChildSlot[Grid];
}

// --------------------------------------------------------------------------------------------------------------------
