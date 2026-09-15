#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_ViewModel;
class FCkDebug_ViewportPicker;

class SCkCrowdDebugger_NavmeshStatusPanel;
class SCkCrowdDebugger_AgentListPanel;
class SCkCrowdDebugger_AgentDetailPanel;
class SCkCrowdDebugger_StatsPanel;
class SCkCrowdDebugger_EventLogPanel;
class SCkCrowdDebugger_3dViewport;
class SCkDebug_WindowChrome;
class SButton;
class SComboButton;
class SBox;
class FCkUiView;
class UWorld;
struct FCkCrowdDebuggerLifecycleTestAccess;
struct FCkCrowdDebuggerAuthoredShellTestAccess;

enum class ECkCrowdDebugger_VoxelSource : uint8
{
	Auto,
	LivePie,
	RetainedSnapshot,
	EditorPreview
};

// --------------------------------------------------------------------------------------------------------------------

class CKCROWDDEBUGGER_API SCkCrowdDebuggerWindow : public SCkDebugger_WindowBase
{
public:
	static const FName WindowId;

	SLATE_BEGIN_ARGS(SCkCrowdDebuggerWindow) {}
#if WITH_DEV_AUTOMATION_TESTS
		SLATE_ARGUMENT(FString, TestResourceDirectory)
#endif
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	auto TargetEntity(const FCk_Handle& InEntity) -> void;

	// The ONE definition of "an entity this debugger lists" — shared by the
	// module's FCkDebug_EntityTargetRoute and the window's viewport picker.
	static auto Is_CrowdDebuggerEntity(const FCk_Handle& InCandidate) -> bool;

	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override
	{ return FText::FromString(TEXT("Crowd")); }

	virtual ~SCkCrowdDebuggerWindow();
	auto Release_Presentation() -> void;

protected:
	virtual auto OnStyleRevisionChanged() -> void override;

private:
	friend struct FCkCrowdDebuggerLifecycleTestAccess;
	friend struct FCkCrowdDebuggerAuthoredShellTestAccess;
	auto BuildNativeShell() -> TSharedRef<SWidget>;
	auto BuildAuthoredShell() -> void;
	auto PollAuthoredShell(double InCurrentTime) -> void;
	auto MountAuthoredShell() -> bool;
	auto DetachFallbackPorts() -> void;
	auto RestoreFallbackPorts() -> void;
	auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
	auto BuildMenuActions() -> TSharedRef<SWidget>;
	auto Refresh_VoxelSnapshot(UWorld* InSelectedWorld) -> void;
	auto Get_VoxelSnapshotBuildParams() const -> ck::voxelnav::FDebugSnapshotBuildParams;
	auto Get_VoxelSourceLabel() const -> FText;
	auto HandleWorldChanged(UWorld* InWorld) -> void;
	auto HandleSessionInvalidated() -> void;
	auto HandleFrameSelectedAgentRequested() -> void;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

	// Shared viewport picker (CkDebuggerCommon), specialized to crowd agents.
	TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;

	TSharedPtr<SCkCrowdDebugger_NavmeshStatusPanel> _NavmeshStatusPanel;
	TSharedPtr<SCkCrowdDebugger_AgentListPanel>     _AgentListPanel;
	TSharedPtr<SCkCrowdDebugger_AgentDetailPanel>   _AgentDetailPanel;
	TSharedPtr<SCkCrowdDebugger_StatsPanel>         _StatsPanel;
	TSharedPtr<SCkCrowdDebugger_EventLogPanel>      _EventLogPanel;
	TSharedPtr<SCkCrowdDebugger_3dViewport>         _ViewportPanel;
	TSharedPtr<SCkDebug_WindowChrome> _Chrome;
	TSharedPtr<SBox> _AuthoredShellHost;
	TSharedPtr<FCkUiView> _AuthoredShellView;
	TSharedPtr<SBox> _FallbackNavHost;
	TSharedPtr<SBox> _FallbackAgentsHost;
	TSharedPtr<SBox> _FallbackStatsHost;
	TSharedPtr<SBox> _FallbackEventsHost;
	TSharedPtr<SBox> _FallbackPreviewHost;
	TSharedPtr<SBox> _FallbackDetailHost;
	FString _AuthoredShellLoadFailure;
	double _NextAuthoredShellPollSeconds = 0.0;
	bool _UsingNativeShellFallback = true;
#if WITH_DEV_AUTOMATION_TESTS
	FString _TestResourceDirectory;
#endif
	TSharedPtr<SComboButton> _DiagnosticsCombo, _SourceCombo, _NavigationCombo, _CrowdCombo, _QueuesCombo, _AvoidanceCombo;
	TArray<TSharedPtr<SWidget>> _ComboMenuRoots;
	TSharedPtr<SButton> _LivePieSourceButton;

	ECkCrowdDebugger_VoxelSource _VoxelSource = ECkCrowdDebugger_VoxelSource::Auto;
	TOptional<ck::voxelnav::FDebugSnapshot> _RetainedVoxelSnapshot;
	TSharedPtr<const TArray<ck::voxelnav::FDebugSnapshot>> _LastEditorVoxelPublication;
	FString _VoxelSourceStatus = TEXT("VoxelNav: waiting for a source");
	double _NextVoxelRefreshTime = 0.0;
	bool _VoxelRefreshRequested = true;
	bool _ShowVoxelVolume = true;
	bool _ShowVoxelChunks = true;
	bool _ShowVoxelMergedFree = true;
	bool _ShowVoxelRawFree = false;
	bool _ShowVoxelOccupied = false;
	bool _ShowVoxelPortals = true;
	bool _ShowVoxelDirtyRepair = true;
	bool _ShowQueues = true;
	bool _ShowAvoidanceVolumes = true;
	TOptional<FCk_Entity> _PendingTarget;
	FDelegateHandle _WorldChangedHandle;
	FDelegateHandle _SessionInvalidatedHandle;
	FDelegateHandle _FrameSelectedAgentHandle;
	bool _PresentationReleased = false;
};

// --------------------------------------------------------------------------------------------------------------------
