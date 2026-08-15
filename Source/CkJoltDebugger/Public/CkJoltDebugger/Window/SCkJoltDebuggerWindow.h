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
    auto BuildCameraGroup() -> TSharedRef<SWidget>;
    auto BuildRenderGroup() -> TSharedRef<SWidget>;
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

    auto HandleDragArm() -> void;
    auto HandleDragRay(FVector InRayOrigin, FVector InRayDirection) -> void;
    auto HandleDragPlaneShift(float InDirection) -> void;
    auto HandleDragRelease() -> void;

    /** Push the drag line into its retained External sub-channel — only when it MOVED (P5-D61/S3). */
    auto DoUpdateDragLine() -> void;

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

    // The live drag. _DragBodyKey is armed on the Ctrl+LMB press; _IsDragBegun only becomes true once the
    // facility's sample has given the grab point its DEPTH, which lands one capture after the press.
    TOptional<uint64> _DragBodyKey;
    bool              _IsDragBegun = false;
    FVector           _DragPlanePoint  = FVector::ZeroVector;
    FVector           _DragPlaneNormal = FVector::ForwardVector;

    // Last drag line pushed into the External channel, so it is re-pushed only when it actually moved.
    TOptional<FVector> _DragLineGrab;
    TOptional<FVector> _DragLineAnchor;

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
