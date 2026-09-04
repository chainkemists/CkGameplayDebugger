#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "CkVoxelNav/Debug/CkVoxelNav_DebugSnapshot.h"
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dPreviewAdapter.h"
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"
#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"

#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnCkCrowdDebugger_AgentPicked, int32 /*AgentIndex*/);
DECLARE_DELEGATE_OneParam(FOnCkCrowdDebugger_WorldCommanded, const FVector& /*WorldDestination*/);

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
        SLATE_EVENT(FOnCkCrowdDebugger_WorldCommanded, OnWorldCommanded)
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    /** Value-only publication point. Collection stays in CkFoundation / the Crowd view model. */
    auto
    Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void;
    auto
    Notify_WorldChanged() -> void;

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
    /** The GroundNav field copy the scene draws, beside the revision derived from its cache key. A
     *  revision that has not moved is not a change, so the same copy costs nothing to re-submit. */
    auto
    Set_GroundNavField(
        const FCkCrowdDebugger_GroundNavField& InField,
        uint64 InRevision) -> void;
    auto
    Set_PathNetworkRibbons(
        const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>& InRibbons) -> void;
    auto
    Set_QueueSnapshots(const TArray<FCkCrowdDebugger_QueueSnapshot>& InQueues) -> void;
    auto
    Set_AvoidanceVolumeSnapshots(const TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>& InVolumes) -> void;
    /** Acknowledge a right-click command at a world point. Self-expires; no caller owns it. */
    auto
    Set_CommandPing(const FVector& InLocation) -> void;
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
    auto
    Get_AvoidanceVolumeRevision_ForTests() const -> uint64
    {
        return _AvoidanceVolumeRevision;
    }
    auto
    Get_AvoidanceVolumeCount_ForTests() const -> int32
    {
        return _Snapshot._AvoidanceVolumes.Num();
    }
#endif

private:
    TSharedPtr<SCkDebug_3dPreviewViewport> _CommonViewport;
    TSharedPtr<FCkCrowdDebugger_3dPreviewAdapter> _PreviewAdapter;
    FCkCrowdDebugger_3dSceneSnapshot _Snapshot;

    FOnCkCrowdDebugger_AgentPicked _OnAgentPicked;
    FOnCkCrowdDebugger_WorldCommanded _OnWorldCommanded;
    uint64 _WorldEpoch = 1;
    uint64 _PathNetworkRevision = 1;
    uint32 _PathNetworkSignature = 0;
    bool _SnapshotDirty = true;

    // Change stamp over the agent fields the scene consumes. 0 = never stamped.
    uint32 _AgentRevision = 0;

    // Change stamp over queue identity, revision, state and reservation geometry.
    uint32 _QueueSignature = 0;
    uint32 _AvoidanceVolumeSignature = 0;
    uint64 _AvoidanceVolumeRevision = 0;

    // Whether there is voxel data to clear; keeps Clear_VoxelNavSnapshot idempotent.
    bool _HasVoxelSnapshot = false;

    // Slate time at which the command ping stops drawing. Negative = no ping pending.
    double _CommandPingExpirySeconds = -1.0;
};

// --------------------------------------------------------------------------------------------------------------------
