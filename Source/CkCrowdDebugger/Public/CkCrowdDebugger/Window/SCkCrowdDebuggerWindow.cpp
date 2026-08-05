#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Settings/CkCrowdDebuggerSettings.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"

#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkVoxelNav/Volume/CkVoxelNavVolume_Utils.h"
#include "CkVoxelNavEditor/Preview/CkVoxelNavPreview_EdMode.h"
#include "CkVoxelNavEditor/Preview/CkVoxelNavPreview_EditorSubsystem.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkCrowdDebuggerWindow::WindowId{TEXT("CkCrowdDebugger")};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Construct(const FArguments& InArgs) -> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkCrowdDebugger_ViewModel>();
	_WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
	_WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(
		this, &SCkCrowdDebuggerWindow::HandleWorldChanged);

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
		});

	ChildSlot
	[
		SNew(SCkDebug_WindowChrome)
		.WindowId(WindowId)
		.ToolTabId(TEXT("CkCrowdDebugger"))
		.DisplayName(FText::FromString(TEXT("CK Crowd Debugger")))
		.Content()
		[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ BuildToolbar() ]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			// Left rail: navmesh status + agent list + stats + event log.
			+ SSplitter::Slot().Value(0.20f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.22f) [ _NavmeshStatusPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.46f) [ _AgentListPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.14f) [ _StatsPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.18f) [ _EventLogPanel.ToSharedRef() ]
			]
			// Center: the viewport, full height (the mockup's centerpiece).
			+ SSplitter::Slot().Value(0.52f)
			[
				_ViewportPanel.ToSharedRef()
			]
			// Right: agent detail + tuners + diagnostics, full height (no scrolling).
			+ SSplitter::Slot().Value(0.28f)
			[
				_AgentDetailPanel.ToSharedRef()
			]
		]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::HandleWorldChanged(UWorld*) -> void
{
	if (_ViewModel.IsValid())
	{ _ViewModel->Reset_ForWorldChange(); }
	if (_ViewportPanel.IsValid())
	{ _ViewportPanel->Clear_VoxelNavSnapshot(); }
	_VoxelRefreshRequested = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
	SCkDebugger_WindowBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT _ViewModel.IsValid())
	{ return; }

	// Resolve the inspected world via the shared selector. If none (editor idle,
	// no PIE), the data collector will early-out on a null world.
	_WorldModel->Ensure_AutoSelect();
	auto* World = _WorldModel->Get_SelectedWorld();

	_ViewModel->Tick(World, InDeltaTime);
	if (_ViewportPanel.IsValid())
	{
		_ViewportPanel->Set_NavmeshTriangles(
			_ViewModel->Get_NavTriVerts(),
			_ViewModel->Get_NavGeometryRevision());
		_ViewportPanel->Set_AgentSnapshots(
			_ViewModel->Get_AllAgents(),
			_ViewModel->Get_SelectedHandle());
		_ViewportPanel->Set_PathNetworkRibbons(_ViewModel->Get_PathNetworkRibbons());
	}

	if (_VoxelRefreshRequested || InCurrentTime >= _NextVoxelRefreshTime)
	{
		Refresh_VoxelSnapshot(World);
		_VoxelRefreshRequested = false;
		_NextVoxelRefreshTime = InCurrentTime + 0.25;
	}

	if (_PendingTarget.IsSet())
	{
		const auto Target = _PendingTarget.GetValue();
		_PendingTarget.Reset();
		if (_AgentListPanel.IsValid()) { _AgentListPanel->SelectEntityExternal(Target); }
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
		return SNew(SCheckBox)
			.ToolTipText(FText::FromString(InDefinition._Tooltip))
			.IsChecked_Lambda([Definition = InDefinition]() -> ECheckBoxState
			{
				return Get_CVarBool(Definition._CVar) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Definition = InDefinition](ECheckBoxState InNewState)
			{
				Set_CVarBool(Definition._CVar, InNewState == ECheckBoxState::Checked);
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
				SNew(STextBlock)
				.Text(FText::FromString(InTitle))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(0.65f, 0.75f, 0.90f))
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
	case ECkCrowdDebugger_VoxelSource::EditorPreview: return FText::FromString(TEXT("Editor Preview"));
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
		Source = HasRuntimeWorld ? ECkCrowdDebugger_VoxelSource::LivePie : ECkCrowdDebugger_VoxelSource::EditorPreview;
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

	if (NOT Combined.IsSet())
	{
		_VoxelSourceStatus = Source == ECkCrowdDebugger_VoxelSource::RetainedSnapshot
			? TEXT("VoxelNav Retained: no PIE snapshot captured yet")
			: Source == ECkCrowdDebugger_VoxelSource::LivePie
				? TEXT("VoxelNav Live PIE: no runtime volume")
				: TEXT("VoxelNav Editor Preview: place a Ck Voxel Nav Volume in the level");
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

	_VoxelSourceStatus = FString::Printf(TEXT("VoxelNav %s: %s | merged %d/%d | raw %d/%d | occupied %d/%d"),
		Source == ECkCrowdDebugger_VoxelSource::LivePie ? TEXT("Live PIE") :
			Source == ECkCrowdDebugger_VoxelSource::RetainedSnapshot ? TEXT("Retained") : TEXT("Editor Preview"),
		Get_StatusLabel(Combined->_Status),
		Combined->_MergedFree._Cells.Num(), Combined->_MergedFree._FilteredTotal,
		Combined->_RawFree._Cells.Num(), Combined->_RawFree._FilteredTotal,
		Combined->_Occupied._Cells.Num(), Combined->_Occupied._FilteredTotal);

	if (_ViewportPanel.IsValid())
	{ _ViewportPanel->Set_VoxelNavSnapshot(*Combined); }
}

auto SCkCrowdDebuggerWindow::BuildToolbar() -> TSharedRef<SWidget>
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
		return SNew(SCheckBox)
			.ToolTipText(FText::FromString(InTooltip))
			.IsChecked_Lambda([InValue]() -> ECheckBoxState
			{ return *InValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, InValue](ECheckBoxState InState)
			{
				*InValue = InState == ECheckBoxState::Checked;
				_VoxelRefreshRequested = true;
			})
			[ SNew(STextBlock).Text(FText::FromString(InLabel)) ];
	};

	const auto MakeCameraButton = [this](const TCHAR* InIconName, const TCHAR* InTooltip, ECkCrowdDebugger_CameraPreset InPreset) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(FText::FromString(InTooltip))
			.ContentPadding(FMargin(4.0f, 1.0f))
			.OnClicked_Lambda([this, InPreset]() -> FReply
			{
				if (_ViewportPanel.IsValid())
				{ _ViewportPanel->Apply_CameraPreset(InPreset); }
				return FReply::Handled();
			})
			[
				SNew(SImage).Image(FAppStyle::Get().GetBrush(InIconName))
			];
	};

	const auto SourceMenu = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Auto (PIE when running, Editor otherwise)"), ECkCrowdDebugger_VoxelSource::Auto) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Live PIE"), ECkCrowdDebugger_VoxelSource::LivePie) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Retained Snapshot"), ECkCrowdDebugger_VoxelSource::RetainedSnapshot) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeSourceButton(TEXT("Editor Preview"), ECkCrowdDebugger_VoxelSource::EditorPreview) ];

	const auto NavigationMenu = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Make_ControlGroup(TEXT("UNREAL NAVIGATION"),
			{
				{TEXT("ck.Crowd.DrawNavProjection"), TEXT("Unreal Nav Projection"), TEXT("Draw the existing Unreal navmesh/projection diagnostics.")}
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f, 8.0f, 2.0f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("VOXEL NAVIGATION")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(0.65f, 0.75f, 0.90f))
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
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
		[
			SNew(SCheckBox)
			.ToolTipText(FText::FromString(TEXT("Draw the exact cooked-Jolt VoxelNav snapshot directly in every Level Editor viewport outside PIE.")))
			.IsChecked_Lambda([]() -> ECheckBoxState
			{
				return UCk_VoxelNavPreview_EdMode::Get_IsLevelOverlayEnabled()
					? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
			{
				const auto Enable = InState == ECheckBoxState::Checked;
				if (UCk_VoxelNavPreview_EdMode::Set_LevelOverlayEnabled(Enable))
				{ _VoxelRefreshRequested = true; }
			})
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Level Editor Overlay")))
			]
		]
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
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 3.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Voxel cells are driven by the selected immutable snapshot.")))
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
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
			SNew(SCheckBox)
			.ToolTipText(FText::FromString(TEXT("Show the selected agent's retained path-trouble overlay in the Crowd Debugger viewport.")))
			.IsChecked_Lambda([]() -> ECheckBoxState
			{
				const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
				return Settings == nullptr || Settings->ShowSelectedPathTroubleOverlay ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState InState)
			{
				auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
				if (Settings != nullptr)
				{ Settings->ShowSelectedPathTroubleOverlay = InState == ECheckBoxState::Checked; Settings->SaveConfig(); }
			})
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Selected Trouble")))
			]
		];

	return SNew(SBorder)
		.Padding(FMargin(8, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("CK Crowd Debugger")))
			]
			// World selector (shared across all CK debuggers)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SCkDebug_WorldSelector, _WorldModel)
				.ShowHeaderLabel(false)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Run Health Check")))
				.ToolTipText(FText::FromString(TEXT("Run a synthetic FindPathSync probe (origin to +200) and surface the result in the Navmesh Status panel. Bypasses the request/processor pipeline entirely; a green probe proves the nav stack works in isolation from any gym wiring.")))
				.OnClicked_Lambda([this]() -> FReply
				{
					if (_ViewModel.IsValid())
					{ _ViewModel->Run_HealthCheckProbe(); }
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 4, 0)
			[
				SNew(SComboButton)
					.ToolTipText(FText::FromString(TEXT("Choose Live PIE, retained, or exact cooked-Jolt editor VoxelNav data.")))
					.ButtonContent()[SNew(STextBlock).Text_Lambda([this]() { return Get_VoxelSourceLabel(); })]
					.MenuContent()[SourceMenu]
			]
			// Compact menus replace the old noisy flat checkbox strip. CVar-backed checks still reflect console changes.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 4, 0)
			[
				SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Navigation display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Navigation")))].MenuContent()[NavigationMenu]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Crowd display settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Crowd")))].MenuContent()[CrowdMenu]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SComboButton).ToolTipText(FText::FromString(TEXT("Crowd diagnostic settings."))).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Diagnostics")))].MenuContent()[DiagnosticsMenu]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Perspective"), TEXT("Perspective camera"), ECkCrowdDebugger_CameraPreset::Perspective) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Top"), TEXT("Top orthographic camera"), ECkCrowdDebugger_CameraPreset::Top) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Bottom"), TEXT("Bottom orthographic camera"), ECkCrowdDebugger_CameraPreset::Bottom) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Left"), TEXT("Left orthographic camera"), ECkCrowdDebugger_CameraPreset::Left) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Right"), TEXT("Right orthographic camera"), ECkCrowdDebugger_CameraPreset::Right) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Front"), TEXT("Front orthographic camera"), ECkCrowdDebugger_CameraPreset::Front) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("EditorViewport.Back"), TEXT("Back orthographic camera"), ECkCrowdDebugger_CameraPreset::Back) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("Icons.FrameActor"), TEXT("Frame all VoxelNav and crowd-agent bounds"), ECkCrowdDebugger_CameraPreset::FrameAll) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)
			[ MakeCameraButton(TEXT("Icons.SelectInViewport"), TEXT("Frame the selected crowd agent"), ECkCrowdDebugger_CameraPreset::FrameSelection) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8, 0)
			[
				SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(_VoxelSourceStatus); })
					.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.78f, 0.92f)))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					if (NOT _ViewModel.IsValid())
					{ return FText::FromString(TEXT("(no view-model)")); }
					return FText::FromString(FString::Printf(TEXT("Agents: %d"), _ViewModel->Get_AgentCount()));
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("ck.CrowdDebugger 1")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			]
		];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebuggerWindow::~SCkCrowdDebuggerWindow()
{
	if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
	{ _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

	_ViewModel.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
