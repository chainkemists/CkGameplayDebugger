#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"

#if WITH_EDITOR
#include "SEditorViewport.h"
#else
#include "Widgets/SCompoundWidget.h"
#endif

class FPreviewScene;
class FCkCrowdDebugger_3dViewportClient;

DECLARE_DELEGATE_OneParam(FOnCkCrowdDebugger_AgentPicked, int32 /*AgentIndex*/);

// --------------------------------------------------------------------------------------------------------------------
// A deliberately isolated 3D inspection surface. It renders value-only Unreal navmesh, VoxelNav, path-network, and
// crowd-agent snapshots, never a UWorld or octree share, so the widget can remain open after PIE has stopped.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkCrowdDebugger_CameraPreset : uint8
{
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back,
	FrameAll,
	FrameSelection
};

#if WITH_EDITOR
using SCkCrowdDebugger_ViewportBase = SEditorViewport;
#else
using SCkCrowdDebugger_ViewportBase = SCompoundWidget;
#endif

class SCkCrowdDebugger_3dViewport final : public SCkCrowdDebugger_ViewportBase
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_3dViewport) {}
		SLATE_EVENT(FOnCkCrowdDebugger_AgentPicked, OnAgentPicked)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	/** Value-only publication point. Collection stays in CkFoundation / the Crowd view model. */
	auto Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void;
	auto Clear_VoxelNavSnapshot() -> void;
	auto Set_AgentSnapshots(
		const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents,
		const FCk_Handle& InSelectedHandle) -> void;
	auto Set_NavmeshTriangles(const TArray<FVector>& InNavTriVerts, uint64 InGeometryRevision) -> void;
	auto Set_PathNetworkRibbons(
		const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>& InRibbons) -> void;
	auto Apply_CameraPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void;

#if WITH_EDITOR
protected:
	virtual auto MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient> override;

private:
	TSharedPtr<FPreviewScene> _PreviewScene;
	TSharedPtr<FCkCrowdDebugger_3dViewportClient> _ViewportClient;
#endif

private:
	FOnCkCrowdDebugger_AgentPicked _OnAgentPicked;
};

// --------------------------------------------------------------------------------------------------------------------
