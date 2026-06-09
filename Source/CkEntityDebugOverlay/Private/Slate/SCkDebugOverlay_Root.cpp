// Implements the viewport root widget for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

// ====================================================================================================================

namespace OverlayRoot_Constants
{
    // Approximate size given to each world-tag slot so it can lay out.
    // The tag wraps to its natural size but the canvas slot needs a non-zero
    // preferred size for hit-test region (irrelevant here — we're hit-invisible).
    constexpr float WorldTagSlotWidth  = 300.0f;
    constexpr float WorldTagSlotHeight = 24.0f;

    // Pixel offset for the focus card from the top-left of the viewport.
    constexpr float FocusCardOffsetX = 8.0f;
    constexpr float FocusCardOffsetY = 8.0f;

    // Approximate max width for the focus card slot.
    constexpr float FocusCardWidth = 480.0f;
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

    ChildSlot
    [
        // Overlay: tag canvas fills the viewport, focus card sits on top at a
        // fixed offset from top-left.
        SNew(SOverlay)

        // Layer 0: world-anchored tags fill the full viewport area.
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            _TagCanvas.ToSharedRef()
        ]

        // Layer 1: focus card, top-left corner.
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin{ OverlayRoot_Constants::FocusCardOffsetX, OverlayRoot_Constants::FocusCardOffsetY, 0.0f, 0.0f })
        [
            SNew(SBox)
                .WidthOverride(OverlayRoot_Constants::FocusCardWidth)
                [
                    _FocusCard.ToSharedRef()
                ]
        ]
    ];
}

// ====================================================================================================================

auto
    SCkDebugOverlay_Root::
    Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow)
    -> void
{
    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_Model(InModel, InStyle, InHistory, InNow);
    }
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
        // SConstraintCanvas positions children via Offset (left, top, right, bottom)
        // when anchored at (0,0)-(0,0) (top-left corner, size driven by Offset width/height).
        // We set left/top to the desired screen position and right/bottom to provide the
        // preferred slot dimensions (width = WorldTagSlotSize.X, height = WorldTagSlotSize.Y).
        const auto OffsetLeft   = static_cast<float>(TagInfo.ScreenPos.X);
        const auto OffsetTop    = static_cast<float>(TagInfo.ScreenPos.Y);
        const auto OffsetRight  = OffsetLeft  + OverlayRoot_Constants::WorldTagSlotWidth;
        const auto OffsetBottom = OffsetTop   + OverlayRoot_Constants::WorldTagSlotHeight;

        TSharedPtr<SCkDebugOverlay_WorldTag> WorldTag;
        SAssignNew(WorldTag, SCkDebugOverlay_WorldTag)
            .Text(TagInfo.Text);
        WorldTag->Set_Style(TagInfo.Scale, TagInfo.Opacity);

        _TagCanvas->AddSlot()
            .Anchors(FAnchors{ 0.0f, 0.0f, 0.0f, 0.0f })
            .Offset(FMargin{ OffsetLeft, OffsetTop, OffsetRight, OffsetBottom })
            .Alignment(FVector2D{ 0.0f, 0.0f })
            [
                WorldTag.ToSharedRef()
            ];
    }
}

// ====================================================================================================================
