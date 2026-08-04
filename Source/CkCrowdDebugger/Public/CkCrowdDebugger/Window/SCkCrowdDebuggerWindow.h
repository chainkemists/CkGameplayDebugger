#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_ViewModel;

class SCkCrowdDebugger_NavmeshStatusPanel;
class SCkCrowdDebugger_AgentListPanel;
class SCkCrowdDebugger_AgentDetailPanel;
class SCkCrowdDebugger_StatsPanel;
class SCkCrowdDebugger_EventLogPanel;
class SCkCrowdDebugger_3dViewport;
class UWorld;

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
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override
	{ return FText::FromString(TEXT("CK Crowd Debugger")); }

	virtual ~SCkCrowdDebuggerWindow();

private:
	auto BuildToolbar() -> TSharedRef<SWidget>;
	auto Refresh_VoxelSnapshot(UWorld* InSelectedWorld) -> void;
	auto Get_VoxelSnapshotBuildParams() const -> ck::voxelnav::FDebugSnapshotBuildParams;
	auto Get_VoxelSourceLabel() const -> FText;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

	TSharedPtr<SCkCrowdDebugger_NavmeshStatusPanel> _NavmeshStatusPanel;
	TSharedPtr<SCkCrowdDebugger_AgentListPanel>     _AgentListPanel;
	TSharedPtr<SCkCrowdDebugger_AgentDetailPanel>   _AgentDetailPanel;
	TSharedPtr<SCkCrowdDebugger_StatsPanel>         _StatsPanel;
	TSharedPtr<SCkCrowdDebugger_EventLogPanel>      _EventLogPanel;
	TSharedPtr<SCkCrowdDebugger_3dViewport>         _ViewportPanel;

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
};

// --------------------------------------------------------------------------------------------------------------------
