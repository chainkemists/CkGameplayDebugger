#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Containers/Array.h"
#include "Math/Vector2D.h"

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"

struct FCk_DebugOverlay_EntityModel;
struct FCk_DebugOverlay_RenderStyle;
class  FCk_DebugOverlay_History;
class  SCkDebugOverlay_FocusCard;
class  SCkDebugOverlay_WorldTag;
class  SConstraintCanvas;

// ====================================================================================================================
// Viewport root for the on-screen entity debug overlay.
//
// Owns:
//   - A fixed top-left focus card (Ultra density).
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
    auto Set_FocusCardContent(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow) -> void;

    // Rebuilds the canvas: one SCkDebugOverlay_WorldTag per entry.
    // Each entry carries a screen position (absolute viewport pixels, top-left origin),
    // label text, and distance-driven scale + opacity (B1).
    auto Update_WorldTags(const TArray<FCk_DebugOverlay_WorldTagInfo>& InTags) -> void;

private:
    TSharedPtr<SCkDebugOverlay_FocusCard> _FocusCard;
    TSharedPtr<SConstraintCanvas>         _TagCanvas;
};

// ====================================================================================================================
