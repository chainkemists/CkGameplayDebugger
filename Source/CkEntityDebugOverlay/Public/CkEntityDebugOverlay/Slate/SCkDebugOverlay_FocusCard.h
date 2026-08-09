#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"  // ECk_DebugOverlay_Severity
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"      // ECk_Tone

struct FCk_DebugOverlay_RenderStyle;
class  FCk_DebugOverlay_History;

// One legend row on the focus card: colored abbrev pill + muted expanded provider name.
// Deduplicated per PROVIDER, not per rendered section — subtree aggregation emits one section
// per (provider x source), which used to repeat "[LABL] Label" once per lifetime descendant.
struct FCk_DebugOverlay_LegendEntry
{
    FString      Abbrev;
    FString      FullName;
    FLinearColor Color = FLinearColor::White;

    // How many rendered sections this provider contributed.
    int32        SectionCount = 1;
};

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

    // Legend rows for the sections that Set_Model actually draws, one entry per unique
    // provider (SectionCount records how many of its sections contributed). Sections the
    // render loop skips — no rows and no omission summary — contribute nothing.
    // This is the LegendMode::Deduped builder (the default).
    static auto Build_LegendEntries(
        const TArray<FCk_DebugOverlay_Section>& InSections) -> TArray<FCk_DebugOverlay_LegendEntry>;

    // The LegendMode::PerSection builder: one entry per RENDERED SECTION, no dedup, so a
    // provider that contributed three sections is listed three times (SectionCount stays 1).
    // Skips exactly the same sections the render loop and the deduped builder skip.
    static auto Build_LegendEntries_PerSection(
        const TArray<FCk_DebugOverlay_Section>& InSections) -> TArray<FCk_DebugOverlay_LegendEntry>;

    // Width the section wrap-boxes wrap at. MUST be the card's inner content width:
    // the card rebuilds its SWrapBoxes every tick, so UseAllottedSize never gets a
    // Tick to sync PreferredSize (default 100px) — desired height would reserve
    // phantom wrapped lines, rendering as big empty gaps between sections.
    auto Set_WrapWidth(float InWidth) -> void { _WrapWidth = InWidth; }

private:
    // Converts ECk_DebugOverlay_Severity to an ECk_Tone for pills / text.
    static auto Severity_To_Tone(ECk_DebugOverlay_Severity InSeverity) -> ECk_Tone;

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
