// Implements the viewport root widget for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace OverlayRoot_Constants
{
    // Pixel margin between the focus card and the viewport edge it anchors to.
    constexpr float FocusCardMargin = 8.0f;
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Construct(const FArguments& /*InArgs*/)
    -> void
{
    SetVisibility(EVisibility::HitTestInvisible);

    SAssignNew(_FocusCard, SCkDebugOverlay_FocusCard);
    SAssignNew(_TagCanvas, SConstraintCanvas);
    SAssignNew(_CardStrip, SVerticalBox);

    SAssignNew(_HintsBox, SBorder)
        .Visibility(EVisibility::Collapsed)
        .BorderImage(CkDebugStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.82f))
        .Padding(FMargin{ CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
        [
            SAssignNew(_HintsText, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro()))
                .ColorAndOpacity(CkDebugStyle::TextMute())
        ];

    DoRebuildLayout();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_PlateLayout(
        ECk_DebugOverlay_PlateAnchor InAnchor,
        float InWidth)
    -> void
{
    if (InAnchor == _PlateAnchor && FMath::IsNearlyEqual(InWidth, _PlateWidth))
    { return; }

    _PlateAnchor = InAnchor;
    _PlateWidth  = InWidth;
    DoRebuildLayout();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoRebuildLayout()
    -> void
{
    // Tell the card its inner content width so its section wrap-boxes wrap at the
    // real width (lock ring = 2px each side, card padding = SpaceM each side).
    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkDebugStyle::SpaceM);
    }

    auto HAlign = HAlign_Right;
    auto VAlign = VAlign_Top;

    switch (_PlateAnchor)
    {
        case ECk_DebugOverlay_PlateAnchor::TopLeft:      HAlign = HAlign_Left;   VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::TopCenter:    HAlign = HAlign_Center; VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::TopRight:     HAlign = HAlign_Right;  VAlign = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::Left:         HAlign = HAlign_Left;   VAlign = VAlign_Center; break;
        case ECk_DebugOverlay_PlateAnchor::Right:        HAlign = HAlign_Right;  VAlign = VAlign_Center; break;
        case ECk_DebugOverlay_PlateAnchor::BottomLeft:   HAlign = HAlign_Left;   VAlign = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::BottomCenter: HAlign = HAlign_Center; VAlign = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::BottomRight:  HAlign = HAlign_Right;  VAlign = VAlign_Bottom; break;
    }

    auto HintsH = HAlign_Left;
    auto HintsV = VAlign_Bottom;
    Resolve_HintsAnchor(HintsH, HintsV);

    ChildSlot
    [
        // Overlay: tag canvas fills the viewport; the card strip (primary + pinned cards)
        // sits on top anchored to the settings-driven corner/edge; the key-hints strip sits
        // in the opposite corner. Child widgets are created once in Construct and re-slotted
        // here, so re-anchoring at runtime keeps all card/canvas/strip state.
        SNew(SOverlay)

        // Layer 0: world-anchored tags fill the full viewport area.
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            _TagCanvas.ToSharedRef()
        ]

        // Layer 1: card strip (primary + pinned) at the configured anchor.
        + SOverlay::Slot()
        .HAlign(HAlign)
        .VAlign(VAlign)
        .Padding(FMargin{ OverlayRoot_Constants::FocusCardMargin })
        [
            _CardStrip.ToSharedRef()
        ]

        // Layer 2: key-hints strip in the opposite corner.
        + SOverlay::Slot()
        .HAlign(HintsH)
        .VAlign(HintsV)
        .Padding(FMargin{ OverlayRoot_Constants::FocusCardMargin })
        [
            _HintsBox.ToSharedRef()
        ]
    ];

    DoRebuild_CardStrip();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoRebuild_CardStrip()
    -> void
{
    if (NOT _CardStrip.IsValid())
    { return; }

    _CardStrip->ClearChildren();

    const auto AddCard = [this](const TSharedPtr<SCkDebugOverlay_FocusCard>& InCard)
    {
        if (NOT InCard.IsValid())
        { return; }

        _CardStrip->AddSlot()
            .AutoHeight()
            .Padding(FMargin{ 0.0f, 0.0f, 0.0f, OverlayRoot_Constants::FocusCardMargin })
            [
                SNew(SBox)
                    .WidthOverride(_PlateWidth)
                    [
                        InCard.ToSharedRef()
                    ]
            ];
    };

    AddCard(_FocusCard);
    for (const auto& Pinned : _PinnedCards)
    { AddCard(Pinned); }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Resolve_HintsAnchor(EHorizontalAlignment& OutH, EVerticalAlignment& OutV) const
    -> void
{
    // Opposite corner of the focus-card anchor (flip both axes; center stays centered).
    switch (_PlateAnchor)
    {
        case ECk_DebugOverlay_PlateAnchor::TopLeft:      OutH = HAlign_Right;  OutV = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::TopCenter:    OutH = HAlign_Center; OutV = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::TopRight:     OutH = HAlign_Left;   OutV = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::Left:         OutH = HAlign_Right;  OutV = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::Right:        OutH = HAlign_Left;   OutV = VAlign_Bottom; break;
        case ECk_DebugOverlay_PlateAnchor::BottomLeft:   OutH = HAlign_Right;  OutV = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::BottomCenter: OutH = HAlign_Center; OutV = VAlign_Top;    break;
        case ECk_DebugOverlay_PlateAnchor::BottomRight:  OutH = HAlign_Left;   OutV = VAlign_Top;    break;
        default:                                         OutH = HAlign_Left;   OutV = VAlign_Bottom; break;
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked,
        int32                               InCoLocatedIndex,
        int32                               InCoLocatedCount)
    -> void
{
    if (_FocusCard.IsValid())
    {
        constexpr auto NotPinned = false;
        _FocusCard->Set_Model(InModel, InStyle, InHistory, InNow, bIsLocked, NotPinned,
            InCoLocatedIndex, InCoLocatedCount);
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_PinnedCards(
        const TArray<FCk_DebugOverlay_EntityModel>& InModels,
        const FCk_DebugOverlay_RenderStyle&         InStyle,
        const FCk_DebugOverlay_History&             InHistory,
        double                                      InNow)
    -> void
{
    const auto DesiredNum  = InModels.Num();
    const auto CountChanged = _PinnedCards.Num() != DesiredNum;

    // Grow / shrink the pinned-card pool (reuse existing widgets across frames).
    while (_PinnedCards.Num() < DesiredNum)
    {
        TSharedPtr<SCkDebugOverlay_FocusCard> NewCard;
        SAssignNew(NewCard, SCkDebugOverlay_FocusCard);
        NewCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkDebugStyle::SpaceM);
        _PinnedCards.Add(NewCard);
    }
    while (_PinnedCards.Num() > DesiredNum)
    {
        _PinnedCards.Pop();
    }

    if (CountChanged)
    {
        DoRebuild_CardStrip();
    }

    constexpr auto NotLocked = false;
    constexpr auto IsPinned  = true;
    for (auto Idx = 0; Idx < DesiredNum; ++Idx)
    {
        if (_PinnedCards[Idx].IsValid())
        {
            _PinnedCards[Idx]->Set_Model(InModels[Idx], InStyle, InHistory, InNow, NotLocked, IsPinned);
        }
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Update_KeyHints(
        const FString& InCompact,
        const FString& InFull,
        bool           bShowFull,
        bool           bVisible)
    -> void
{
    if (NOT _HintsBox.IsValid() || NOT _HintsText.IsValid())
    { return; }

    if (NOT bVisible)
    {
        _HintsBox->SetVisibility(EVisibility::Collapsed);
        return;
    }

    _HintsBox->SetVisibility(EVisibility::HitTestInvisible);
    _HintsText->SetText(FText::FromString(bShowFull ? InFull : InCompact));
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Update_WorldTags(const TArray<FCk_DebugOverlay_WorldTagInfo>& InTags)
    -> void
{
    if (NOT _TagCanvas.IsValid())
    {
        return;
    }

    _TagCanvas->ClearChildren();

    for (const auto& TagInfo : InTags)
    {
        // SConstraintCanvas with a POINT anchor (0,0): Offset is (PosX, PosY, W, H) and,
        // with AutoSize, the child uses its own desired size (the pill hugs its text) —
        // Offset W/H are ignored. Alignment is the pivot ON the widget that lands at the
        // anchor+offset position: (0.5, 1.0) = bottom-centre, so the pill sits centred
        // directly above the entity's projected screen point.
        const auto PosX = static_cast<float>(TagInfo.ScreenPos.X);
        const auto PosY = static_cast<float>(TagInfo.ScreenPos.Y);

        auto Content = TSharedPtr<SWidget>{};
        if (TagInfo.bIsPlate)
        {
            Content = DoBuild_NearPlate(TagInfo);
        }
        else
        {
            TSharedPtr<SCkDebugOverlay_WorldTag> WorldTag;
            SAssignNew(WorldTag, SCkDebugOverlay_WorldTag)
                .Text(TagInfo.Text);
            WorldTag->Set_Style(TagInfo.Scale, TagInfo.Opacity);
            Content = WorldTag;
        }

        _TagCanvas->AddSlot()
            .Anchors(FAnchors{ 0.0f, 0.0f })
            .Offset(FMargin{ PosX, PosY, 0.0f, 0.0f })
            .Alignment(FVector2D{ 0.5f, 1.0f })
            .AutoSize(true)
            [
                Content.ToSharedRef()
            ];
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    DoBuild_NearPlate(const FCk_DebugOverlay_WorldTagInfo& InInfo)
    -> TSharedRef<SWidget>
{
    // Ultra-condensed plate: name header with a row of colored feature-abbreviation
    // badges directly under it. Mirrors the focus card's visual language (dark
    // rounded panel, provider-colored chips, micro fonts) at minimum footprint.
    auto BadgeRow = SNew(SWrapBox)
        .UseAllottedSize(false); // hug content — plate stays as narrow as its badges

    for (const auto& Badge : InInfo.Badges)
    {
        BadgeRow->AddSlot()
            .Padding(FMargin{ 0.0f, 0.0f, 2.0f, 0.0f })
            [
                SNew(SBorder)
                    .BorderImage(CkDebugStyle::GetRoundedBrush())
                    .BorderBackgroundColor(Badge.Color)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ 3.0f, 0.0f })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Badge.Text))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro()))
                            .ColorAndOpacity(FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f })
                    ]
            ];
    }

    auto Body = SNew(SVerticalBox);

    Body->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(STextBlock)
                .Text(InInfo.Header)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
                .ColorAndOpacity(CkDebugStyle::TextStrong())
        ];

    if (InInfo.Badges.Num() > 0)
    {
        Body->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(FMargin{ 0.0f, 2.0f, 0.0f, 0.0f })
            [
                BadgeRow
            ];
    }

    return SNew(SBorder)
        .Visibility(EVisibility::HitTestInvisible)
        .BorderImage(CkDebugStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.82f))
        .Padding(FMargin{ CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
        [
            Body
        ];
}

// ====================================================================================================================
