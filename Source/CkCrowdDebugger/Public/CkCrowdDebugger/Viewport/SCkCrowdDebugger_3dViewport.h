#pragma once

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"

#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FCkCrowdDebugger_3dViewportClient;

// --------------------------------------------------------------------------------------------------------------------
// A deliberately isolated 3D inspection surface. It renders value-only VoxelNav snapshots, never an ECS handle,
// UWorld, or octree share, so the widget can remain open after PIE has stopped.
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

class SCkCrowdDebugger_3dViewport final : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_3dViewport) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	/** Value-only publication point. Collection stays in CkFoundation / the Crowd view model. */
	auto Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void;
	auto Clear_VoxelNavSnapshot() -> void;
	auto Apply_CameraPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void;

protected:
	virtual auto MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient> override;

private:
	TSharedPtr<FAdvancedPreviewScene> _PreviewScene;
	TSharedPtr<FCkCrowdDebugger_3dViewportClient> _ViewportClient;
};

// --------------------------------------------------------------------------------------------------------------------
