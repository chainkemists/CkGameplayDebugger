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

    // Viewport corner/edge the focus card (plate) is anchored to. Default is top-right so the
    // plate doesn't fight with engine on-screen debug text (which uses the top-left).
    // Applied live — changing it during PIE re-anchors the plate on the next overlay tick.
    UPROPERTY(Config, EditAnywhere, Category="General")
    ECk_DebugOverlay_PlateAnchor PlateAnchor = ECk_DebugOverlay_PlateAnchor::TopRight;

    // Width (Slate units) of the main overlay plate. Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="240.0", ClampMax="1600.0"))
    float PlateWidth = 720.0f;

    // Uniform scale applied to the ECS-diamond marker billboards (screen-space icons,
    // base 28px). Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0.2", ClampMax="5.0"))
    float DiamondScale = 1.0f;

    // Font-size multiplier for the main overlay plate's text. Applied live during PIE.
    UPROPERTY(Config, EditAnywhere, Category="General", meta=(ClampMin="0.5", ClampMax="2.0"))
    float PlateFontScale = 1.0f;

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

    // ---- Input (B3 — double-tap lock) ----

    // Key to tap twice quickly to toggle focus lock.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey LockKey = EKeys::LeftShift;

    // Key to tap twice quickly to open/select the focused entity in the CK ECS Debugger.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey EcsDebuggerFocusKey = EKeys::LeftControl;

    // Key to tap twice quickly to cycle focus through entities co-located with the
    // current focus (within CoLocatedRadius). Locks focus onto each in turn.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey CycleCoLocatedKey = EKeys::LeftAlt;

    // World-space radius (cm) within which entities count as co-located for cycling.
    // Fallback only — the on-screen fan-out and cycle now group by screen-space overlap
    // (CoLocatedScreenRadius) so "what you see fanned == what you cycle".
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="10.0", ClampMax="1000.0"))
    float CoLocatedRadius = 100.0f;

    // Screen-space radius (pixels) within which on-screen candidates are treated as one
    // co-located cluster — used to drive the hover fan-out (i/N badges) and the cycle key.
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="4.0", ClampMax="256.0"))
    float CoLocatedScreenRadius = 36.0f;

    // Optional key: tap twice quickly to release ALL pinned cards at once
    // (also available as `ck.DebugOverlay.UnpinAll`). Default unbound.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey UnpinAllKey;

    // Optional key: tap twice quickly to toggle the full keyboard-hints legend
    // (also available as `ck.DebugOverlay.Help`). Default unbound.
    UPROPERTY(Config, EditAnywhere, Category="Input")
    FKey HelpKey;

    // Maximum interval (seconds) between two taps to count as a double-tap.
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(ClampMin="0.05", ClampMax="1.0"))
    float LockDoubleTapWindowSeconds = 0.3f;
};

// --------------------------------------------------------------------------------------------------------------------
