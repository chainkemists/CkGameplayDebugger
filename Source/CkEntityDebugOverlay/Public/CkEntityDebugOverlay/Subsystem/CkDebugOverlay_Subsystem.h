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
// Shared in-world entity marker preview. Gated with the implementation: the module's
// CkDebuggerCommon dependency is UncookedOnly and absent in Shipping, where the entire
// marker-driving code below is compiled out anyway.
#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"
// Global Slate input pre-processor — keeps double-tap gestures alive while ejected.
#include "CkEntityDebugOverlay/Input/CkDebugOverlay_InputProcessor.h"
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
//   2. Gather_Candidates — enumerate all transform-bearing entities that have
//      at least one provider willing to serve them.
//   3. Compute FViewpoint from the player camera / camera manager.
//   4. Pick_Best to find the focus entity (unless locked).
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

#if WITH_CK_DEBUG_OVERLAY

private:
    // ---- Activation / deactivation ----
    auto DoActivate()   -> void;
    auto DoDeactivate() -> void;

    // ---- Per-frame work (FTSTicker callback, returns true = keep ticking) ----
    auto DoTick(float InDeltaSeconds) -> bool;

    // ---- Helpers ----
    auto Resolve_ActiveWorld()  const -> UWorld*;
    auto Resolve_ActiveLayout() const -> const FCk_DebugOverlay_Layout*;

    /** Enumerate all transform-bearing entities that have at least one capable provider.
     *  Non-const: rebuilds the shared _Markers snapshot (markers/links/candidates are
     *  the same set — what you see is what you can focus).
     *  InCullOrigin: when set (a real viewpoint exists), entities farther than
     *  Settings->MarkerMaxDist from it are dropped from the whole set (diamond declutter). */
    auto Gather_Candidates(
        UWorld*                                              InWorld,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const TOptional<FVector>&                            InCullOrigin,
        TArray<FCk_Handle>&                                  OutHandles,
        TArray<ck_debugoverlay::FCandidate>&                 OutCandidates) -> void;

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

    // Focus lock state. INDEX_NONE = auto-pick via Pick_Best.
    int32 _LockedCandidateIndex = INDEX_NONE;
    bool  _FocusLocked          = false;

    // Soft co-located preference set by the cycle key (NOT a hard lock — no ring). When valid
    // and still on-screen + co-located with the auto-pick, the focus prefers this entity;
    // auto-clears when it leaves the cluster. FCk_Handle so it survives candidate-index churn.
    FCk_Handle _PreferredCoLocated;

    // Candidate handle list from the last frame (for Next/Prev cycling).
    TArray<FCk_Handle> _LastFrameCandidates;

    // Active layout index into Settings->Layouts.
    int32 _ActiveLayoutIndex = INDEX_NONE;

    // B2 — screen-space ECS-diamond marker billboards via the shared entity-marker
    // preview (FCkDebug_EntityMarkers in CkDebuggerCommon — same implementation as
    // the ECS Debugger's viewport picker). Snapshot is built each DoTick; consumed
    // by DoDrawMarkers (which fires per-viewport from the debug-draw service).
    auto DoDrawMarkers(UCanvas* InCanvas, APlayerController* InPC) -> void;

    FCkDebug_EntityMarkers _Markers;

    // Entity numbers whose markers are suppressed this tick (locally possessed
    // pawn's entities — a marker there would sit permanently at screen center).
    TSet<uint32> _MarkerSuppressed;

    // Focused candidate's entity number (drawn emphasized). MAX_uint32 = none.
    uint32 _FocusedEntityNum = MAX_uint32;

    FDelegateHandle _DebugDrawHandle_Game;
    FDelegateHandle _DebugDrawHandle_Editor;

    // True while the user is ejected from PIE (F8) and the editor camera drives the
    // view — focus picking follows it; PC-projected world tags are suppressed.
    bool _ViewpointIsEjected = false;

    // B3 — Double-tap lock detection.
    // Tracks the timestamp of the most recent LockKey press for double-tap detection.
    double _LastLockKeyPressTime = -1.0;

    // Double-tap detection for EcsDebuggerFocusKey (focus entity in the ECS Debugger).
    double _LastEcsFocusKeyPressTime = -1.0;

    // Double-tap detection for CycleCoLocatedKey (cycle through co-located entities).
    double _LastCycleKeyPressTime = -1.0;

    // Double-tap detection for UnpinAllKey / HelpKey.
    double _LastUnpinAllKeyPressTime = -1.0;
    double _LastHelpKeyPressTime     = -1.0;

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

    // Throttle for the ~1/sec marker diagnostics log line in DoTick.
    double _LastMarkerLogTime = -1.0;

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
