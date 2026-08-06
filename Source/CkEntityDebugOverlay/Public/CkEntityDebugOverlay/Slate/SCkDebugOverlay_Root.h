#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Containers/Array.h"
#include "Math/Vector2D.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"

struct FCk_DebugOverlay_EntityModel;
class  FCk_DebugOverlay_History;
class  SCkDebugOverlay_FocusCard;
class  SCkDebugOverlay_WorldTag;
class  SConstraintCanvas;
class  SVerticalBox;
class  SBorder;
class  STextBlock;

// ====================================================================================================================
// Viewport root for the on-screen entity debug overlay.
//
// Owns:
//   - The focus card (plate), anchored to a settings-driven viewport corner/edge
//     (default top-right — keeps clear of engine on-screen debug text).
//   - A SConstraintCanvas of world-anchored tags, each at a supplied screen
//     position.
//
// Hit-test invisible overall — does not consume input.
// ====================================================================================================================

class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_Root : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugOverlay_Root) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Forwards to the primary (auto-following) focus card.
    // bIsLocked: amber ring (focus lock). bIsPinned: cyan ring, which takes precedence.
    // InCoLocatedIndex/Count: "i/N" header badge when the focus entity is one of several
    // co-located entities.
    auto Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked        = false,
        bool                                bIsPinned        = false,
        int32                               InCoLocatedIndex = INDEX_NONE,
        int32                               InCoLocatedCount = 0) -> void;

    // Rebuilds the pinned-card strip (one persistent live card per pinned entity), stacked
    // under the primary card at the same anchor. Each pinned card gets a cyan ring.
    auto Set_PinnedCards(
        const TArray<FCk_DebugOverlay_EntityModel>& InModels,
        const FCk_DebugOverlay_RenderStyle&         InStyle,
        const FCk_DebugOverlay_History&             InHistory,
        double                                      InNow) -> void;

    // Updates the keyboard-hints strip (corner opposite the focus card). InCompact is the
    // always-on one-liner; InFull is the expanded legend shown when bShowFull is true.
    // bVisible hides the strip entirely (ShowKeyHints setting off).
    auto Update_KeyHints(
        const FString& InCompact,
        const FString& InFull,
        bool           bShowFull,
        bool           bVisible) -> void;

    // Rebuilds the canvas: one SCkDebugOverlay_WorldTag per entry.
    // Each entry carries a screen position (absolute viewport pixels, top-left origin),
    // label text, and distance-driven scale + opacity (B1).
    auto Update_WorldTags(const TArray<FCk_DebugOverlay_WorldTagInfo>& InTags) -> void;

    // Re-anchors / resizes the focus card. Cheap no-op when unchanged; rebuilds the
    // slot tree (re-using the existing child widgets) when anchor, width, or the
    // viewport-height budget fraction differ. Content past the height budget clips
    // (hit-test-invisible root — a scrollbar would be uninteractable).
    auto Set_PlateLayout(
        ECk_DebugOverlay_PlateAnchor InAnchor,
        float                        InWidth,
        float                        InMaxHeightFraction = 0.66f) -> void;

private:
    auto DoRebuildLayout() -> void;

    // (Re)populates the vertical card strip: primary focus card first, then one card per
    // pinned model. Called from DoRebuildLayout and Set_PinnedCards.
    auto DoRebuild_CardStrip() -> void;

    // Builds the ultra-condensed near plate (name header + colored feature badges).
    auto DoBuild_NearPlate(const FCk_DebugOverlay_WorldTagInfo& InInfo) -> TSharedRef<SWidget>;

    // Resolve the hints-strip anchor as the opposite corner of the focus-card anchor.
    auto Resolve_HintsAnchor(EHorizontalAlignment& OutH, EVerticalAlignment& OutV) const -> void;

private:
    TSharedPtr<SCkDebugOverlay_FocusCard> _FocusCard;
    TSharedPtr<SConstraintCanvas>         _TagCanvas;

    // Vertical strip holding the primary card + one card per pinned entity.
    TSharedPtr<SVerticalBox>                            _CardStrip;
    TArray<TSharedPtr<SCkDebugOverlay_FocusCard>>       _PinnedCards;

    // Keyboard-hints strip (opposite corner from the focus card).
    TSharedPtr<SBorder>    _HintsBox;
    TSharedPtr<STextBlock> _HintsText;

    ECk_DebugOverlay_PlateAnchor _PlateAnchor = ECk_DebugOverlay_PlateAnchor::TopRight;
    float                        _PlateWidth  = 720.0f;
    float                        _PlateMaxHeightFraction = 0.66f;
};

// ====================================================================================================================
