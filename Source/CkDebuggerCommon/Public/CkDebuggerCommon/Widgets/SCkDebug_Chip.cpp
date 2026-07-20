#include "SCkDebug_Chip.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_GlowWrap.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "InputCoreTypes.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_chip
{
    auto ToneOf(ECkDebug_ChipKind InKind) -> FLinearColor
    {
        switch (InKind)
        {
            case ECkDebug_ChipKind::Satisfied:   return CkStyle::Ok();
            case ECkDebug_ChipKind::Unsatisfied: return CkStyle::Err();
            case ECkDebug_ChipKind::Effect:      return CkStyle::Accent();
            default:                             return CkStyle::TextDim();
        }
    }

    auto DimOf(ECkDebug_ChipKind InKind) -> FLinearColor
    {
        switch (InKind)
        {
            case ECkDebug_ChipKind::Satisfied:   return CkStyle::OkDim();
            case ECkDebug_ChipKind::Unsatisfied: return CkStyle::ErrDim();
            case ECkDebug_ChipKind::Effect:      return CkStyle::AccentDim();
            default:                             return CkStyle::NeutralDim();
        }
    }

    auto BorderOf(ECkDebug_ChipKind InKind) -> FLinearColor
    {
        if (InKind == ECkDebug_ChipKind::Neutral)
        { return CkStyle::Border(); }

        auto Color = ToneOf(InKind);
        Color.A = 0.55f;
        return Color;
    }
}

// ====================================================================================================================

auto
    SCkDebug_Chip::
    Construct(const FArguments& InArgs)
    -> void
{
    _Kind      = InArgs._Kind;
    _OnClicked = InArgs._OnClicked;
    _CopyText  = InArgs._CopyText;

    const auto Kind        = _Kind;
    const auto Highlighted = InArgs._Highlighted;

    if (_OnClicked.IsBound())
    { SetCursor(EMouseCursor::Hand); }

    auto Row = SNew(SHorizontalBox);

    if (InArgs._ShowDot)
    {
        // Effect chips lead with the "sets" arrow; condition chips lead with a
        // value dot. Both slots exist; visibility flips live with Kind.
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 5.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(6.0f)
                .HeightOverride(6.0f)
                .Visibility_Lambda([Kind]
                {
                    return Kind.Get(ECkDebug_ChipKind::Neutral) == ECkDebug_ChipKind::Effect
                        ? EVisibility::Collapsed
                        : EVisibility::SelfHitTestInvisible;
                })
                [
                    SNew(SImage)
                    .Image(CkStyle::GetRoundedBrush_Pill())
                    .ColorAndOpacity_Lambda([Kind]
                    {
                        return FSlateColor{ck_debug_chip::ToneOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
                    })
                ]
            ];

        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("→")))
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity_Lambda([Kind]
                {
                    return FSlateColor{ck_debug_chip::ToneOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
                })
                .Visibility_Lambda([Kind]
                {
                    return Kind.Get(ECkDebug_ChipKind::Neutral) == ECkDebug_ChipKind::Effect
                        ? EVisibility::SelfHitTestInvisible
                        : EVisibility::Collapsed;
                })
            ];
    }

    Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(InArgs._Text)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity_Lambda([Kind]
            {
                const auto Resolved = Kind.Get(ECkDebug_ChipKind::Neutral);
                return FSlateColor{Resolved == ECkDebug_ChipKind::Neutral
                    ? CkStyle::TextDim()
                    : ck_debug_chip::ToneOf(Resolved)};
            })
        ];

    ChildSlot
    [
        SNew(SCkDebug_GlowWrap)
        .Tight(true)
        .Extent(3.0f)
        .GlowColor_Lambda([Highlighted]
        {
            return Highlighted.Get(false) ? CkStyle::Accent() : FLinearColor::Transparent;
        })
        [
            SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor_Lambda([Kind, Highlighted]
            {
                return FSlateColor{Highlighted.Get(false)
                    ? CkStyle::Accent()
                    : ck_debug_chip::BorderOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
            })
            .Padding(FMargin{1.0f})
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor_Lambda([Kind]
                {
                    return FSlateColor{ck_debug_chip::DimOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
                })
                .Padding(FMargin{7.0f, 2.0f})
                [
                    Row
                ]
            ]
        ]
    ];
}

auto
    SCkDebug_Chip::
    OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && NOT _CopyText.IsEmpty())
    {
        return ck::DebugCopyMenu::Handle_RightClickToCopy(SharedThis(this), InMouseEvent, _CopyText);
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _OnClicked.IsBound())
    {
        _OnClicked.Execute();
        return FReply::Handled();
    }

    return SCompoundWidget::OnMouseButtonDown(InGeometry, InMouseEvent);
}

// ====================================================================================================================
