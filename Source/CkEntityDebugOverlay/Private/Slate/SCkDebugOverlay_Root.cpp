// Implements the viewport root widget for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

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

namespace ck_debugoverlay_root
{
    // Dark ink for text sitting on a saturated provider fill — same reading as the focus card's.
    constexpr auto ChipInk = FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f };

    // ProviderChipStyle at plate density (3px side padding, micro font). Tint is the badge
    // exactly as the near-plate shipped; the provider color is per-badge, so this cannot go
    // through the tone-keyed ck::debug_axes::Make_ProviderChip.
    auto Make_PlateBadge(
        const FText&        InText,
        const FLinearColor& InProviderColor) -> TSharedRef<SWidget>
    {
        const auto Make_Pill = [&](const FLinearColor& InFill, const FLinearColor& InInk) -> TSharedRef<SWidget>
        {
            return SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(InFill)
                .VAlign(VAlign_Center)
                .Padding(ck::debug_axes::Apply_RowDensity(FMargin{ 3.0f, 0.0f }))
                [
                    SNew(STextBlock)
                        .Text(InText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(InInk)
                ];
        };

        switch (UCkDebuggerStyleSettings::Get_Selection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:
                return Make_Pill(InProviderColor, ChipInk);

            case ECkDebugAxis_ProviderChipStyle::Solid:
                return Make_Pill(InProviderColor, CkStyle::TextStrong());

            // The plate badge text is ALREADY the provider abbreviation, so this option drops the
            // pill rather than truncating a second time.
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly:
                return SNew(STextBlock)
                    .Text(InText)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(InProviderColor);
        }

        return Make_Pill(InProviderColor, ChipInk);
    }
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
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.82f))
        .Padding(ck::debug_axes::Apply_RowDensity(FMargin{ CkStyle::SpaceS, CkStyle::SpaceXS }))
        [
            SAssignNew(_HintsText, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextMute())
        ];

    DoRebuildLayout();
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_PlateLayout(
        ECk_DebugOverlay_PlateAnchor InAnchor,
        float InWidth,
        float InMaxHeightFraction)
    -> void
{
    if (InAnchor == _PlateAnchor &&
        FMath::IsNearlyEqual(InWidth, _PlateWidth) &&
        FMath::IsNearlyEqual(InMaxHeightFraction, _PlateMaxHeightFraction))
    { return; }

    _PlateAnchor            = InAnchor;
    _PlateWidth             = InWidth;
    _PlateMaxHeightFraction = InMaxHeightFraction;
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
        _FocusCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkStyle::SpaceM);
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
        // top-left (top-right if the card is top-left). Child widgets are created once in Construct and re-slotted
        // here, so re-anchoring at runtime keeps all card/canvas/strip state.
        SNew(SOverlay)

        // Layer 0: world-anchored tags fill the full viewport area.
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            _TagCanvas.ToSharedRef()
        ]

        // Layer 1: card strip (primary + pinned) at the configured anchor. Height is
        // budgeted to a fraction of the viewport (live — tracks resizes) and CLIPS at
        // the cap: the root is hit-test invisible, so a scrollbar would be
        // uninteractable.
        + SOverlay::Slot()
        .HAlign(HAlign)
        .VAlign(VAlign)
        .Padding(FMargin{ OverlayRoot_Constants::FocusCardMargin })
        [
            SNew(SBox)
                .Clipping(EWidgetClipping::ClipToBounds)
                .MaxDesiredHeight_Lambda([this]() -> FOptionalSize
                {
                    const auto ViewportH = GetCachedGeometry().GetLocalSize().Y;
                    if (ViewportH <= KINDA_SMALL_NUMBER)
                    { return FOptionalSize{}; }   // no geometry yet — unconstrained

                    return FOptionalSize{
                        ViewportH * _PlateMaxHeightFraction
                        - 2.0f * OverlayRoot_Constants::FocusCardMargin };
                })
                [
                    _CardStrip.ToSharedRef()
                ]
        ]

        // Layer 2: key-hints strip top-left (top-right when the card is top-left).
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
    // Hints live in the TOP-LEFT by default, flipping to the TOP-RIGHT only when the focus
    // card itself is anchored top-left (so the two never share that corner).
    OutV = VAlign_Top;
    OutH = (_PlateAnchor == ECk_DebugOverlay_PlateAnchor::TopLeft) ? HAlign_Right : HAlign_Left;
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
        bool                                bIsPinned,
        int32                               InCoLocatedIndex,
        int32                               InCoLocatedCount,
        const FText&                        InLayoutLabel)
    -> void
{
    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_Model(InModel, InStyle, InHistory, InNow, bIsLocked, bIsPinned,
            InCoLocatedIndex, InCoLocatedCount, InLayoutLabel);
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
        NewCard->Set_WrapWidth(_PlateWidth - 4.0f - 2.0f * CkStyle::SpaceM);
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
                ck_debugoverlay_root::Make_PlateBadge(FText::FromString(Badge.Text), Badge.Color)
            ];
    }

    auto Body = SNew(SVerticalBox);

    Body->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(STextBlock)
                .Text(InInfo.Header)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall()))
                .ColorAndOpacity(CkStyle::TextStrong())
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

    // The focus entity's plate gets a bright outer ring so it visibly matches the emphasized
    // (1.25x) diamond marker — you can always tell which plate belongs to the focused entity.
    const auto RingColor = InInfo.bIsFocus
        ? FLinearColor{ 1.0f, 1.0f, 1.0f, 1.0f }
        : FLinearColor::Transparent;

    return SNew(SBorder)
        .Visibility(EVisibility::HitTestInvisible)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(RingColor)
        .Padding(FMargin{ InInfo.bIsFocus ? 2.0f : 0.0f })
        [
            SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.82f))
                .Padding(ck::debug_axes::Apply_RowDensity(FMargin{ CkStyle::SpaceS, CkStyle::SpaceXS }))
                [
                    Body
                ]
        ];
}

// ====================================================================================================================
