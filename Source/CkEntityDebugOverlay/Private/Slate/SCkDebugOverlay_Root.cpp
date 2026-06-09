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
        double                              InNow,
        bool                                bIsLocked)
    -> void
{
    if (_FocusCard.IsValid())
    {
        _FocusCard->Set_Model(InModel, InStyle, InHistory, InNow, bIsLocked);
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
        // SConstraintCanvas with a POINT anchor (0,0): Offset is (PosX, PosY, W, H) and,
        // with AutoSize, the child uses its own desired size (the pill hugs its text) —
        // Offset W/H are ignored. Alignment is the pivot ON the widget that lands at the
        // anchor+offset position: (0.5, 1.0) = bottom-centre, so the pill sits centred
        // directly above the entity's projected screen point.
        const auto PosX = static_cast<float>(TagInfo.ScreenPos.X);
        const auto PosY = static_cast<float>(TagInfo.ScreenPos.Y);

        TSharedPtr<SCkDebugOverlay_WorldTag> WorldTag;
        SAssignNew(WorldTag, SCkDebugOverlay_WorldTag)
            .Text(TagInfo.Text);
        WorldTag->Set_Style(TagInfo.Scale, TagInfo.Opacity);

        _TagCanvas->AddSlot()
            .Anchors(FAnchors{ 0.0f, 0.0f })
            .Offset(FMargin{ PosX, PosY, 0.0f, 0.0f })
            .Alignment(FVector2D{ 0.5f, 1.0f })
            .AutoSize(true)
            [
                WorldTag.ToSharedRef()
            ];
    }
}

// ====================================================================================================================
