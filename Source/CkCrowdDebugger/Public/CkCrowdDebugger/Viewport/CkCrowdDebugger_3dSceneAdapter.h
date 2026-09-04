#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkDebugScene/CkDebugScene_Target.h"

#include <CoreMinimal.h>

enum class ECkCrowdDebugger_3dSceneRole : uint8
{
    AgentCapsule,
    SelectedPath,
    Recast,
    GroundNavField,
    PathNetworkRibbon,
    VoxelOccupied,
    VoxelMergedFree,
    VoxelRawFree,
    VoxelChunk,
    VoxelPortal,
    VoxelRepair,
    QueueOwnerTarget,
    QueueReservation,
    AvoidanceVolumePhysical,
    AvoidanceVolumeInfluence,
    AvoidanceVolumePainted,
    CommandPing
};

enum class ECkCrowdDebugger_3dVoxelLayer : uint8
{
    Occupied,
    MergedFree,
    RawFree
};

struct FCkCrowdDebugger_3dRibbonSnapshot
{
    TArray<FVector> _Points;
    TArray<float> _HalfWidths;
};
struct FCkCrowdDebugger_3dSegmentSnapshot
{
    FVector _From = FVector::ZeroVector;
    FVector _To = FVector::ZeroVector;
    FVector _Via = FVector::ZeroVector;
    bool _HasVia = false;
};

struct FCkCrowdDebugger_3dAgentSnapshot
{
    uint64 _Identity = 0;
    FVector _Position = FVector::ZeroVector;
    FVector _Velocity = FVector::ZeroVector;
    float _Radius = 0.0f;
    float _Height = 0.0f;
    FLinearColor _StatusColor = FLinearColor::White;
    TArray<FVector> _PlannedPath;
};

struct FCkCrowdDebugger_3dVoxelCells
{
    TArray<FBox> _Occupied;
    TArray<FBox> _MergedFree;
    TArray<FBox> _RawFree;
};

struct FCkCrowdDebugger_3dVoxelSnapshot
{
    uint64 _Revision = 0;
    FBox _AuthoredBounds = FBox{ForceInit};
    FBox _NavigationBounds = FBox{ForceInit};
    FBox _PendingBounds = FBox{ForceInit};
    FBox _ActiveBounds = FBox{ForceInit};
    FCkCrowdDebugger_3dVoxelCells _Cells;
    TArray<FBox> _Chunks;
    TArray<FCkCrowdDebugger_3dSegmentSnapshot> _Portals;
    TArray<FCkCrowdDebugger_3dSegmentSnapshot> _RepairLinks;
    TMap<ECkCrowdDebugger_3dVoxelLayer, bool> _LayerVisibility;
    int32 _RawFreeCellCap = 10000;
    FLinearColor _AuthoredBoundsColor = FLinearColor{1.0f, 0.75f, 0.2f, 0.9f};
    FLinearColor _NavigationBoundsColor = FLinearColor{0.85f, 0.85f, 0.85f, 0.8f};
    FLinearColor _PendingBoundsColor = FLinearColor{1.0f, 0.5f, 0.1f, 0.95f};
    FLinearColor _ActiveBoundsColor = FLinearColor{1.0f, 0.1f, 0.1f, 0.95f};
    FLinearColor _MergedFreeColor = FLinearColor{0.2f, 0.85f, 1.0f, 0.9f};
    FLinearColor _RawFreeColor = FLinearColor{0.2f, 1.0f, 0.45f, 0.45f};
    FLinearColor _OccupiedColor = FLinearColor{1.0f, 0.2f, 0.15f, 0.45f};
    FLinearColor _ChunkColor = FLinearColor{0.7f, 0.35f, 1.0f, 0.85f};
    FLinearColor _PortalColor = FLinearColor{0.9f, 0.45f, 1.0f, 1.0f};
    FLinearColor _RepairColor = FLinearColor::Yellow;
    bool _CanDrawRetainedGeometry = true;
};

