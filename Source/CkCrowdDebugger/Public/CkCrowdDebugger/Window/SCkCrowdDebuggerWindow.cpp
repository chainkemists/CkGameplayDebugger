#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Settings/CkCrowdDebuggerSettings.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"
#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"
#include "CkCrowdDebugger/Commands/CkCrowdDebugger_PathNetworkCommand.h"
#include "CkDebuggerCommon/Viewport/CkDebug3dWorldPing.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkVoxelNav/Volume/CkVoxelNavVolume_Utils.h"
#if WITH_EDITOR
#include "CkVoxelNavEditor/Preview/CkVoxelNavPreview_EdMode.h"
#include "CkVoxelNavEditor/Preview/CkVoxelNavPreview_EditorSubsystem.h"
#endif

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_window
{
	constexpr auto LeftRailStatsMinHeight         = 152.0f;
	constexpr auto LeftRailSplitterHandleSize     = 5.0f;
	constexpr auto LeftRailMinHeight =
		(LeftRailStatsMinHeight / 0.14f)
		+ (3.0f * LeftRailSplitterHandleSize);
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkCrowdDebuggerWindow::WindowId{TEXT("CkCrowdDebugger")};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Is_CrowdDebuggerEntity(const FCk_Handle& InCandidate) -> bool
{
	return ck::IsValid(InCandidate) && InCandidate.Has<ck::FFragment_CrowdAgent_Params>();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Construct(const FArguments& InArgs) -> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkCrowdDebugger_ViewModel>();
	_WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

	// Shared viewport picker, specialized to crowd agents: only entities with
	// FFragment_CrowdAgent_Params (plus their owner chain up to the NPC
	// representative) are previewed and pickable. The pick routes through this
	// module's registered entity-target route, which resolves the lineage and
	// re-fronts the tab with the agent selected.
	_ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
	{
		auto PickerParams = FCkDebug_ViewportPicker::FParams{};
		PickerParams.Get_TargetWorld =
			[WeakWorld = TWeakPtr<FCkDebuggerModel_WorldSelector>(_WorldModel)]() -> UWorld*
			{
				const auto Pinned = WeakWorld.Pin();
				return Pinned.IsValid() ? Pinned->Get_SelectedWorld() : nullptr;
			};
		PickerParams.TargetFilter =
			[](const FCk_Handle& InCandidate) { return Is_CrowdDebuggerEntity(InCandidate); };
		PickerParams.OnEntityPicked =
			[](const FCk_Handle& InPicked)
			{
				ck::DebugSelectionSync::Broadcast(InPicked, TEXT("CrowdDebugger"));
				FCkDebug_EntityTargetRegistry::Get().TryOpenAndTarget(WindowId, InPicked);
			};
		_ViewportPicker->Construct(MoveTemp(PickerParams));
	}
	_WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(
		this, &SCkCrowdDebuggerWindow::HandleWorldChanged);
	_SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
		this, &SCkCrowdDebuggerWindow::HandleSessionInvalidated);

	_NavmeshStatusPanel = SNew(SCkCrowdDebugger_NavmeshStatusPanel).ViewModel(_ViewModel);
	_AgentListPanel     = SNew(SCkCrowdDebugger_AgentListPanel).ViewModel(_ViewModel);
	_AgentDetailPanel   = SNew(SCkCrowdDebugger_AgentDetailPanel).ViewModel(_ViewModel);
	_StatsPanel         = SNew(SCkCrowdDebugger_StatsPanel).ViewModel(_ViewModel);
	_EventLogPanel      = SNew(SCkCrowdDebugger_EventLogPanel).ViewModel(_ViewModel);
	const auto WeakViewModel = TWeakPtr<FCkCrowdDebugger_ViewModel>{_ViewModel};
	_ViewportPanel      = SNew(SCkCrowdDebugger_3dViewport)
		.OnAgentPicked_Lambda([WeakViewModel](int32 InAgentIndex)
		{
			const auto ViewModel = WeakViewModel.Pin();
			if (NOT ViewModel.IsValid())
			{ return; }

			const auto& Agents = ViewModel->Get_AllAgents();
			if (NOT Agents.IsValidIndex(InAgentIndex))
			{ return; }

			const auto& SelectedHandle = Agents[InAgentIndex].Handle;
			if (ck::Is_NOT_Valid(SelectedHandle))
			{ return; }

			ViewModel->Set_SelectedHandle(SelectedHandle);
			ck::DebugSelectionSync::Broadcast(SelectedHandle, TEXT("CrowdDebugger"));
		})
		// RTS-style command: right-click a destination to drive the selected agent there.
		// Commanding auto-arms the debug override, so no separate "Take Control" click is needed.
		.OnWorldCommanded_Lambda([this, WeakViewModel](const FVector& InDestination)
		{
			const auto ViewModel = WeakViewModel.Pin();
			if (NOT ViewModel.IsValid())
			{ return; }

			auto SelectedHandle = ViewModel->Get_SelectedHandle();
			auto Agent = UCk_Utils_CrowdAgent_UE::Cast(SelectedHandle);
			if (ck::Is_NOT_Valid(Agent))
			{ return; }

			auto Destination = FVector{};
			if (NOT ck::crowd_debugger::Try_IssueManualMove(Agent, InDestination, Destination))
			{ return; }

			// Both surfaces: the viewport ping is where the click happened, the world ping is for
			// whoever is watching the game. Neither can substitute for the other -- the viewport
			// renders its own preview world and never shows game-world actors.
			if (_ViewportPanel.IsValid())
			{ _ViewportPanel->Set_CommandPing(Destination); }

			ck::debug_3d::Draw_WorldCommandPing(
				UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Agent), Destination);
		});

	// The agent list broadcasts this to frame the picked agent. The 2D panel used to be the only
	// listener, so the request went nowhere once the viewport became 3D.
	_FrameSelectedAgentHandle = _ViewModel->OnFrameSelectedAgentRequested.AddSP(
		this, &SCkCrowdDebuggerWindow::HandleFrameSelectedAgentRequested);

	ChildSlot
	[
		SNew(SCkDebug_WindowChrome)
		.WindowId(WindowId)
		.ToolTabId(TEXT("CkCrowdDebugger"))
		.CommandGroups(BuildCommandGroups())
		.CommonActionsContent()
		[
			SNew(SCkDebug_ViewportPickerControls).Picker(_ViewportPicker).PickTooltip(FText::FromString(TEXT("Enter pick mode: click a crowd agent in the viewport to inspect it.\nOnly crowd-agent entities (and their owning NPC) are shown and pickable.")))
		]
		.Content()
		[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			// Left rail: navmesh status + agent list + stats + event log.
			+ SSplitter::Slot().Value(0.20f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
				+ SScrollBox::Slot()
				.FillSize(1.0f)
				.MinSize(ck_crowd_debugger_window::LeftRailMinHeight)
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)
					.PhysicalSplitterHandleSize(ck_crowd_debugger_window::LeftRailSplitterHandleSize)
					+ SSplitter::Slot()
					.Value(0.22f)
					[ SNew(SCkDebug_PaneHost) [ _NavmeshStatusPanel.ToSharedRef() ] ]
					+ SSplitter::Slot()
					.Value(0.46f)
					[ SNew(SCkDebug_PaneHost) [ _AgentListPanel.ToSharedRef() ] ]
					+ SSplitter::Slot()
					.Value(0.14f)
					[ SNew(SCkDebug_PaneHost) [ _StatsPanel.ToSharedRef() ] ]
					+ SSplitter::Slot()
					.Value(0.18f)
					[ SNew(SCkDebug_PaneHost) [ _EventLogPanel.ToSharedRef() ] ]
				]
			]
			// Center: the viewport, full height (the mockup's centerpiece).
			+ SSplitter::Slot().Value(0.52f)
			[
				SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer) [ _ViewportPanel.ToSharedRef() ]
			]
			// Right: agent detail + tuners + diagnostics, full height (no scrolling).
			+ SSplitter::Slot().Value(0.28f)
			[
				SNew(SCkDebug_PaneHost) [ _AgentDetailPanel.ToSharedRef() ]
			]
		]
		]
	];
}

