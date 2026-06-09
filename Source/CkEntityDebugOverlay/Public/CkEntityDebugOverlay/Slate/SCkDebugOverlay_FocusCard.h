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
    auto Set_Model(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow) -> void;

private:
    // Converts ECk_DebugOverlay_Severity to an ECkDebug_Tone for pills / text.
    static auto Severity_To_Tone(ECk_DebugOverlay_Severity InSeverity) -> ECkDebug_Tone;

    // Returns the flash alpha multiplier for a field (1.0 if not flashing).
    static auto Get_FlashAlpha(double InLastChanged, double InNow) -> float;

    // Root container rebuilt on every Set_Model call.
    TSharedPtr<SVerticalBox> _ContentBox;
};

// ====================================================================================================================