struct FCkCrowdDebugger_3dRecastSnapshot
{
    uint64 _Revision = 0;
    TArray<FVector> _Triangles;
};
// The GroundNav field the window is looking at, as the debugger's own copy of one bake.
//
// The field arrives WHOLE rather than pre-projected into primitives so the overlay reads the same
// arrays CkGroundNav's own draw does - _Plates feeds the plate outlines, _Boundary feeds the boundary
// runs - and there is no intermediate projection for the two to drift apart in.
//
// _Revision is the producer's change stamp, moved when the copy's cache key moves. Same shape as the
// Voxel layer's: an unchanged field re-submits its retained items instead of re-hashing every plate
// once a frame.
struct FCkCrowdDebugger_3dGroundNavSnapshot
{
    uint64 _Revision = 0;
    FCkCrowdDebugger_GroundNavField _Field;
    // Outside the layer palette on purpose, exactly as CkGroundNav draws it: a tile-rim run is a wall
    // only until the neighbouring tile is baked, and it must never read as one the bake is sure of.
    FLinearColor _TileRimColor = FLinearColor{FColor{255, 140, 0}};
};
struct FCkCrowdDebugger_3dPathNetworkSnapshot
{
    uint64 _Revision = 0;
    float _Opacity = 1.0f;
    TArray<FCkCrowdDebugger_3dRibbonSnapshot> _Ribbons;
};
struct FCkCrowdDebugger_3dQueueMemberSnapshot
{
    uint64 _AgentIdentity = 0;
    uint64 _SlotIdentity = 0;
    int32 _Rank = INDEX_NONE;
    FVector _ReservationLocation = FVector::ZeroVector;
    FVector _ReservationForward = FVector::ForwardVector;
    bool _HasReservation = false;
};
struct FCkCrowdDebugger_3dQueueSnapshot
{
    uint64 _Identity = 0;
    uint64 _Revision = 0;
    FString _DebugName;
    FString _Category;
    FString _State;
    FCkCrowdDebugger_3dSegmentSnapshot _OwnerTarget;
    TArray<FCkCrowdDebugger_3dQueueMemberSnapshot> _Members;
};
struct FCkCrowdDebugger_3dAvoidanceVolumeSnapshot
{
    uint64 _Identity = 0;
    uint64 _Revision = 0;
    FTransform _YawWorldTransform = FTransform::Identity;
    FVector _PhysicalWorldHalfExtents = FVector::ZeroVector;
    FVector _InfluenceWorldHalfExtents = FVector::ZeroVector;
    FVector _PaintedWorldHalfExtents = FVector::ZeroVector;
    ECkCrowdDebugger_AvoidanceVolumeState _State = ECkCrowdDebugger_AvoidanceVolumeState::Pending;
    ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy _TraversalPolicy =
        ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::AvoidIfPossible;
    bool _HasValidGeometry = false;
};
// Acknowledgment for a right-click command, drawn IN the debugger viewport. The world-space PMG
// ping cannot appear here: this scene renders into the viewport's own preview world, never the
// game world, so a game-world actor is structurally absent from it.
struct FCkCrowdDebugger_3dCommandPing
{
    FVector _Location = FVector::ZeroVector;
    FLinearColor _Color = FLinearColor{0.15f, 1.0f, 0.35f, 1.0f};
    bool _IsSet = false;
};

struct FCkCrowdDebugger_3dSceneSnapshot
{
    uint64 _WorldEpoch = 0;
    TArray<FCkCrowdDebugger_3dAgentSnapshot> _Agents;
    TOptional<uint64> _SelectedIdentity;
    FCkCrowdDebugger_3dVoxelSnapshot _Voxel;
    FCkCrowdDebugger_3dRecastSnapshot _Recast;
    FCkCrowdDebugger_3dGroundNavSnapshot _GroundNav;
    FCkCrowdDebugger_3dPathNetworkSnapshot _PathNetwork;
    TArray<FCkCrowdDebugger_3dQueueSnapshot> _Queues;
    TArray<FCkCrowdDebugger_3dAvoidanceVolumeSnapshot> _AvoidanceVolumes;
    FCkCrowdDebugger_3dCommandPing _CommandPing;
};

struct FCkCrowdDebugger_3dPickResolution
{
    uint64 _Identity = 0;
    int32 _CurrentAgentIndex = INDEX_NONE;
};