auto SCkCrowdDebuggerWindow::HandleWorldChanged(UWorld*) -> void
{
	if (_ViewportPicker.IsValid())
	{ _ViewportPicker->Deactivate(); }

	_PendingTarget.Reset();
	if (_ViewModel.IsValid())
	{ _ViewModel->Reset_ForWorldChange(); }
	if (_ViewportPanel.IsValid())
	{ _ViewportPanel->Notify_WorldChanged(); }
	_VoxelRefreshRequested = true;
}

auto SCkCrowdDebuggerWindow::HandleSessionInvalidated() -> void
{
	if (_WorldModel.IsValid() && _WorldModel->Get_SelectedWorld() != nullptr)
	{
		_WorldModel->Set_SelectedWorld(nullptr);
		return;
	}

	HandleWorldChanged(nullptr);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::OnStyleRevisionChanged() -> void
{
	// Every panel here is attribute-bound and has already moved by the time this runs; the agent list
	// is the one surface with generated ROW widgets, whose STableRow style and per-row band index are
	// resolved at generation time.
	if (_AgentListPanel.IsValid())
	{ _AgentListPanel->Rebuild_ForStyleChange(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
	SCkDebugger_WindowBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (_ViewportPicker.IsValid() && _ViewportPicker->IsActive())
	{ _ViewportPicker->Tick(InDeltaTime); }

	if (NOT _ViewModel.IsValid())
	{ return; }

	// Resolve the inspected world via the shared selector. If none (editor idle,
	// no PIE), the data collector will early-out on a null world.
	_WorldModel->Ensure_AutoSelect();
	auto* World = _WorldModel->Get_SelectedWorld();

	_ViewModel->Tick(World, InDeltaTime);
	if (_PendingTarget.IsSet())
	{
		const auto Target = _PendingTarget.GetValue();
		_PendingTarget.Reset();
		if (_AgentListPanel.IsValid()) { _AgentListPanel->SelectEntityExternal(Target); }
	}

	if (_ViewportPanel.IsValid())
	{
		_ViewportPanel->Set_NavmeshTriangles(
			_ViewModel->Get_NavTriVerts(),
			_ViewModel->Get_NavGeometryRevision());
		_ViewportPanel->Set_AgentSnapshots(
			_ViewModel->Get_AllAgents(),
			_ViewModel->Get_SelectedHandle());
		_ViewportPanel->Set_PathNetworkRibbons(_ViewModel->Get_PathNetworkRibbons());
		_ViewportPanel->Set_QueueSnapshots(_ShowQueues ? _ViewModel->Get_Queues() : TArray<FCkCrowdDebugger_QueueSnapshot>{});
		_ViewportPanel->Set_AvoidanceVolumeSnapshots(
			_ShowAvoidanceVolumes ? _ViewModel->Get_AvoidanceVolumes() : TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>{});
	}

	if (_VoxelRefreshRequested || InCurrentTime >= _NextVoxelRefreshTime)
	{
		Refresh_VoxelSnapshot(World);
		_VoxelRefreshRequested = false;
		_NextVoxelRefreshTime = InCurrentTime + 0.25;
	}
}

auto SCkCrowdDebuggerWindow::TargetEntity(const FCk_Handle& InEntity) -> void
{
	if (ck::IsValid(InEntity)) { _PendingTarget = InEntity.Get_Entity(); }
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	struct FCrowdDebugger_ToggleDefinition
	{
		const TCHAR* _CVar = nullptr;
		const TCHAR* _Label = nullptr;
		const TCHAR* _Tooltip = nullptr;
	};

	// Read/write helpers for the two CkCrowd-side overlay CVars. The toolbar checkboxes call
	// these so any external `console set` is reflected on the next paint without us caching
	// state — the CVar is the single source of truth.
	auto Get_CVarBool(const TCHAR* InName) -> bool
	{
		const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
		return CVar != nullptr && CVar->GetInt() != 0;
	}

	auto Set_CVarBool(const TCHAR* InName, bool InValue) -> void
	{
		auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
		if (CVar != nullptr)
		{ CVar->Set(InValue ? 1 : 0, ECVF_SetByConsole); }
	}

	auto Make_CVarToggle(const FCrowdDebugger_ToggleDefinition& InDefinition) -> TSharedRef<SWidget>
	{
		return SNew(SCkDebug_ToggleSurface)
			.ToolTipText(FText::FromString(InDefinition._Tooltip))
			.IsOn_Lambda([Definition = InDefinition]() -> bool
			{
				return Get_CVarBool(Definition._CVar);
			})
			.OnStateChanged_Lambda([Definition = InDefinition](bool InIsOn)
			{
				Set_CVarBool(Definition._CVar, InIsOn);
			})
			[
				SNew(STextBlock).Text(FText::FromString(InDefinition._Label))
			];
	}

	auto Make_ControlGroup(const TCHAR* InTitle, TArray<FCrowdDebugger_ToggleDefinition> InToggles) -> TSharedRef<SWidget>
	{
		auto Content = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 7.0f, 8.0f, 3.0f)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(InTitle)
			];

		for (const auto& Toggle : InToggles)
		{
			Content->AddSlot().AutoHeight().Padding(8.0f, 2.0f)
			[
				Make_CVarToggle(Toggle)
			];
		}

		return Content;
	}

	auto Get_StatusRank(ck::voxelnav::EDebugSnapshotStatus InStatus) -> int32
	{
		using ck::voxelnav::EDebugSnapshotStatus;
		switch (InStatus)
		{
		case EDebugSnapshotStatus::Failed: return 6;
		case EDebugSnapshotStatus::MissingCook: return 5;
		case EDebugSnapshotStatus::StaleCook: return 4;
		case EDebugSnapshotStatus::Building: return 3;
		case EDebugSnapshotStatus::RuntimeOnly: return 2;
		case EDebugSnapshotStatus::Current: return 1;
		default: return 0;
		}
	}

	auto Get_StatusLabel(ck::voxelnav::EDebugSnapshotStatus InStatus) -> const TCHAR*
	{
		using ck::voxelnav::EDebugSnapshotStatus;
		switch (InStatus)
		{
		case EDebugSnapshotStatus::MissingCook: return TEXT("Missing Cook");
		case EDebugSnapshotStatus::StaleCook: return TEXT("Stale Cook");
		case EDebugSnapshotStatus::Building: return TEXT("Building");
		case EDebugSnapshotStatus::Current: return TEXT("Current");
		case EDebugSnapshotStatus::Failed: return TEXT("Failed");
		case EDebugSnapshotStatus::RuntimeOnly: return TEXT("Runtime Only");
		default: return TEXT("Unknown");
		}
	}

	auto Append_Layer(
		const ck::voxelnav::FDebugSnapshotLayerOutput& InSource,
		int32 InCap,
		ck::voxelnav::FDebugSnapshotLayerOutput& InOutCombined) -> void
	{
		InOutCombined._FilteredTotal += InSource._FilteredTotal;
		const auto Remaining = FMath::Max(0, InCap - InOutCombined._Cells.Num());
		const auto CopyCount = FMath::Min(Remaining, InSource._Cells.Num());
		for (auto Index = 0; Index < CopyCount; ++Index)
		{ InOutCombined._Cells.Emplace(InSource._Cells[Index]); }
		InOutCombined._Truncated |= InSource._Truncated || CopyCount < InSource._Cells.Num();
	}

	auto Combine_Snapshots(
		const TArray<ck::voxelnav::FDebugSnapshot>& InSnapshots,
		int32 InCellCap) -> TOptional<ck::voxelnav::FDebugSnapshot>
	{
		if (InSnapshots.IsEmpty())
		{ return {}; }

		auto Combined = ck::voxelnav::FDebugSnapshot{};
		Combined._Source = InSnapshots[0]._Source;
		Combined._Status = ck::voxelnav::EDebugSnapshotStatus::Current;
		Combined._SourceIdentity = FString::Printf(TEXT("%d VoxelNav volume(s)"), InSnapshots.Num());
		Combined._BuildProgress = 1.0f;
		Combined._IsBuilt = true;

		for (const auto& Snapshot : InSnapshots)
		{
			if (Get_StatusRank(Snapshot._Status) > Get_StatusRank(Combined._Status))
			{
				Combined._Status = Snapshot._Status;
				Combined._StatusMessage = Snapshot._StatusMessage;
			}

			if (Snapshot._AuthoredBounds.IsValid != 0)
			{ Combined._AuthoredBounds += Snapshot._AuthoredBounds; }
			if (Snapshot._NavigationBounds.IsValid != 0)
			{ Combined._NavigationBounds += Snapshot._NavigationBounds; }
			if (Snapshot._PendingDirtyBounds.IsValid != 0)
			{ Combined._PendingDirtyBounds += Snapshot._PendingDirtyBounds; }
			if (Snapshot._ActiveDirtyBounds.IsValid != 0)
			{ Combined._ActiveDirtyBounds += Snapshot._ActiveDirtyBounds; }

			Combined._SourceEpoch += Snapshot._SourceEpoch;
			Combined._SourceFingerprint = HashCombineFast(
				static_cast<uint32>(Combined._SourceFingerprint),
				static_cast<uint32>(Snapshot._SourceFingerprint));
			Combined._Generation = FMath::Max(Combined._Generation, Snapshot._Generation);
			Combined._BuildProgress = FMath::Min(Combined._BuildProgress, Snapshot._BuildProgress);
			Combined._IsBuilt &= Snapshot._IsBuilt;
			Combined._IsPartitioned |= Snapshot._IsPartitioned;
			Combined._Chunks.Append(Snapshot._Chunks);
			Combined._Portals.Append(Snapshot._Portals);
			Append_Layer(Snapshot._MergedFree, InCellCap, Combined._MergedFree);
			Append_Layer(Snapshot._RawFree, InCellCap, Combined._RawFree);
			Append_Layer(Snapshot._Occupied, InCellCap, Combined._Occupied);
		}

		return Combined;
	}

	auto MakeCVarToggle(
		FName InId,
		ECk_Icon InIconId,
		const TCHAR* InCVarName,
		const TCHAR* InLabel,
		const TCHAR* InToolTip) -> FCkDebug_IconToggleAction
	{
		const auto CVarName = FString{InCVarName};
		return FCkDebug_IconToggleAction{
			InId,
			InIconId,
			FText::FromString(InLabel),
			FText::FromString(InToolTip),
			TAttribute<bool>::CreateLambda([CVarName]() { return Get_CVarBool(*CVarName); }),
			FOnCkDebug_IconToggleChanged::CreateLambda([CVarName](bool InNewState)
			{
				Set_CVarBool(*CVarName, InNewState);
			})};
	}
}

auto SCkCrowdDebuggerWindow::Get_VoxelSnapshotBuildParams() const -> ck::voxelnav::FDebugSnapshotBuildParams
{
	auto Params = ck::voxelnav::FDebugSnapshotBuildParams{};
	Params._RequestedLayers = ck::voxelnav::EDebugSnapshotLayer::None;
	if (_ShowVoxelMergedFree)
	{ Params._RequestedLayers = Params._RequestedLayers | ck::voxelnav::EDebugSnapshotLayer::MergedFree; }
	if (_ShowVoxelRawFree)
	{ Params._RequestedLayers = Params._RequestedLayers | ck::voxelnav::EDebugSnapshotLayer::RawFree; }
	if (_ShowVoxelOccupied)
	{ Params._RequestedLayers = Params._RequestedLayers | ck::voxelnav::EDebugSnapshotLayer::Occupied; }
	Params._MaxCellsPerLayer = 10000;
	return Params;
}

auto SCkCrowdDebuggerWindow::Get_VoxelSourceLabel() const -> FText
{
	switch (_VoxelSource)
	{
	case ECkCrowdDebugger_VoxelSource::LivePie: return FText::FromString(TEXT("Live PIE"));
	case ECkCrowdDebugger_VoxelSource::RetainedSnapshot: return FText::FromString(TEXT("Retained"));
#if WITH_EDITOR
	case ECkCrowdDebugger_VoxelSource::EditorPreview: return FText::FromString(TEXT("Editor Preview"));
#endif
	default: return FText::FromString(TEXT("Auto Source"));
	}
}

auto SCkCrowdDebuggerWindow::Refresh_VoxelSnapshot(UWorld* InSelectedWorld) -> void
{
	const auto Params = Get_VoxelSnapshotBuildParams();
	auto Source = _VoxelSource;
	if (Source == ECkCrowdDebugger_VoxelSource::Auto)
	{
		const auto HasRuntimeWorld = InSelectedWorld != nullptr &&
			(InSelectedWorld->WorldType == EWorldType::PIE || InSelectedWorld->WorldType == EWorldType::Game);
#if WITH_EDITOR
		Source = HasRuntimeWorld ? ECkCrowdDebugger_VoxelSource::LivePie : ECkCrowdDebugger_VoxelSource::EditorPreview;
#else
		Source = HasRuntimeWorld ? ECkCrowdDebugger_VoxelSource::LivePie : ECkCrowdDebugger_VoxelSource::RetainedSnapshot;
#endif
	}

	auto Combined = TOptional<ck::voxelnav::FDebugSnapshot>{};
	if (Source == ECkCrowdDebugger_VoxelSource::LivePie)
	{
		auto Snapshots = TArray<ck::voxelnav::FDebugSnapshot>{};
		if (UCk_Utils_VoxelNavVolume_UE::TryBuild_DebugSnapshotsForWorld(InSelectedWorld, Params, Snapshots))
		{ Combined = Combine_Snapshots(Snapshots, Params._MaxCellsPerLayer); }

		if (Combined.IsSet())
		{
			_RetainedVoxelSnapshot = *Combined;
			_RetainedVoxelSnapshot->_Source = ck::voxelnav::EDebugSnapshotSource::RetainedSnapshot;
		}
	}
	else if (Source == ECkCrowdDebugger_VoxelSource::RetainedSnapshot)
	{
		Combined = _RetainedVoxelSnapshot;
	}
#if WITH_EDITOR
	else
	{
		auto* PreviewSubsystem = UCk_VoxelNavPreview_EditorSubsystem_UE::Get();
		if (PreviewSubsystem != nullptr)
		{
			if (_VoxelRefreshRequested)
			{ PreviewSubsystem->Request_RebuildAll(Params); }

			const auto Published = PreviewSubsystem->Get_RenderSnapshots();
			if (NOT _VoxelRefreshRequested && Published == _LastEditorVoxelPublication)
			{ return; }

			_LastEditorVoxelPublication = Published;
			if (Published.IsValid())
			{ Combined = Combine_Snapshots(*Published, Params._MaxCellsPerLayer); }
		}
	}
#endif

	if (NOT Combined.IsSet())
	{
		if (Source == ECkCrowdDebugger_VoxelSource::RetainedSnapshot)
		{ _VoxelSourceStatus = TEXT("VoxelNav Retained: no PIE snapshot captured yet"); }
		else if (Source == ECkCrowdDebugger_VoxelSource::LivePie)
		{ _VoxelSourceStatus = TEXT("VoxelNav Live PIE: no runtime volume"); }
#if WITH_EDITOR
		else
		{ _VoxelSourceStatus = TEXT("VoxelNav Editor Preview: place a Ck Voxel Nav Volume in the level"); }
#endif
		if (_ViewportPanel.IsValid())
		{ _ViewportPanel->Clear_VoxelNavSnapshot(); }
		return;
	}

	if (NOT _ShowVoxelVolume)
	{
		Combined->_AuthoredBounds = FBox{ForceInit};
		Combined->_NavigationBounds = FBox{ForceInit};
	}
	if (NOT _ShowVoxelChunks)
	{ Combined->_Chunks.Reset(); }
	if (NOT _ShowVoxelMergedFree)
	{ Combined->_MergedFree._Cells.Reset(); }
	if (NOT _ShowVoxelRawFree)
	{ Combined->_RawFree._Cells.Reset(); }
	if (NOT _ShowVoxelOccupied)
	{ Combined->_Occupied._Cells.Reset(); }
	if (NOT _ShowVoxelPortals)
	{ Combined->_Portals.Reset(); }
	if (NOT _ShowVoxelDirtyRepair)
	{
		Combined->_PendingDirtyBounds = FBox{ForceInit};
		Combined->_ActiveDirtyBounds = FBox{ForceInit};
	}

	const auto* SourceLabel = Source == ECkCrowdDebugger_VoxelSource::LivePie ? TEXT("Live PIE") :
		Source == ECkCrowdDebugger_VoxelSource::RetainedSnapshot ? TEXT("Retained") :
#if WITH_EDITOR
		TEXT("Editor Preview");
#else
		TEXT("Retained");
#endif
	_VoxelSourceStatus = FString::Printf(TEXT("VoxelNav %s: %s | merged %d/%d | raw %d/%d | occupied %d/%d"),
		SourceLabel,
		Get_StatusLabel(Combined->_Status),
		Combined->_MergedFree._Cells.Num(), Combined->_MergedFree._FilteredTotal,
		Combined->_RawFree._Cells.Num(), Combined->_RawFree._FilteredTotal,
		Combined->_Occupied._Cells.Num(), Combined->_Occupied._FilteredTotal);

	if (_ViewportPanel.IsValid())
	{ _ViewportPanel->Set_VoxelNavSnapshot(*Combined); }
}

auto SCkCrowdDebuggerWindow::BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>
{
	const auto MakeSourceButton = [this](const TCHAR* InLabel, ECkCrowdDebugger_VoxelSource InSource) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Text(FText::FromString(InLabel))
			.OnClicked_Lambda([this, InSource]() -> FReply
			{
				_VoxelSource = InSource;
				_VoxelRefreshRequested = true;
				return FReply::Handled();
			});
	};

	const auto MakeVoxelToggle = [this](const TCHAR* InLabel, const TCHAR* InTooltip, bool* InValue) -> TSharedRef<SWidget>
	{
		return SNew(SCkDebug_ToggleSurface)
			.ToolTipText(FText::FromString(InTooltip))
			.IsOn_Lambda([InValue]() -> bool
			{ return *InValue; })
			.OnStateChanged_Lambda([this, InValue](bool InIsOn)
			{
				*InValue = InIsOn;
				_VoxelRefreshRequested = true;
			})
			[ SNew(STextBlock).Text(FText::FromString(InLabel)) ];
	};

#if WITH_EDITOR
	const auto* AutoSourceLabel = TEXT("Auto (PIE when running, Editor otherwise)");
#else
	const auto* AutoSourceLabel = TEXT("Auto (Live when running, Retained otherwise)");
#endif
	const auto SourceMenu = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(AutoSourceLabel, ECkCrowdDebugger_VoxelSource::Auto) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Live PIE"), ECkCrowdDebugger_VoxelSource::LivePie) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Retained Snapshot"), ECkCrowdDebugger_VoxelSource::RetainedSnapshot) ]
