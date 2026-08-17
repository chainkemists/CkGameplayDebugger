#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

#include "CkJoltDebugger/Data/CkJoltDebugger_DataCollector.h"

class FCkDebug_ViewportPicker;
class SCkJoltDebugger_3dViewport;
class SCkJoltDebugger_DetailPanel;
class SCkJoltDebugger_OutlinerPanel;
class SDockTab;
class SHorizontalBox;
class STextBlock;
class UCk_Jolt_Subsystem;
class UWorld;

namespace ck_jolt_debugger
{
    struct FPopulationGroup;
}

// --------------------------------------------------------------------------------------------------------------------
// Cached world-level Jolt stats, refreshed on the gated Tick and read by the value-row TAttribute lambdas.
// --------------------------------------------------------------------------------------------------------------------

struct FCkJoltDebugger_Stats
{
    bool    HasWorld = false;
    FString WorldLabel;

    bool    AsyncPhysics    = false;
    bool    ParallelPhysics = false;
    int32   ThreadCount     = 0;

    int32   NumBodies    = 0;
    int32   NumDynamic   = 0;
    int32   NumKinematic = 0;
    int32   NumStatic    = 0;
    int32   NumAwake     = 0;
    int32   NumAsleep    = 0;

    int32   NumCharacters = 0;

    int32   NumStaticActors = 0;
    int32   NumStaticBodies = 0;
    int32   NumUniqueShapes = 0;

    // ---- Simulation (P7-D51 / P6-D48) ----

    bool    IsPaused = false;
    float   LastStepMs = 0.0f;
    int32   ContactPairsLastStep = 0;

    int32   NumActiveRigidBodies = 0;
    int32   NumActiveSoftBodies  = 0;

    /*
     * The half of FCk_Jolt_DebugDraw_WorldStats the capture refreshes only every 30th capture, because
     * GetBodyStats walks every body and GetConstraints copies the constraint array. Rendered under a
     * "(sampled)" label so a lagging number reads as designed rather than as a bug (P5-D61/S10).
     */
    bool    HasSampledStats = false;
    int32   SampleAge = 0;

    int32   SampledNumBodies             = 0;
    int32   SampledMaxBodies             = 0;
    int32   SampledStaticBodies          = 0;
    int32   SampledDynamicBodies         = 0;
    int32   SampledActiveDynamicBodies   = 0;
    int32   SampledKinematicBodies       = 0;
    int32   SampledActiveKinematicBodies = 0;
    int32   SampledSoftBodies            = 0;
    int32   SampledConstraints           = 0;
};

// --------------------------------------------------------------------------------------------------------------------
// A route target that arrived before the collector had rows to match it against, together with the world it was
// resolved in. An entity id only means something inside its own registry, so a pending target whose world is not
// the one being inspected is dropped rather than matched against whatever shares its id here.
// --------------------------------------------------------------------------------------------------------------------

struct FCkJoltDebugger_PendingTarget
{
    FCk_Entity             Entity;
    TWeakObjectPtr<UWorld> World;
};

// --------------------------------------------------------------------------------------------------------------------
// Where a selection came from. Only the two USER-driven sources re-broadcast to the rest of the suite; the
// rest already originated outside this window and echoing them would loop.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkJoltDebugger_SelectionSource : uint8
{
    Outliner,
    Viewport,

    /*
     * A Ctrl+click in the viewport. User-driven like Viewport — it re-broadcasts — but the outliner is NOT
     * re-stamped from it: the additive pick drove the outliner's own store to build the set in the first
     * place, and pushing a single-row select back would collapse the multi-selection it just made.
     */
    ViewportAdditive,

    /*
     * The outliner PRUNED itself: a selected row's entity left the world and the panel dropped it (promoting a
     * survivor to primary). The window's set has to follow or the highlight and the isolation keep naming dead
     * body keys — but nobody selected anything, so this neither re-broadcasts nor re-stamps the panel that
     * already holds the state (P7-D71/F8).
     */
    OutlinerPrune,

    External
};

// --------------------------------------------------------------------------------------------------------------------
// CK Jolt Physics Debugger window — placed inside a NomadTab. A preview-world viewport is the main pane; the
// world-level stats are its right-hand rail.
//
// The window owns ONE FCk_Jolt_DebugDrawTarget bound to the viewport's preview world, registered against the
// SELECTED game world's UCk_Jolt_Subsystem. Registration, demand, and the game-world weak refs are the only
// things Tick touches — nothing here rebuilds Slate structure.
//
// Scrunch-free: the widget tree is built ONCE in Construct; every value is a TAttribute lambda reading _Stats
// or the target's own state.
// --------------------------------------------------------------------------------------------------------------------

class SCkJoltDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    /** The nomad tab this window lives in — the key the entity-target route and the chrome's Sync action use. */
    static const FName TabId;

    /*
     * The ONE predicate that says "this window can show that entity". Shared by the module's entity-target
     * route and the viewport picker's TargetFilter, so a route that opens the tab and a pick that previews
     * an entity can never disagree about what is targetable.
     */
    static auto Is_JoltDebuggerEntity(const FCk_Handle& InCandidate) -> bool;

    SLATE_BEGIN_ARGS(SCkJoltDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /** Route entry point. The collector may not have run yet, so the target is applied on the next refresh. */
    auto TargetEntity(const FCk_Handle& InEntity) -> void;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Jolt Physics Debugger")); }

    /*
     * What the preference restore actually LANDED on the facility target and on this window. A read surface,
     * not a control: asserting that a preference is unchanged after Construct proves only that the restore did
     * not overwrite it, never that it arrived anywhere (P7-D71/F10).
     */
    auto Get_TargetDrawFlags() const -> ECk_Jolt_DebugDrawFlags;
    auto Get_TargetColorMode() const -> ECk_Jolt_DebugDrawColorMode;
    auto Get_TargetRenderMode() const -> ECk_Jolt_DebugDraw_RenderMode;
    auto Get_TargetDirectionGlyphScale() const -> float;
#if WITH_DEV_AUTOMATION_TESTS
    /** The render button's one click path, exposed so its three-state cycle and persistence are pinned. */
    auto Cycle_RenderModeForTest() -> void;
#endif
    auto Get_IsolateActive() const -> bool { return _IsolateActive; }
    auto Get_FollowSelection() const -> bool { return _FollowSelection; }
    auto Get_ShowGrid() const -> bool { return _ShowGrid; }
    auto Get_DirectionGlyphScale() const -> float { return _DirectionGlyphScale; }
    auto Get_NumGridLines() const -> int32;

    virtual ~SCkJoltDebuggerWindow() override;

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto DoCreateDebugDrawTarget() -> void;
    auto DoRefreshStats(UWorld* InWorld) -> void;
    auto DoSyncDebugDrawTarget(UWorld* InWorld) -> void;
    auto DoUnregisterDebugDrawTarget() -> void;

    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleSessionInvalidated() -> void;
    auto HandleTabForegrounded(TSharedPtr<SDockTab> InForegrounded, TSharedPtr<SDockTab> InBackgrounded) -> void;

    auto HandleOutlinerRowSelected(
        TOptional<FCkJoltDebugger_BodySnapshot> InPrimary,
        TArray<FCkJoltDebugger_BodySnapshot> InAll) -> void;
    auto HandleViewportBodyPicked(TOptional<uint64> InBodyKey, bool InIsAdditive) -> void;
    auto HandleViewportBodyHovered(TOptional<uint64> InBodyKey) -> void;
    auto HandleContactSelected(uint64 InOtherBodyKey) -> void;
    auto HandleGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;

    auto DoRefreshBodies(UWorld* InWorld) -> void;
    auto DoApplyPendingTarget(UWorld* InWorld) -> void;
    /** The single-selection convenience: the primary IS the whole set. */
    auto DoApplySelection(
        TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot,
        ECkJoltDebugger_SelectionSource InSource) -> void;

    /** The ONE writer of the selection model. Primary first — the facility samples the first key alone. */
    auto DoApplySelectionSet(
        TArray<FCkJoltDebugger_BodySnapshot> InAll,
        TOptional<FCkJoltDebugger_BodySnapshot> InPrimary,
        ECkJoltDebugger_SelectionSource InSource) -> void;

    auto DoRefreshSelectionFacts() -> void;
    auto Get_Selection() const -> const TOptional<FCkJoltDebugger_BodySnapshot>& { return _Selection; }
    auto Get_SelectionFacts() const -> const FCkJoltDebugger_SelectionFacts& { return _SelectionFacts; }

    /** Row for a drawn body key, through the collector's row list and its baked-body owner index. */
    auto TryFind_RowForBodyKey(uint64 InBodyKey) const -> const FCkJoltDebugger_BodySnapshot*;

    auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto BuildInWorldDrawToggles() const -> TSharedRef<SWidget>;
    auto BuildTargetGroup() -> TSharedRef<SWidget>;
    auto BuildSimGroup() -> TSharedRef<SWidget>;
    auto BuildSelectionGroup() -> TSharedRef<SWidget>;
    auto BuildDrawGroup() -> TSharedRef<SWidget>;
    auto BuildPopulationGroup() -> TSharedRef<SWidget>;
    auto BuildLegendGroup() -> TSharedRef<SWidget>;
    auto BuildStatRail() const -> TSharedRef<SWidget>;
    auto BuildRightRail() -> TSharedRef<SWidget>;

    auto MakePopulationToggle(
        const ck_jolt_debugger::FPopulationGroup& InGroup) const -> TSharedRef<SWidget>;

    /** The selected world's Jolt subsystem, or null whenever there is nothing inspectable to command. */
    auto Get_SelectedJoltSubsystem() const -> UCk_Jolt_Subsystem*;
    auto Get_HasCommandableWorld() const -> bool;

    auto DoApplyPopulationVisibility() -> void;

    auto Get_ColorMode() const -> ECk_Jolt_DebugDrawColorMode;
    auto Set_ColorMode(ECk_Jolt_DebugDrawColorMode InColorMode) -> void;
    auto Cycle_RenderMode() -> void;
    auto Set_RenderMode(ECk_Jolt_DebugDraw_RenderMode InRenderMode) -> void;

    auto Set_DrawFlag(ECk_Jolt_DebugDrawFlags InFlag, bool InIsEnabled) -> void;

    /*
     * The legend names the classes of the CURRENT colour mode, so a mode change has to re-populate it. This is
     * the one Slate structure this window rebuilds after Construct — from the mode control's own handler, never
     * from Tick.
     */
    auto DoRebuildLegend() -> void;

    auto HandleTogglePause() -> void;
    auto HandleStepOnce() -> void;
    auto HandleToggleIsolate() -> void;

    auto Set_IsolateActive(bool InIsActive) -> void;
    auto Set_FollowSelection(bool InIsActive) -> void;

    /*
     * Isolation follows the SELECTION, so it is re-applied every time the selection changes while it is on.
     * An empty selection clears it rather than isolating nothing — a blank viewport with a lit toggle is
     * indistinguishable from a broken one.
     */
    auto DoApplyIsolation() -> void;

    /*
     * Whether the selected world is the authority. The debug drag is the only sim-MUTATING thing this module
     * does, and a drag on a client moves a body the server corrects on the next replication.
     */
    auto Get_IsAuthorityWorld() const -> bool;

    /*
     * The Ctrl+LMB press, carrying the key the pick resolved and the exact world point the ray met it at
     * (P7-D70/i). The drag opens HERE, on the press: the grab point is the hit point, so nothing has to wait
     * for a capture to sample a bounds centre. Refused when the press hit nothing, when the picked body is not
     * the primary the selection just became, or when the world is not the authority.
     */
    auto HandleDragArm(TOptional<uint64> InPickedKey, FVector InGrabPointWorld) -> void;
    auto HandleDragRay(FVector InRayOrigin, FVector InRayDirection) -> void;
    auto HandleDragPlaneShift(float InDirection) -> void;

    /** Idempotent: a second call with no drag live is a no-op, which is what lets teardown paths just call it. */
    auto HandleDragRelease() -> void;

    /** Push the drag line into its retained External sub-channel — only when it MOVED (P5-D61/S3). */
    auto DoUpdateDragLine() -> void;

    /*
     * The selected probe's overlaps, in their own retained External sub-channel (P8-D56 + P5-D61/S3). Re-pushed
     * only when the overlap SET changes, because Get_CurrentOverlaps copies a whole TSet and the channel is
     * retained — a per-tick re-push would pay for the copy and make the result flicker for nothing.
     */
    auto DoUpdateProbeResults() -> void;

    auto Set_ShowProbeResults(bool InIsEnabled) -> void;
    /** Applies a live glyph preview; persistence is deferred until the spinbox interaction commits. */
    auto Set_DirectionGlyphScale(float InScale) -> void;

    auto Set_ShowGrid(bool InIsEnabled) -> void;

    /*
     * The ground grid, pushed ONCE into its retained External sub-channel (P8-D59 + P5-D61/S3). It never
     * moves, so the capture re-emitting it every pass is the whole of its per-frame cost; the toggle either
     * pushes it or clears the channel.
     */
    auto DoApplyGrid() -> void;

    /*
     * Re-derive the selection from the outliner after a refresh PRUNED it. The panel is the store; the window
     * is the sink — and a sink still naming a body that left the world isolates on a key that draws nothing.
     */
    auto DoSyncSelectionFromOutliner() -> void;

    /*
     * The facility's health scan, armed from the window's own policy: the runaway bar is a per-user preference
     * and KillZ belongs to the selected world's AWorldSettings. CkJolt owns neither, which is why they are
     * pushed rather than read there (P8-D57).
     */
    auto DoApplyProblemThresholds(UWorld* InWorld) -> void;

    /*
     * ConstraintReferenceFrames while a constraint is selected. Jolt's reference-frame draw is ALL-CONSTRAINTS
     * (PhysicsSystem::DrawConstraintReferenceFrame takes no constraint), so selecting one turns the whole
     * world's frames on rather than that constraint's — documented, not faked (P5-D61/S8).
     */
    auto DoApplyConstraintReferenceFrames() -> void;

    /** The primary selection's own label, which the viewport paints whatever the Labels draw flag says. */
    auto DoUpdateViewportLabels() -> void;

    /** Restore the per-user preferences into the target and the camera. Runs once, at the end of Construct. */
    auto DoApplySavedPreferences() -> void;

    auto MakeSectionHeader(const FString& InText) const -> TSharedRef<SWidget>;
    auto MakeStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;
    auto MakeSampledStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;

    /** A throttled count, or `--` while the capture has never taken a sample. */
    auto Get_SampledStatText(int32 InValue) const -> FText;

    TSharedPtr<FCkDebuggerModel_WorldSelector>  _WorldModel;
    TSharedPtr<SCkJoltDebugger_3dViewport>      _Viewport;
    TSharedPtr<FCk_Jolt_DebugDrawTarget>        _DebugDrawTarget;
    TSharedPtr<SCkJoltDebugger_OutlinerPanel>   _OutlinerPanel;
    TSharedPtr<SCkJoltDebugger_DetailPanel>     _DetailPanel;
    TSharedPtr<FCkDebug_ViewportPicker>         _ViewportPicker;
    TSharedPtr<SHorizontalBox>                  _LegendBox;

    FCkJoltDebugger_DataCollector _Collector;

    // The window's selection model: the PRIMARY, plus the whole selected set with the primary first.
    // Cleared through the same session-invalidated path as the collector, because they carry the only
    // other live handles here.
    TOptional<FCkJoltDebugger_BodySnapshot> _Selection;
    TArray<FCkJoltDebugger_BodySnapshot>    _SelectionAll;

    // What the FACILITY knows about the primary, refreshed on the gated Tick. Kept beside the snapshot
    // rather than inside it: the outliner copies a snapshot per row per pass, and three sample structs
    // plus a contacts array on every one of them would be paid for by every row to serve the one selected.
    FCkJoltDebugger_SelectionFacts _SelectionFacts;

    bool _IsolateActive   = false;
    bool _FollowSelection = false;

    // The live drag: set on the Ctrl+LMB press that opened it, unset the moment it is released. One value, not
    // two — the press now carries its own grab point, so there is no window in which a drag is armed but not
    // yet begun (P7-D70/i).
    TOptional<uint64> _DragBodyKey;
    FVector           _DragPlanePoint  = FVector::ZeroVector;
    FVector           _DragPlaneNormal = FVector::ForwardVector;

    // The subsystem the drag BEGAN on (P8-D73). The release has to reach THAT one: a world change ends the drag
    // after the selector has already re-pointed, so asking the selector again would end nothing and leave the
    // old world's body on its spring. Weak — that world can die while this window lives.
    TWeakObjectPtr<UCk_Jolt_Subsystem> _DragSubsystem;

    // Last drag line pushed into the External channel, so it is re-pushed only when it actually moved.
    TOptional<FVector> _DragLineGrab;
    TOptional<FVector> _DragLineAnchor;

    bool _ShowProbeResults = false;

    float _DirectionGlyphScale = 1.0f;

    bool _ShowGrid = true;

    // A cheap digest of what the probe-results channel currently holds: the overlap set's own contents, so the
    // channel is rebuilt when they change and left alone when they do not. Unset means the channel is empty.
    TOptional<uint32> _ProbeResultsSignature;

    TOptional<FCkJoltDebugger_PendingTarget> _PendingTarget;

    // The subsystem the target is currently registered with. Weak: the game world dies while this window lives.
    TWeakObjectPtr<UCk_Jolt_Subsystem> _RegisteredSubsystem;

    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _SelectionSyncHandle;

    // Tab backgrounding detaches this widget's content, so Tick stops firing and the last-seen demand would
    // latch ON forever. The foreground broadcast is the only push signal that survives that.
    FDelegateHandle _TabForegroundedHandle;

    FCkJoltDebugger_Stats _Stats;
};

// --------------------------------------------------------------------------------------------------------------------
