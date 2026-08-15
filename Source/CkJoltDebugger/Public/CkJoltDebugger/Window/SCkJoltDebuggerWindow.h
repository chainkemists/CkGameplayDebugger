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
class STextBlock;
class UCk_Jolt_Subsystem;
class UWorld;

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

    auto HandleOutlinerRowSelected(TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot) -> void;
    auto HandleViewportBodyPicked(TOptional<uint64> InBodyKey) -> void;
    auto HandleGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;

    auto DoRefreshBodies(UWorld* InWorld) -> void;
    auto DoApplyPendingTarget(UWorld* InWorld) -> void;
    auto DoApplySelection(
        TOptional<FCkJoltDebugger_BodySnapshot> InSnapshot,
        ECkJoltDebugger_SelectionSource InSource) -> void;
    auto DoRefreshSelectionFacts() -> void;
    auto Get_Selection() const -> TOptional<FCkJoltDebugger_BodySnapshot> { return _Selection; }

    auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto BuildInWorldDrawToggles() const -> TSharedRef<SWidget>;
    auto BuildTargetGroup() -> TSharedRef<SWidget>;
    auto BuildCameraGroup() -> TSharedRef<SWidget>;
    auto BuildRenderGroup() -> TSharedRef<SWidget>;
    auto BuildPopulationGroup() -> TSharedRef<SWidget>;
    auto BuildLegendGroup() const -> TSharedRef<SWidget>;
    auto BuildStatRail() const -> TSharedRef<SWidget>;
    auto BuildRightRail() -> TSharedRef<SWidget>;

    auto MakePopulationToggle(
        FName InIconId,
        const FString& InLabel,
        const FString& InToolTip,
        TArray<ECk_Jolt_DebugDraw_ColorClass> InColorClasses) const -> TSharedRef<SWidget>;

    auto MakeSectionHeader(const FString& InText) const -> TSharedRef<SWidget>;
    auto MakeStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;

    TSharedPtr<FCkDebuggerModel_WorldSelector>  _WorldModel;
    TSharedPtr<SCkJoltDebugger_3dViewport>      _Viewport;
    TSharedPtr<FCk_Jolt_DebugDrawTarget>        _DebugDrawTarget;
    TSharedPtr<SCkJoltDebugger_OutlinerPanel>   _OutlinerPanel;
    TSharedPtr<SCkJoltDebugger_DetailPanel>     _DetailPanel;
    TSharedPtr<FCkDebug_ViewportPicker>         _ViewportPicker;

    FCkJoltDebugger_DataCollector _Collector;

    // The window's whole selection model: one snapshot, or nothing. Cleared through the same
    // session-invalidated path as the collector, because it carries the only other live handle here.
    TOptional<FCkJoltDebugger_BodySnapshot> _Selection;

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