#if WITH_EDITOR
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Editor Preview"), ECkCrowdDebugger_VoxelSource::EditorPreview) ]
#endif
		;

	const auto NavigationMenu = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f, 8.0f, 2.0f)
		[
			ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Voxel Navigation"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Volume Bounds"), TEXT("Show authored and navigation volume bounds."), &_ShowVoxelVolume) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Chunks"), TEXT("Show partition chunk bounds."), &_ShowVoxelChunks) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Merged Free Cells"), TEXT("Show the merged cells used by the path search. This is the performant default."), &_ShowVoxelMergedFree) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Raw Free Cells"), TEXT("Show unmerged free octree cells. Opt-in and capped at 10,000."), &_ShowVoxelRawFree) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Occupied Cells"), TEXT("Show occupied finest-layer voxel cubes. Opt-in and capped at 10,000."), &_ShowVoxelOccupied) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Chunk Portals"), TEXT("Show cross-chunk portal connections."), &_ShowVoxelPortals) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
		[ MakeVoxelToggle(TEXT("Dirty / Repair Bounds"), TEXT("Show pending and active local-repair regions."), &_ShowVoxelDirtyRepair) ]
#if WITH_EDITOR
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
		[
			SNew(SCkDebug_ToggleSurface)
			.ToolTipText(FText::FromString(TEXT("Draw the exact cooked-Jolt VoxelNav snapshot directly in every Level Editor viewport outside PIE.")))
			.IsOn_Lambda([]() -> bool
			{
				return UCk_VoxelNavPreview_EdMode::Get_IsLevelOverlayEnabled();
			})
			.OnStateChanged_Lambda([this](bool InIsOn)
			{
				if (UCk_VoxelNavPreview_EdMode::Set_LevelOverlayEnabled(InIsOn))
				{ _VoxelRefreshRequested = true; }
			})
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Level Editor Overlay")))
			]
		]
