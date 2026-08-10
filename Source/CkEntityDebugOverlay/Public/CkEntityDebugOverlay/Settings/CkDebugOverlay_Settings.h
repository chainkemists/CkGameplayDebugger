#pragma once

#include "Engine/DeveloperSettings.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "InputCoreTypes.h"

#include "CkDebugOverlay_Settings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ck On-Screen Debugger"))
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UCk_DebugOverlay_Settings();

    virtual FName GetCategoryName() const override { return TEXT("Ck"); }

    // ---- Layouts ----

    // Inline layout definitions; highest-priority when resolving a layout tag.
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<FCk_DebugOverlay_Layout> Layouts;

    // Additional layouts loaded from data assets (lower priority than inline Layouts).
    UPROPERTY(Config, EditAnywhere, Category="Layouts")
    TArray<TSoftObjectPtr<class UCk_DebugOverlay_Layout_PDA>> LayoutAssets;

    // Layout that is active when the overlay first opens.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(Categories="Ck.OnScreenDebugger.Layout"))
    FGameplayTag StartingLayout;

    // Viewport corner/edge the focus card (plate) is anchored to. Default is top-left: the
    // overlay suppresses engine on-screen debug text while active (restored on deactivate),
    // so the plate owns that corner and can use the tall PlateMaxHeightFraction budget.
    // Applied live — changing it during PIE re-anchors the plate on the next overlay tick.
    UPROPERTY(Config, EditAnywhere, Category="General")
    ECk_DebugOverlay_PlateAnchor PlateAnchor = ECk_DebugOverlay_PlateAnchor::TopLeft;

    // Width (Slate units) of the main overlay plate. Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="240.0", ClampMax="1600.0"))
    float PlateWidth = 720.0f;

    // Maximum height of the card strip (primary + pinned cards) as a fraction of the
    // viewport height. Content grows as needed and CLIPS at the cap (the overlay is
    // hit-test invisible, so a scrollbar would be uninteractable). Applied live.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0.2", ClampMax="0.95"))
    float PlateMaxHeightFraction = 0.66f;

    // Uniform scale applied to the ECS-diamond marker billboards (screen-space icons,
    // base 28px). Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0.2", ClampMax="5.0"))
    float DiamondScale = 1.0f;

    // Font-size multiplier for the main overlay plate's text. Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0.5", ClampMax="2.0"))
    float PlateFontScale = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="1", ClampMax="32"))
    int32 FocusCardMaxRows = 18;

    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="1", ClampMax="16"))
    int32 FocusCardMaxRowsPerSection = 4;

    // ---- Focus-card distance LOD (opt-in) ----

    // Trim the focus card by camera distance to the FOCUSED entity: the full card up close,
    // then header + the top budget-ordered sections, then a single flow line of one
    // severity-colored token per provider. Everything a tier drops is still counted into the
    // card's existing "+N more" / omission-summary affordances — the trim is visible, never
    // silent. OFF by default: the shipped card renders identically at every distance, and this
    // only starts changing that once you opt in (Project Settings, or the Style Lab preview).
    UPROPERTY(Config, EditAnywhere, Category="Focus Card LOD")
    bool bEnableFocusCardDistanceLod = false;

    // Camera distance (cm) at which the card steps down to the SUMMARY tier. Sits inside the
    // MarkerMaxDist candidate cull (3000 by default) so all three tiers are reachable without
    // retuning the declutter range. A small enter/exit gap around this boundary stops the card
    // flickering between tiers.
    UPROPERTY(Config, EditAnywhere, Category="Focus Card LOD",
        meta=(ClampMin="0.0", EditCondition="bEnableFocusCardDistanceLod"))
    float FocusCardSummaryDist = 1200.0f;

    // Camera distance (cm) at which the card steps down to the PILL tier (entity ref + one
    // token per provider). Values below FocusCardSummaryDist are treated as equal to it.
    UPROPERTY(Config, EditAnywhere, Category="Focus Card LOD",
        meta=(ClampMin="0.0", EditCondition="bEnableFocusCardDistanceLod"))
    float FocusCardPillDist = 2400.0f;

    // Collapse rows that are identical (same field + same value) within one provider across
    // the focus entity's whole lifetime subtree into a single row badged "xN". Off = every
    // descendant contributes its own row (one "Label" row per sub-entity, etc.), which eats
    // the focus-card row budget with repeats.
    UPROPERTY(Config, EditAnywhere, Category="General")
    bool bMergeDuplicateRows = true;

    // State-machine state-name depth — same rule as the CkSmDebugger graph: show the
    // last N underscore-segments of the state class name ("Ck_SmTest_Complex_State_Chase"
    // → depth 1 → "Chase"). 0 = full (dotted) name.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0", ClampMax="8"))
    int32 SmStateNameDepth = 1;

    // How many levels of nested sub-state-machines the StateMachine provider descends
    // into when rendering the focus card / compact token. A visited-set guard protects
    // against malformed cycles regardless of this value. 0 = top-level only.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0", ClampMax="8"))
    int32 SmMaxRecursionDepth = 3;

    // Show the always-on keyboard-hints strip (built from the live key bindings below)
    // in the corner opposite the focus card. Toggle the full legend with HelpKey /
    // `ck.DebugOverlay.Help`.
    UPROPERTY(Config, EditAnywhere, Category="General")
    bool ShowKeyHints = true;

    // ---- Attributes (focus-card volume control) ----

    // Case-insensitive SUBSTRING patterns matched against attribute label tags
    // ("Health" catches "Attr.Health.Max"). Meaning depends on the mode bool below.
    // Empty list = no filtering in EITHER mode. Editable live from the ECS Debugger's
    // picker-toolbar popover ("Overlay Attributes").
    UPROPERTY(Config, EditAnywhere, Category="Attributes")
    TArray<FString> AttributeFilterPatterns;

    // false: show ONLY attributes matching a pattern. true: show all EXCEPT matching.
    UPROPERTY(Config, EditAnywhere, Category="Attributes")
    bool bAttributeFilterIsExclusion = false;

    // Whether InAttributeName passes the filter above (all five attribute providers
    // consult this per row).
    static auto Get_PassesAttributeFilter(const FString& InAttributeName) -> bool;

    // ---- World Tags (B1 — distance-scaled / faded / culled pills) ----

    // Distance below which pills appear at full size (unscaled).
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float NearDist = 600.0f;

    // Distance at which pills reach MinScale. Lerp from NearDist to FarDist.
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float FarDist = 4000.0f;

    // Minimum scale factor applied to pills at FarDist and beyond.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0.1", ClampMax="1.0"))
    float MinScale = 0.5f;

    // Distance at which pill opacity begins fading toward MinOpacity (0.15).
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float FadeStartDist = 3000.0f;

    // Hard cull distance: pills beyond this range are not emitted at all.
    UPROPERTY(Config, EditAnywhere, Category="World Tags")
    float MaxDist = 5000.0f;

    // Hard cull distance for the in-world ECS diamond markers AND the candidate set they
    // share (markers == links == plates == focusable candidates). Entities beyond this
    // range of the camera are not gathered at all — the primary declutter knob.
    // 0 = unlimited (no marker distance cull).
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0.0"))
    float MarkerMaxDist = 3000.0f;

    // Maximum characters of the entity NAME shown on a world-tag near-plate before it is
    // truncated with an ellipsis (the trailing "[id]" is never truncated). 0 = unlimited.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0", ClampMax="128"))
    int32 MaxWorldTagNameChars = 24;

    // NOTE: input keybinds + co-located radii + the double-tap window now live in the per-USER
    // editor settings class UCk_DebugOverlay_InputSettings (below) so each developer can rebind
    // without touching shared project config.

    // ---- Co-located fan-out (gradual, distance-driven) ----

    // Camera distance (cm) at/below which a co-located cluster is fanned out to MAXIMUM.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0.0"))
    float FanFullDist = 1000.0f;

    // Camera distance (cm) at/above which a co-located cluster is fully collapsed (no fan —
    // the plates sit on top of each other, "looking like one entity"). Between FanFullDist
    // and FanFadeDist the fan spread is lerped, so it opens up as you approach. Default spans
    // most of the marker range so the fan is visible at typical debug distances; raise the
    // marker cull range (MarkerMaxDist) and lower this if you want far clusters to collapse.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="0.0"))
    float FanFadeDist = 6000.0f;

    // Maximum horizontal spacing (px) between fanned plates at full fan.
    UPROPERTY(Config, EditAnywhere, Category="World Tags", meta=(ClampMin="16.0", ClampMax="400.0"))
    float FanMaxSpacing = 110.0f;
};

