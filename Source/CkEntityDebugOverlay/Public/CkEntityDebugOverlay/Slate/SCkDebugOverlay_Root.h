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

    // Forwards to the child focus card.
    // bIsLocked: when true, renders a yellow ring around the card to indicate focus lock.
    auto Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked = false) -> void;

    // Rebuilds the canvas: one SCkDebugOverlay_WorldTag per entry.
    // Each entry carries a screen position (absolute viewport pixels, top-left origin),
    // label text, and distance-driven scale + opacity (B1).
    auto Update_WorldTags(const TArray<FCk_DebugOverlay_WorldTagInfo>& InTags) -> void;

    // Re-anchors / resizes the focus card. Cheap no-op when unchanged; rebuilds the
    // slot tree (re-using the existing child widgets) when anchor or width differ.
    auto Set_PlateLayout(ECk_DebugOverlay_PlateAnchor InAnchor, float InWidth) -> void;

private:
    auto DoRebuildLayout() -> void;

    // Builds the ultra-condensed near plate (name header + colored feature badges).
    auto DoBuild_NearPlate(const FCk_DebugOverlay_WorldTagInfo& InInfo) -> TSharedRef<SWidget>;

private:
    TSharedPtr<SCkDebugOverlay_FocusCard> _FocusCard;
    TSharedPtr<SConstraintCanvas>         _TagCanvas;

    ECk_DebugOverlay_PlateAnchor _PlateAnchor = ECk_DebugOverlay_PlateAnchor::TopRight;
    float                        _PlateWidth  = 720.0f;
};

// ====================================================================================================================
