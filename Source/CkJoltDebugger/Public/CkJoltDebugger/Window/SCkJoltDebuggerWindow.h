#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

class SCkJoltDebugger_3dViewport;
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

    SLATE_BEGIN_ARGS(SCkJoltDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Jolt Physics Debugger")); }

    virtual ~SCkJoltDebuggerWindow() override;

private:
    auto DoCreateDebugDrawTarget() -> void;
    auto DoRefreshStats(UWorld* InWorld) -> void;
    auto DoSyncDebugDrawTarget(UWorld* InWorld) -> void;
    auto DoUnregisterDebugDrawTarget() -> void;

    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleSessionInvalidated() -> void;
    auto HandleTabForegrounded(TSharedPtr<SDockTab> InForegrounded, TSharedPtr<SDockTab> InBackgrounded) -> void;

    auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto BuildInWorldDrawToggles() const -> TSharedRef<SWidget>;
    auto BuildTargetGroup() -> TSharedRef<SWidget>;
    auto BuildCameraGroup() -> TSharedRef<SWidget>;
    auto BuildRenderGroup() -> TSharedRef<SWidget>;
    auto BuildPopulationGroup() -> TSharedRef<SWidget>;
    auto BuildLegendGroup() const -> TSharedRef<SWidget>;
    auto BuildStatRail() const -> TSharedRef<SWidget>;

    auto MakePopulationToggle(
        FName InIconId,
        const FString& InLabel,
        const FString& InToolTip,
        TArray<ECk_Jolt_DebugDraw_ColorClass> InColorClasses) const -> TSharedRef<SWidget>;

    auto MakeSectionHeader(const FString& InText) const -> TSharedRef<SWidget>;
    auto MakeStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<SCkJoltDebugger_3dViewport>     _Viewport;
    TSharedPtr<FCk_Jolt_DebugDrawTarget>       _DebugDrawTarget;

    // The subsystem the target is currently registered with. Weak: the game world dies while this window lives.
    TWeakObjectPtr<UCk_Jolt_Subsystem> _RegisteredSubsystem;

    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _SessionInvalidatedHandle;

    // Tab backgrounding detaches this widget's content, so Tick stops firing and the last-seen demand would
    // latch ON forever. The foreground broadcast is the only push signal that survives that.
    FDelegateHandle _TabForegroundedHandle;

    FCkJoltDebugger_Stats _Stats;
};

// --------------------------------------------------------------------------------------------------------------------