// --------------------------------------------------------------------------------------------------------------------
// Per-USER editor settings for the on-screen debugger's input gestures. Stored in
// EditorPerProjectUserSettings (per-user, NOT committed) and shown under
// Editor → Editor Preferences (GetContainerName == "Editor"), so each developer can rebind the
// gestures independently of the shared project config.
// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="Ck On-Screen Debugger (Input)"))
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_InputSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName()  const override { return TEXT("Ck"); }
    virtual FName GetContainerName() const override { return TEXT("Editor"); }

    // Tap twice quickly to PIN / unpin the focused entity (side-by-side card).
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey LockKey = EKeys::LeftShift;

    // Tap twice quickly to open / select the focused entity in the CK ECS Debugger.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey EcsDebuggerFocusKey = EKeys::LeftControl;

    // Tap twice quickly to cycle the focus through co-located entities (soft preference, not a
    // hard lock; wraps to auto-follow past the last). A NON-modifier key by default — a lone
    // modifier like Alt triggers the OS/Slate menu chrome (cursor appears, viewport defocuses).
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey CycleCoLocatedKey = EKeys::V;

    // Tap twice quickly to release ALL pinned cards at once (also `ck.DebugOverlay.UnpinAll`, or
    // double-tap the pin key on an entity to unpin just it).
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey UnpinAllKey = EKeys::BackSpace;

    // Tap twice quickly to cycle the ACTIVE LAYOUT (same step as `ck.DebugOverlay.Layout.Next`;
    // the card's corner chip shows which layout is live). A NON-modifier key by default, same
    // reason as CycleCoLocatedKey.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey CycleLayoutKey = EKeys::L;

    // Optional: tap twice quickly to toggle the full keyboard-hints legend (also
    // `ck.DebugOverlay.Help`). Default unbound.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey HelpKey;

    // Per-USER memory of the layout the quick-switcher last selected, re-seeded on the next
    // session. It lives here rather than next to the layout definitions because this is the
    // overlay's only per-user config slot (EditorPerProjectUserSettings): the project class is
    // Config=Game/DefaultConfig and a runtime gesture must never dirty shared project config.
    // Unset / unknown falls back to the project's StartingLayout.
    UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(Categories="Ck.OnScreenDebugger.Layout"))
    FGameplayTag LastActiveLayout;

    // World-space radius (cm) for the co-located cycle's connected-component flood.
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="10.0", ClampMax="1000.0"))
    float CoLocatedRadius = 100.0f;

    // Screen-space radius (px) for treating on-screen candidates as one co-located cluster
    // (drives the fan-out i/N and the focus-card i/N).
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="4.0", ClampMax="256.0"))
    float CoLocatedScreenRadius = 36.0f;

    // Maximum interval (seconds) between two taps to count as a double-tap.
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="0.05", ClampMax="1.0"))
    float LockDoubleTapWindowSeconds = 0.3f;
};

// --------------------------------------------------------------------------------------------------------------------