#endif
#if WITH_EDITOR
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
		[
			SNew(SButton)
				.Text(FText::FromString(TEXT("Refresh Exact Preview")))
				.ToolTipText(FText::FromString(TEXT("Revalidate cooked Jolt data and rebuild placed VoxelNav volumes outside PIE.")))
				.OnClicked_Lambda([this]() -> FReply
				{
					_VoxelRefreshRequested = true;
					return FReply::Handled();
				})
		]
#endif
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 3.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Voxel cells are driven by the selected immutable snapshot.")))
			.AutoWrapText(true)
			.ColorAndOpacity(CkStyle::TextMute())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Path opacity")))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SSlider)
				.MinValue(0.0f).MaxValue(1.0f)
				.Value_Lambda([]() -> float
				{
					const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
					return Settings != nullptr ? Settings->PathNetworkOpacity : 0.0f;
				})
				.OnValueChanged_Lambda([](float InValue)
				{
					auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
					if (Settings != nullptr)
					{ Settings->PathNetworkOpacity = FMath::Clamp(InValue, 0.0f, 1.0f); Settings->SaveConfig(); }
				})
			]
		];

	const auto CrowdMenu = Make_ControlGroup(TEXT("CROWD"),
	{
		{TEXT("ck.Crowd.DrawBreadcrumbs"), TEXT("Breadcrumbs"), TEXT("Draw the actually-traversed breadcrumb trail for agents with the recorder feature.")},
		{TEXT("ck.Crowd.DrawPlannedPaths"), TEXT("Planned Paths"), TEXT("Draw planned path waypoints for crowd agents.")},
		{TEXT("ck.Crowd.Debug.AgentBody"), TEXT("Agent Body"), TEXT("Draw each crowd agent body capsule and forward cone.")}
	});
	const auto QueueMenu = SNew(SCkDebug_ToggleSurface)
		.ToolTipText(FText::FromString(TEXT("Show queue origins, reservation slots, formation order, and agent-to-slot links in the Crowd Debugger viewport.")))
		.IsOn_Lambda([this]() { return _ShowQueues; })
		.OnStateChanged_Lambda([this](bool InIsOn) { _ShowQueues = InIsOn; })
		[ SNew(STextBlock).Text(FText::FromString(TEXT("Queue Reservations"))) ];
	const auto AvoidanceVolumeMenu = SNew(SCkDebug_ToggleSurface)
		.ToolTipText(FText::FromString(TEXT("Show physical, steering-influence, and Recast-painted Crowd avoidance volumes. Painted color shows pending/confirmed/invalid/retiring state; physical color shows traversal policy: blue Avoid If Possible, red Hard Exclude, gold Cost Only.")))
		.IsOn_Lambda([this]() { return _ShowAvoidanceVolumes; })
		.OnStateChanged_Lambda([this](bool InIsOn) { _ShowAvoidanceVolumes = InIsOn; })
		[ SNew(STextBlock).Text_Lambda([this]() -> FText
		{
			const auto Count = _ViewModel.IsValid() ? _ViewModel->Get_AvoidanceVolumes().Num() : 0;
			return FText::FromString(FString::Printf(TEXT("Avoidance Volumes (%d)"), Count));
		}) ];

	const auto DiagnosticsMenu = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Make_ControlGroup(TEXT("DIAGNOSTICS"),
			{
				{TEXT("ck.Crowd.DrawPathTrouble"), TEXT("World Trouble"), TEXT("Draw path-trouble diagnostics in the game world for affected agents.")},
				{TEXT("ck.Crowd.Debug"), TEXT("Separation"), TEXT("Draw separation-radius, neighbor, and force diagnostics.")},
				{TEXT("ck.Crowd.DrawAgentRings"), TEXT("Rings"), TEXT("Draw arrival, orbit, turn-radius, and velocity rings.")}
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 8.0f)
		[
			SNew(SCkDebug_ToggleSurface)
			.ToolTipText(FText::FromString(TEXT("Show the selected agent's retained path-trouble overlay in the Crowd Debugger viewport.")))
			.IsOn_Lambda([]() -> bool
			{
				const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
				return Settings == nullptr || Settings->ShowSelectedPathTroubleOverlay;
			})
			.OnStateChanged_Lambda([](bool InIsOn)
			{
				auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
				if (Settings != nullptr)
				{ Settings->ShowSelectedPathTroubleOverlay = InIsOn; Settings->SaveConfig(); }
			})
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Selected Trouble")))
			]
		];

	const auto Target = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
		[ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ];
	const auto Diagnostics = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
		[ SNew(SButton).Text(FText::FromString(TEXT("Run Health Check"))).ToolTipText(FText::FromString(TEXT("Run a synthetic FindPathSync probe (origin to +200) and surface the result in the Navmesh Status panel. Bypasses the request/processor pipeline entirely; a green probe proves the nav stack works in isolation from any gym wiring."))).OnClicked_Lambda([this]() -> FReply { if (_ViewModel.IsValid()) { _ViewModel->Run_HealthCheckProbe(); } return FReply::Handled(); }) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Crowd diagnostic settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Diagnostics")))].MenuContent()[DiagnosticsMenu] ];
	const auto DataView = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Choose Live PIE, retained, or exact cooked-Jolt editor VoxelNav data."))).ButtonContent()[SNew(STextBlock).Text_Lambda([this]() { return Get_VoxelSourceLabel(); })].MenuContent()[SourceMenu] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Navigation display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Navigation")))].MenuContent()[NavigationMenu] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Crowd display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Crowd")))].MenuContent()[CrowdMenu] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Queue reservation and formation display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Queues")))].MenuContent()[QueueMenu] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[ SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Crowd avoidance-volume display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Avoidance")))].MenuContent()[AvoidanceVolumeMenu] ];
	const auto Telemetry = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(_VoxelSourceStatus); }).ColorAndOpacity(FSlateColor(CkStyle::Info())) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Text_Lambda([this]() -> FText { return _ViewModel.IsValid() ? FText::FromString(FString::Printf(TEXT("Agents: %d"), _ViewModel->Get_AgentCount())) : FText::FromString(TEXT("(no view-model)")); }) ];
	return {
		FCkDebug_CommandGroup::Primary(TEXT("CrowdOverlays"), FText::FromString(TEXT("Crowd overlay controls")), BuildMenuActions()),
		FCkDebug_CommandGroup::Context(TEXT("CrowdTarget"), FText::FromString(TEXT("Crowd target selection")), Target),
		FCkDebug_CommandGroup::Context(TEXT("CrowdDiagnostics"), FText::FromString(TEXT("Crowd diagnostics")), Diagnostics),
		FCkDebug_CommandGroup::Context(TEXT("CrowdDataView"), FText::FromString(TEXT("Crowd data and view settings")), DataView),
		FCkDebug_CommandGroup::Context(TEXT("CrowdTelemetry"), FText::FromString(TEXT("Crowd telemetry")), Telemetry)};
}

