#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"  // ECk_DebugOverlay_Severity
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"      // ECkDebug_Tone

struct FCk_DebugOverlay_RenderStyle;
class  FCk_DebugOverlay_History;

// ====================================================================================================================
// Ultra-density focus card for the on-screen entity debug overlay.
//
// Call Set_Model() to rebuild all slots from a fresh model snapshot.
// The widget is hit-test invisible — it sits over the viewport without
// blocking user input.
// ====================================================================================================================

class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_FocusCard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugOverlay_FocusCard) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Rebuilds all child slots from the supplied model snapshot.
    // Must be called from the game thread.
    // bIsLocked: amber ring (focus lock). bIsPinned: cyan ring (a pinned card). Pinned wins.
    // InCoLocatedIndex/Count: when Count > 1, a small "i/N" badge is shown in the header so
    // you know which of several co-located entities this card is (1-based index).
    auto Set_Model(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked        = false,
        bool                                bIsPinned        = false,
        int32                               InCoLocatedIndex = INDEX_NONE,
        int32                               InCoLocatedCount = 0) -> void;

    // Stable, visually-distinct color per provider — used for the provider chip
    // fill, the field-chip tint, and the near-plate feature badges.
    static auto Get_ProviderColor(const FGameplayTag& InProviderTag) -> FLinearColor;

    // Width the section wrap-boxes wrap at. MUST be the card's inner content width:
    // the card rebuilds its SWrapBoxes every tick, so UseAllottedSize never gets a
    // Tick to sync PreferredSize (default 100px) — desired height would reserve
    // phantom wrapped lines, rendering as big empty gaps between sections.
    auto Set_WrapWidth(float InWidth) -> void { _WrapWidth = InWidth; }

private:
    // Converts ECk_DebugOverlay_Severity to an ECkDebug_Tone for pills / text.
    static auto Severity_To_Tone(ECk_DebugOverlay_Severity InSeverity) -> ECkDebug_Tone;

    // Returns the flash alpha multiplier for a field (1.0 if not flashing).
    static auto Get_FlashAlpha(double InLastChanged, double InNow) -> float;

    // Outer lock-ring border — tinted amber/yellow when the focus is locked.
    TSharedPtr<class SBorder> _LockFrame;

    // Root container rebuilt on every Set_Model call.
    TSharedPtr<SVerticalBox> _ContentBox;

    // Inner content width the section wrap-boxes wrap at (see Set_WrapWidth).
    float _WrapWidth = 700.0f;
};

// ====================================================================================================================
