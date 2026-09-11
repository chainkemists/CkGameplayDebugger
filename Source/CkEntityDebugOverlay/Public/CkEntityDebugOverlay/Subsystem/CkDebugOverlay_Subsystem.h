#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HAL/IConsoleManager.h"

// These headers are small enough to include unconditionally and are needed for
// the member types / method signatures below.
#include "CkEcs/Handle/CkHandle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"

#if WITH_CK_DEBUG_OVERLAY
// Global Slate input pre-processor — keeps double-tap gestures alive while ejected.
#include "CkEntityDebugOverlay/Input/CkDebugOverlay_InputProcessor.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_SelectionSession.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#endif

#include "CkDebugOverlay_Subsystem.generated.h"

class APlayerController;
class UCanvas;

// ====================================================================================================================
// UCk_DebugOverlay_Subsystem
//
// LocalPlayer subsystem that drives the on-screen entity debug overlay.
// Implementation is gated by WITH_CK_DEBUG_OVERLAY so shipping builds carry
// no overhead. The UCLASS declaration is unconditional so UHT always sees it.
//
// Per-frame tick (when active):
//   1. Resolve active world (own LP world or manual PIE override).
//   2. Gather live ECS topology, resolve meaningful roots and spatial anchors.
//   3. Project through the active player/ejected camera.
//   4. Update the identity-stable selection session and handle deliberate actions.
//   5. Build_Model — collect provider sections for the focus entity.
//   6. Push_ToRoot — forward model + history + world tags to the Slate widget.
// ====================================================================================================================

UCLASS(NotBlueprintable)
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_Subsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_DebugOverlay_Subsystem);

public:
    // --- USubsystem ---
    virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* InNewPlayerController) override;

#if WITH_CK_DEBUG_OVERLAY

private:
    struct FSelectionEntry
    {
        FCk_Handle Entity;
        ck_debugoverlay::selection_session::FCandidateSelection Candidate;
        FCk_Handle AnchorEntity;
    };

    auto Reset_SelectionSession() -> void;
    auto Update_Selection(UWorld* InWorld, const ck_debugoverlay::FViewpoint& InViewpoint,
        TArray<FCk_Handle>& OutHandles, TArray<ck_debugoverlay::FCandidate>& OutCandidates) -> void;
    auto Refresh_SelectionSnapshot(bool InSelectBest) -> void;
    auto Cycle_Selection(int32 InDirection) -> void;
    auto Set_FamilyVisible(bool InVisible) -> void;
    auto CanHandle_SelectionInput() const -> bool;
    auto DoCmd_Select() -> void;
    auto DoCmd_Settings() -> void;
    auto DoCmd_Family() -> void;
    auto Close_SelectionSettings() -> void;
    auto Get_SelectionStatus(bool InCompact = false) const -> FText;
    auto Update_SelectionHud() -> void;
    auto TryGet_SelectionPosition(const FSelectionEntry& InEntry) const -> TOptional<FVector>;

    TWeakObjectPtr<UWorld> _SelectionWorld;
    TArray<ck_debugoverlay::selection_session::FNode> _SelectionNodes;
    TMap<uint32, FCk_Handle> _SelectionHandles;
    TArray<FSelectionEntry> _WorldSelection;
    TArray<FSelectionEntry> _FamilySelection;
    FCk_Handle _SelectionRoot;
    FCk_Handle _SelectionAimTarget;
    FCk_Handle _SelectionCycleTarget;
    FCk_Handle _SelectionLockedEntity;
    ck_debugoverlay::selection_session::FViewpoint _SelectionView;
    ck::DebugViewportView::FProjection _SelectionProjection;
    bool _SelectionProjectionValid = false;
    bool _FamilyVisible = false;
    uint32 _SelectionRevision = 0;
    TSharedPtr<class SCkDebugOverlay_SelectionHud> _SelectionHud;
    TSharedPtr<class SCkDebugOverlay_SelectionPanel> _SelectionPanel;
    TSharedPtr<SWidget> _SelectionPanelHost;
    TWeakPtr<SWidget> _SelectionPriorFocus;
    TWeakObjectPtr<APlayerController> _SelectionInputController;
    bool _SelectionPriorCursor = false;
    FDelegateHandle _SelectionSessionInvalidated;
    FDelegateHandle _SelectionWorldInvalidated;
    TUniquePtr<FAutoConsoleCommand> _Cmd_Select;
    TUniquePtr<FAutoConsoleCommand> _Cmd_Settings;
    TUniquePtr<FAutoConsoleCommand> _Cmd_Family;

    // ---- Activation / deactivation ----
    auto DoActivate()   -> void;
    auto DoDeactivate() -> void;

    // ---- Per-frame work (FTSTicker callback, returns true = keep ticking) ----
    auto DoTick(float InDeltaSeconds) -> bool;

    // ---- Helpers ----
    auto Resolve_ActiveWorld()  const -> UWorld*;
    auto Resolve_ActiveLayout() const -> const FCk_DebugOverlay_Layout*;

    /** Layout quick-switcher: clamp + adopt InIndex and remember it per-USER (the input
     *  settings class is the overlay's only per-user config slot; the project class is shared
     *  DefaultConfig and must never be dirtied by a runtime gesture). */
    auto Set_ActiveLayoutIndex(int32 InIndex) -> void;

    /** Text for the focus card's corner switcher chip ("LAYOUT AI 2/6  L x2"). Empty when no
     *  layouts are configured. */
    auto Get_LayoutSwitcherLabel() const -> FText;

    /** Populate the focus-entity model from all capable providers. */
    auto Build_Model(
        const FCk_Handle&                                    InFocusEntity,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        double                                               InNow,
        FCk_DebugOverlay_EntityModel&                        OutModel) -> void;

    /** Forward model, history, and world-tags to SCkDebugOverlay_Root. */
    auto Push_ToRoot(
        const FCk_DebugOverlay_EntityModel&                  InModel,
        const FCk_DebugOverlay_Layout&                       InLayout,
        const TArray<FCk_Handle>&                            InCandidateHandles,
        const TArray<ck_debugoverlay::FCandidate>&           InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        APlayerController*                                   InPC,
        double                                               InNow) -> void;

    // ---- CVar / command callbacks ----
    auto OnCVar_MasterChanged(IConsoleVariable* InVar) -> void;
    auto DoCmd_Next()        -> void;
    auto DoCmd_Prev()        -> void;
    auto DoCmd_Lock()        -> void;
    auto DoCmd_Layout_Next() -> void;
    auto DoCmd_Layout_Prev() -> void;
    auto DoCmd_UnpinAll()    -> void;
    auto DoCmd_Help()        -> void;
    auto DoCmd_World(const TArray<FString>& InArgs, UWorld* InWorld) -> void;