auto SCkCrowdDebuggerWindow::BuildMenuActions() -> TSharedRef<SWidget>
{
	// CVar-backed state remains live so external console changes repaint without caching.
	return SNew(SCkDebug_IconToolbar)
		.Actions(TArray<FCkDebug_IconToggleAction>{
			MakeCVarToggle(TEXT("Breadcrumbs"), ECk_Icon::BodyActivity, TEXT("ck.Crowd.DrawBreadcrumbs"), TEXT("Breadcrumbs"), TEXT("Draw a breadcrumb trail (the agent's actually-traversed path) for every agent that has the recorder feature. The selected agent is always drawn regardless of this toggle.")),
			MakeCVarToggle(TEXT("PlannedPaths"), ECk_Icon::Target, TEXT("ck.Crowd.DrawPlannedPaths"), TEXT("Planned paths"), TEXT("Draw the planned-path waypoints for every agent that has a path result. The selected agent is always drawn regardless of this toggle.")),
			MakeCVarToggle(TEXT("WorldTrouble"), ECk_Icon::Error, TEXT("ck.Crowd.DrawPathTrouble"), TEXT("World trouble"), TEXT("Draw path-trouble diagnostics in the game world for every affected agent: marker, sidewalk/Unreal-nav status, attempted goal, dashed line, and Euclidean distance.")),
			MakeCVarToggle(TEXT("AgentBody"), ECk_Icon::Actor, TEXT("ck.Crowd.Debug.AgentBody"), TEXT("Agent body"), TEXT("Draw a body capsule + forward-facing cone for every crowd agent. Color comes from the agent's debug color (per-agent override or hash-derived stable fallback). PathPending agents tint yellow; Asleep agents desaturate.")),
			MakeCVarToggle(TEXT("Separation"), ECk_Icon::Crowd, TEXT("ck.Crowd.Debug"), TEXT("Separation"), TEXT("Draw separation diagnostics for every awake crowd agent: yellow separation-radius circle on the floor, cyan lines to each neighbor in the steering cache, orange arrow showing the active separation force.")),
			MakeCVarToggle(TEXT("Rings"), ECk_Icon::Ring, TEXT("ck.Crowd.DrawAgentRings"), TEXT("Rings"), TEXT("Draw the orbit-diagnosis rings for every crowd agent: arrival ring (green) + predicted-orbit ring (red) at the goal, the turn-radius circle (blue) tangent to the agent, and the velocity vector (yellow). The selected agent is always drawn regardless of this toggle."))});
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkCrowdDebuggerWindow::
	HandleFrameSelectedAgentRequested()
	-> void
{
	if (_ViewportPanel.IsValid())
	{ _ViewportPanel->Apply_CameraPreset(ECkCrowdDebugger_CameraPreset::FrameSelection); }
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebuggerWindow::~SCkCrowdDebuggerWindow()
{
	if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
	{ _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

	if (_SessionInvalidatedHandle.IsValid())
	{ ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }

	if (_ViewModel.IsValid() && _FrameSelectedAgentHandle.IsValid())
	{ _ViewModel->OnFrameSelectedAgentRequested.Remove(_FrameSelectedAgentHandle); }

	_ViewModel.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
