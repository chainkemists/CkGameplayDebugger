#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dPreviewAdapter.h"
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"
#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"

#include "Widgets/SCompoundWidget.h"

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

class CKCROWDDEBUGGER_API SCkCrowdDebugger_3dViewport final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkCrowdDebugger_3dViewport) {}
        SLATE_EVENT(FOnCkCrowdDebugger_AgentPicked, OnAgentPicked)
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    /** Value-only publication point. Collection stays in CkFoundation / the Crowd view model. */
    auto
    Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void;
    auto
    Clear_VoxelNavSnapshot() -> void;
    auto
    Set_AgentSnapshots(
        const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents,
        const FCk_Handle& InSelectedHandle) -> void;
    auto
    Set_NavmeshTriangles(
        const TArray<FVector>& InNavTriVerts,
        uint64 InGeometryRevision) -> void;
    auto
    Set_PathNetworkRibbons(
        const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>& InRibbons) -> void;
    auto
    Set_QueueSnapshots(const TArray<FCkCrowdDebugger_QueueSnapshot>& InQueues) -> void;
    auto
    Apply_CameraPreset(
        ECkCrowdDebugger_CameraPreset InPreset) -> void;

    virtual auto
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

#if WITH_DEV_AUTOMATION_TESTS
    auto
    Get_PathNetworkOpacity_ForTests() const -> float
    {
        return _Snapshot._PathNetwork._Opacity;
    }

    auto
    Get_PathNetworkRevision_ForTests() const -> uint64
    {
        return _Snapshot._PathNetwork._Revision;
    }
#endif

private:
    TSharedPtr<SCkDebug_3dPreviewViewport> _CommonViewport;
    TSharedPtr<FCkCrowdDebugger_3dPreviewAdapter> _PreviewAdapter;
    FCkCrowdDebugger_3dSceneSnapshot _Snapshot;

    FOnCkCrowdDebugger_AgentPicked _OnAgentPicked;
    uint64 _WorldEpoch = 1;
    uint64 _PathNetworkRevision = 1;
    uint32 _PathNetworkSignature = 0;
    bool _SnapshotDirty = true;
};

// --------------------------------------------------------------------------------------------------------------------