private:
    // Providers — created once on first activation; stable for subsystem lifetime.
    TArray<TSharedPtr<ICk_DebugOverlay_Provider>> _Providers;

    // Viewport Slate widget.
    TSharedPtr<class SCkDebugOverlay_Root> _RootWidget;

    // FTSTicker registration.
    FTSTicker::FDelegateHandle _TickerHandle;

    // History: allocated on activation, reset on deactivation so stale records
    // don't persist across PIE stop/start.
    TUniquePtr<FCk_DebugOverlay_History> _History;

    // Hold Select to toggle the full-handle selection lock; data-card pins are independent.
    bool  _FocusLocked          = false;

    // Candidate handle list from the last frame (for Next/Prev cycling).
    TArray<FCk_Handle> _LastFrameCandidates;

    // Active layout index into Settings->Layouts.
    int32 _ActiveLayoutIndex = INDEX_NONE;

    FCk_Handle _FocusedEntity;

    // Last focus emitted by the opt-in continuous sync path. Full handle identity
    // includes the entity generation, so slot reuse still emits a new selection.
    FCk_Handle _LastSyncedEntity;

    // True while the user is ejected from PIE (F8) and the editor camera drives the
    // view — focus picking follows it; PC-projected world tags are suppressed.
    bool _ViewpointIsEjected = false;

    // B3 — Double-tap lock detection.
    // Tracks the timestamp of the most recent LockKey press for double-tap detection.
    double _LastLockKeyPressTime = -1.0;

    // Double-tap detection for EcsDebuggerFocusKey (focus entity in the ECS Debugger).
    double _LastEcsFocusKeyPressTime = -1.0;

    // Double-tap detection for UnpinAllKey / HelpKey.
    double _LastUnpinAllKeyPressTime = -1.0;
    double _LastHelpKeyPressTime     = -1.0;

    // Double-tap detection for CycleLayoutKey (layout quick-switcher).
    double _LastCycleLayoutKeyPressTime = -1.0;

    // Focus-card distance-LOD tier currently on screen. Carried across ticks because it IS the
    // hysteresis reference (Resolve_LodTier widens the boundary the card last crossed). Reset
    // with the rest of the session state on deactivation.
    ECk_DebugOverlay_LodTier _LodTier = ECk_DebugOverlay_LodTier::Full;

    // Global Slate input pre-processor — observes key-downs regardless of game-vs-editor
    // viewport focus so the double-tap gestures keep working while ejected (F8).
    TSharedPtr<FCkDebugOverlay_InputProcessor> _InputProcessor;

    // Pinned entities — each gets its own persistent live-updating card alongside the
    // primary (auto-following) focus card. Double-tap LockKey toggles a pin; the whole
    // set is cleared by UnpinAllKey / `ck.DebugOverlay.UnpinAll`. Stored as handles
    // (stable across frames; invalid ones are pruned each tick).
    TArray<FCk_Handle> _PinnedEntities;

    // Whether the full keyboard-hints legend is shown (toggled by HelpKey / Help command).
    bool _ShowFullLegend = false;

    // Engine on-screen debug text state saved at activation (suppressed while the overlay
    // is active — the plate owns the top-left corner) and restored on deactivation.
    bool _PriorOnScreenMessagesEnabled = true;

    // PIE world override: INDEX_NONE = use own LP world.
    int32 _WorldOverrideIndex = INDEX_NONE;

    // Console: raw pointer to the static TAutoConsoleVariable (not owned).
    TAutoConsoleVariable<int32>* _CVar_Master          = nullptr;
    // True if this instance registered the console commands (first LP subsystem wins).
    bool                         _bIsPrimaryConsoleOwner = false;

    TUniquePtr<FAutoConsoleCommand>                _Cmd_Next;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_Prev;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_Lock;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_Layout_Next;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_Layout_Prev;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_UnpinAll;
    TUniquePtr<FAutoConsoleCommand>                _Cmd_Help;
    TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> _Cmd_World;

#endif // WITH_CK_DEBUG_OVERLAY
};
