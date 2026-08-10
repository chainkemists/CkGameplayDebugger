#include "SCkDebug_Chip.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
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

    // ----- ChipStyle ---------------------------------------------------------
    // The chip is a ring around a fill around a row. The axis is expressed by making a layer
    // transparent-and-zero-padded on demand rather than by building four different widget trees
    // (the SCkDebug_CountBadge idiom) — every option is then live, and Tint is today's chip
    // untouched, ring colour, fill colour, paddings and ink all identical.
    constexpr auto RingWidth    = 1.0f;
    constexpr auto FillPaddingX = 7.0f;
    constexpr auto FillPaddingY = 2.0f;

    auto Get_ChipStyle() -> ECkDebugAxis_ChipStyle
    {
        return UCkDebuggerStyleSettings::Get_Selection().ChipStyle;
    }

    auto Has_Box() -> bool
    {
        return Get_ChipStyle() != ECkDebugAxis_ChipStyle::TextOnly;
    }

    auto Get_RingPadding() -> FMargin
    {
        return Has_Box() ? FMargin{RingWidth} : FMargin{0.0f};
    }

    auto Get_FillPadding() -> FMargin
    {
        return Has_Box() ? FMargin{FillPaddingX, FillPaddingY} : FMargin{0.0f};
    }

    auto RingOf(ECkDebug_ChipKind InKind, bool InHighlighted) -> FLinearColor
    {
        if (NOT Has_Box())
        { return FLinearColor::Transparent; }

        if (InHighlighted)
        { return CkStyle::Accent(); }

        // Outline promotes the ring to the full tone; every other boxed option keeps the
        // half-alpha hairline the chip has always drawn.
        return Get_ChipStyle() == ECkDebugAxis_ChipStyle::Outline ? ToneOf(InKind) : BorderOf(InKind);
    }

    auto FillOf(ECkDebug_ChipKind InKind) -> FLinearColor
    {
        switch (Get_ChipStyle())
        {
            case ECkDebugAxis_ChipStyle::Solid:    return ToneOf(InKind);
            case ECkDebugAxis_ChipStyle::Outline:  return CkStyle::Bg2();
            case ECkDebugAxis_ChipStyle::TextOnly: return FLinearColor::Transparent;
            default:                               return DimOf(InKind);
        }
    }

    // Ink drives the label AND the leading dot / arrow glyph, so a Solid chip's contents stay
    // legible on top of a full-tone body. Under Tint this resolves to exactly what the label and
    // the glyph read before: TextDim for Neutral, the kind's tone otherwise.
    auto InkOf(ECkDebug_ChipKind InKind) -> FLinearColor
    {
        if (Get_ChipStyle() == ECkDebugAxis_ChipStyle::Solid)
        { return CkStyle::TextStrong(); }

        return InKind == ECkDebug_ChipKind::Neutral ? CkStyle::TextDim() : ToneOf(InKind);
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
                        return FSlateColor{ck_debug_chip::InkOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
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
                    return FSlateColor{ck_debug_chip::InkOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
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
                return FSlateColor{ck_debug_chip::InkOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
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
                return FSlateColor{
                    ck_debug_chip::RingOf(Kind.Get(ECkDebug_ChipKind::Neutral), Highlighted.Get(false))};
            })
            .Padding_Static(&ck_debug_chip::Get_RingPadding)
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor_Lambda([Kind]
                {
                    return FSlateColor{ck_debug_chip::FillOf(Kind.Get(ECkDebug_ChipKind::Neutral))};
                })
                .Padding_Static(&ck_debug_chip::Get_FillPadding)
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