class FCkCrowdDebugger_3dSceneAdapter
{
  public:
    auto
    Reconcile(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot, FCk_DebugScene_Target& InTarget) -> bool;
    auto
    Reset_ForWorldChange(FCk_DebugScene_Target& InTarget) -> void;
    auto
    Resolve_Pick(const FCk_DebugScene_Pick& InPick) const -> TOptional<FCkCrowdDebugger_3dPickResolution>;
    auto
    Get_CurrentAgentIndex(uint64 InIdentity) const -> TOptional<int32>;
    auto
    TrySelect_Identity(uint64 InIdentity, FCk_DebugScene_Target& InTarget) -> bool;
    auto
    Get_SelectedIdentity() const -> TOptional<uint64>
    {
        return _SelectedIdentity;
    }
    auto
    Get_SelectionBounds(const FCk_DebugScene_Target& InTarget) const -> TOptional<FBox>;
    auto
    Get_ItemCount(ECkCrowdDebugger_3dSceneRole InRole) const -> int32;
    auto
    Has_Role(ECkCrowdDebugger_3dSceneRole InRole) const -> bool;
    auto
    Get_TargetItemId(ECkCrowdDebugger_3dSceneRole InRole, int32 InIndex) const -> TOptional<uint64>;
    auto
    Get_SubmittedInstances(uint64 InIdentity) const -> const TArray<FCk_DebugScene_Instance>&;
    auto
    Get_Appearance(uint64 InIdentity) const -> FCk_DebugScene_Appearance;
    auto
    Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole InRole) const -> FCk_DebugScene_Appearance;
    auto
    Get_RibbonTriangleCount(int32 InIndex) const -> int32;
    auto
    Get_RibbonRenderedTriangleCount(int32 InIndex) const -> int32;
    auto
    Get_RibbonOutlinePointCount(int32 InIndex) const -> int32;
    auto
    Get_RecastTriangleCount() const -> int32;
    auto
    Get_RecastRenderedTriangleCount() const -> int32;
    /** Plate outlines and boundary runs share one role, so the counts they are read back by are
     *  separate: an overlay that agrees with CkGroundNav's own draw has to be checkable per element
     *  against the snapshot arrays that fed it. */
    auto
    Get_GroundNavPlateCount() const -> int32;
    auto
    Get_GroundNavBoundaryCount() const -> int32;

  private:
    auto
    MakeItemKey(ECkCrowdDebugger_3dSceneRole InRole, uint64 InIdentity) -> uint64;
    auto
    MakeAppearance(FLinearColor InColor, bool InTransparent = false,
                   ECk_DebugScene_DepthPriority InDepthPriority = ECk_DebugScene_DepthPriority::World,
                   int32 InTranslucencySortPriority = 0) const -> FCk_DebugScene_Appearance;
    auto
    SubmitLines(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot, FCk_DebugScene_Target& InTarget) const -> bool;

  private:
    uint64 _WorldEpoch = 0;
    uint64 _RecastRevision = MAX_uint64;
    uint64 _GroundNavRevision = MAX_uint64;
    uint64 _RibbonRevision = MAX_uint64;
    uint64 _VoxelRevision = MAX_uint64;
    int32 _RibbonCount = 0;
    uint64 _NextItemKey = 1;
    TOptional<uint64> _SelectedIdentity;
    TMap<uint64, int32> _AgentIndices;
    TMap<uint64, uint64> _AgentItemKeys;
    TMap<uint64, TArray<FCk_DebugScene_Instance>> _AgentInstances;
    TMap<uint64, TArray<FCk_DebugScene_Instance>> _StaticInstances;
    TMap<FString, uint64> _InternalItemKeys;
    TMap<ECkCrowdDebugger_3dSceneRole, TArray<uint64>> _RoleItems;
    TMap<ECkCrowdDebugger_3dSceneRole, int32> _NonItemRoleCounts;
    TMap<uint64, FCk_DebugScene_Appearance> _Appearances;
    TMap<ECkCrowdDebugger_3dSceneRole, FCk_DebugScene_Appearance> _RoleAppearances;
    // Per-bucket content stamp of the navmesh mesh, so a Recast rebake rebuilds only the buckets
    // whose geometry moved instead of the whole map.
    TMap<uint64, uint32> _RecastBucketSignatures;
    int32 _RecastTriangleCount = 0;
    int32 _RecastRenderedTriangleCount = 0;
    // The GroundNav identities currently on screen, so the ones a rebake dropped can be evicted from
    // the instance and interned-key maps. The item keys alone cannot do it: eviction needs the
    // identity the key was interned UNDER, not the key.
    TSet<uint64> _GroundNavIdentities;
    int32 _GroundNavPlateCount = 0;
    int32 _GroundNavBoundaryCount = 0;
    TArray<int32> _RibbonTriangleCounts;
    TArray<int32> _RibbonRenderedTriangleCounts;
    TArray<int32> _RibbonOutlinePointCounts;
};
