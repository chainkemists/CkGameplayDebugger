#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "CkCrowdDebugger/Settings/CkCrowdDebuggerSettings.h"

#include "EngineGlobals.h"
#include "HAL/IConsoleManager.h"

// These colours encode Crowd and VoxelNav state in the 3D scene. They intentionally remain local to this
// debugger adapter instead of becoming Slate style roles.
namespace ck_crowd_debugger_3d_viewport
{
    auto
    Get_BoxColor(int32 InLayer) -> FLinearColor
    {
        switch (InLayer)
        {
            case 0: return FLinearColor(0.20f, 0.85f, 1.00f, 0.90f); // merged free
            case 1: return FLinearColor(0.20f, 1.00f, 0.45f, 0.45f); // raw free
            default:return FLinearColor(1.00f, 0.20f, 0.15f, 0.45f); // occupied
        }
    }

    auto
    Get_StatusColor(ck::voxelnav::EDebugSnapshotStatus InStatus) -> FLinearColor
    {
        using ck::voxelnav::EDebugSnapshotStatus;
        switch (InStatus)
        {
            case EDebugSnapshotStatus::Building: return FLinearColor(0.70f, 0.35f, 1.00f, 0.85f);
            case EDebugSnapshotStatus::StaleCook: return FLinearColor(1.00f, 0.55f, 0.05f, 0.85f);
            case EDebugSnapshotStatus::MissingCook:
            case EDebugSnapshotStatus::Failed: return FLinearColor(1.00f, 0.10f, 0.10f, 0.90f);
            default: return FLinearColor(1.00f, 0.75f, 0.20f, 0.90f);
        }
    }

    auto
    Get_CanDrawRetainedGeometry(ck::voxelnav::EDebugSnapshotStatus InStatus) -> bool
    {
        using ck::voxelnav::EDebugSnapshotStatus;
        return InStatus == EDebugSnapshotStatus::Current ||
            InStatus == EDebugSnapshotStatus::Building ||
            InStatus == EDebugSnapshotStatus::StaleCook ||
            InStatus == EDebugSnapshotStatus::RuntimeOnly;
    }

    auto
    Get_AgentColor(ECkCrowdDebugger_AgentStatus InStatus) -> FLinearColor
    {
        switch (InStatus)
        {
            case ECkCrowdDebugger_AgentStatus::Walking:     return FLinearColor(0.20f, 0.75f, 1.00f, 1.00f);
            case ECkCrowdDebugger_AgentStatus::Idle:        return FLinearColor(0.55f, 0.55f, 0.55f, 1.00f);
            case ECkCrowdDebugger_AgentStatus::Replanning:  return FLinearColor(1.00f, 0.70f, 0.15f, 1.00f);
            case ECkCrowdDebugger_AgentStatus::Failed:      return FLinearColor(1.00f, 0.20f, 0.15f, 1.00f);
            case ECkCrowdDebugger_AgentStatus::Asleep:      return FLinearColor(0.35f, 0.35f, 0.35f, 1.00f);
            case ECkCrowdDebugger_AgentStatus::PlayerProxy: return FLinearColor(0.25f, 1.00f, 0.55f, 1.00f);
            default:                                        return FLinearColor(0.65f, 0.65f, 0.65f, 1.00f);
        }
    }

    auto
    Trace_PathNetworkPublication(
        const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>& InRibbons,
        int32 InNavmeshTriangleCount,
        float InOpacity) -> void
    {
        static const auto* TraceCVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("ck.CrowdDebugger.PathNetworkTrace"));
        static uint64 LastTraceFrame = MAX_uint64;
        if (TraceCVar == nullptr || TraceCVar->GetInt() == 0 || LastTraceFrame == GFrameCounter)
        {
            return;
        }

        LastTraceFrame = GFrameCounter;
        auto FirstStart = FVector::ZeroVector;
        auto FirstEnd = FVector::ZeroVector;
        auto FirstStartHalfWidth = 0.0f;
        auto FirstEndHalfWidth = 0.0f;
        if (NOT InRibbons.IsEmpty() && InRibbons[0].Points.Num() >= 2)
        {
            FirstStart = InRibbons[0].Points[0];
            FirstEnd = InRibbons[0].Points[1];
            if (InRibbons[0].HalfWidths.Num() >= 2)
            {
                FirstStartHalfWidth = InRibbons[0].HalfWidths[0];
                FirstEndHalfWidth = InRibbons[0].HalfWidths[1];
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("CkCrowdDebugger.PathNetworkTrace stage=retained_publish frame=%llu navmesh_triangles=%d ribbons=%d opacity=%.3f first_start=%s first_end=%s first_start_half_width=%.3f first_end_half_width=%.3f"),
            static_cast<unsigned long long>(GFrameCounter),
            InNavmeshTriangleCount,
            InRibbons.Num(),
            InOpacity,
            *FirstStart.ToCompactString(),
            *FirstEnd.ToCompactString(),
            FirstStartHalfWidth,
            FirstEndHalfWidth);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkCrowdDebugger_3dViewport::
    Construct(const FArguments& InArgs)
    -> void
{
    _OnAgentPicked = InArgs._OnAgentPicked;
    _PreviewAdapter = MakeShared<FCkCrowdDebugger_3dPreviewAdapter>();
    _PreviewAdapter->Set_OnSelected([Callback = _OnAgentPicked](TOptional<int32> InIndex)
    {
        if (InIndex.IsSet())
        {
            Callback.ExecuteIfBound(*InIndex);
        }
    });

    _OnWorldCommanded = InArgs._OnWorldCommanded;
    _PreviewAdapter->Set_OnCommandAtPoint([Callback = _OnWorldCommanded](const FVector& InDestination)
    {
        Callback.ExecuteIfBound(InDestination);
    });

    auto Descriptor = FCkDebug3dPreviewDescriptor{};
    Descriptor._PreviewPolicy._DefaultLighting = false;
    Descriptor._PreviewPolicy._Lighting = false;
    Descriptor._PreviewPolicy._PostProcessing = false;
    Descriptor._PreviewPolicy._EyeAdaptation = false;
    _CommonViewport = SNew(SCkDebug_3dPreviewViewport)
        .Descriptor(Descriptor)
        .Adapter(_PreviewAdapter);
    _PreviewAdapter->Set_Target(MakeShared<FCk_DebugScene_Target>(
        FCk_DebugScene_TargetConfig{}.Set_World(_CommonViewport->Get_PreviewWorld())));
    _Snapshot._WorldEpoch = _WorldEpoch;
    ChildSlot[_CommonViewport.ToSharedRef()];
}

auto
    SCkCrowdDebugger_3dViewport::
    Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot)
    -> void
{
    _Snapshot._WorldEpoch = _WorldEpoch;
    _HasVoxelSnapshot = true;
    _Snapshot._Voxel = {};
    _Snapshot._Voxel._Revision = InSnapshot._Generation != 0
        ? InSnapshot._Generation
        : HashCombineFast(GetTypeHash(InSnapshot._SourceFingerprint), GetTypeHash(InSnapshot._SourceEpoch));
    _Snapshot._Voxel._AuthoredBounds = InSnapshot._AuthoredBounds;
    _Snapshot._Voxel._NavigationBounds = InSnapshot._NavigationBounds;
    _Snapshot._Voxel._PendingBounds = InSnapshot._PendingDirtyBounds;
    _Snapshot._Voxel._ActiveBounds = InSnapshot._ActiveDirtyBounds;

    const auto StatusColor = ck_crowd_debugger_3d_viewport::Get_StatusColor(InSnapshot._Status);
    const auto UsesLayerColors = InSnapshot._Status == ck::voxelnav::EDebugSnapshotStatus::Current ||
        InSnapshot._Status == ck::voxelnav::EDebugSnapshotStatus::RuntimeOnly;
    _Snapshot._Voxel._AuthoredBoundsColor = StatusColor;
    _Snapshot._Voxel._MergedFreeColor = UsesLayerColors
        ? ck_crowd_debugger_3d_viewport::Get_BoxColor(0)
        : StatusColor;
    _Snapshot._Voxel._RawFreeColor = UsesLayerColors
        ? ck_crowd_debugger_3d_viewport::Get_BoxColor(1)
        : StatusColor;
    _Snapshot._Voxel._OccupiedColor = UsesLayerColors
        ? ck_crowd_debugger_3d_viewport::Get_BoxColor(2)
        : StatusColor;
    _Snapshot._Voxel._ChunkColor = InSnapshot._Status == ck::voxelnav::EDebugSnapshotStatus::Current
        ? FLinearColor{0.7f, 0.35f, 1.0f, 0.85f}
        : StatusColor;
    _Snapshot._Voxel._CanDrawRetainedGeometry =
        ck_crowd_debugger_3d_viewport::Get_CanDrawRetainedGeometry(InSnapshot._Status);

    for (const auto& Cell : InSnapshot._Occupied._Cells)
    {
        _Snapshot._Voxel._Cells._Occupied.Add(Cell._Bounds);
    }
    for (const auto& Cell : InSnapshot._MergedFree._Cells)
    {
        _Snapshot._Voxel._Cells._MergedFree.Add(Cell._Bounds);
    }
    for (const auto& Cell : InSnapshot._RawFree._Cells)
    {
        _Snapshot._Voxel._Cells._RawFree.Add(Cell._Bounds);
    }
    for (const auto& Chunk : InSnapshot._Chunks)
    {
        _Snapshot._Voxel._Chunks.Add(Chunk._Bounds);
    }
    for (const auto& Portal : InSnapshot._Portals)
    {
        _Snapshot._Voxel._Portals.Add({Portal._From, Portal._To, Portal._ConnectionPoint, true});
    }

    constexpr auto IsLayerVisible = true;
    _Snapshot._Voxel._LayerVisibility.Add(ECkCrowdDebugger_3dVoxelLayer::Occupied, IsLayerVisible);
    _Snapshot._Voxel._LayerVisibility.Add(ECkCrowdDebugger_3dVoxelLayer::MergedFree, IsLayerVisible);
    _Snapshot._Voxel._LayerVisibility.Add(ECkCrowdDebugger_3dVoxelLayer::RawFree, IsLayerVisible);
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Clear_VoxelNavSnapshot()
    -> void
{
    // Idempotent, and deliberately does NOT touch the world epoch.
    //
    // A map with no VoxelNav volume calls this from the window's voxel refresh forever. This used
    // to bump _WorldEpoch unconditionally, which the scene adapter reads as "the world changed" and
    // answers with Reset_ForWorldChange — dropping _RecastRevision, _RibbonRevision and every cached
    // static instance. The next reconcile therefore rebuilt the navmesh mesh AND every path-network
    // ribbon mesh from scratch, ~313ms, and left the frame slow enough that the refresh's own 0.25s
    // throttle had always elapsed by the next frame. It fired every frame and could not recover.
    //
    // Clearing voxel data is not a world change. The voxel section is revision-gated on its own, so
    // emptying the snapshot is enough for the adapter to drop the voxel items and leave the navmesh
    // and ribbons — which are static — cached exactly as they should be.
    if (NOT _HasVoxelSnapshot)
    { return; }

    _HasVoxelSnapshot = false;
    _Snapshot._Voxel = {};
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Notify_WorldChanged()
    -> void
{
    // The real world-change signal, split out from Clear_VoxelNavSnapshot so that "this map has no
    // voxel volume" can no longer masquerade as "the world was replaced". Item keys and every
    // revision cache in the adapter are keyed to the epoch, so a genuine world change must bump it
    // even when there was no voxel data to clear.
    ++_WorldEpoch;
    _Snapshot._WorldEpoch = _WorldEpoch;
    _Snapshot._Voxel = {};
    _HasVoxelSnapshot = false;
    _AgentRevision = 0;
    _QueueSignature = 0;
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Set_AgentSnapshots(
        const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents,
        const FCk_Handle& InSelectedHandle)
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SetAgentSnapshots);
    _Snapshot._WorldEpoch = _WorldEpoch;
    _Snapshot._Agents.Reset(InAgents.Num());
    _Snapshot._SelectedIdentity.Reset();
    for (const auto& Agent : InAgents)
    {
        const auto Identity = static_cast<uint64>(Agent.Handle.Get_Entity().Get_ID());
        const auto Radius = FMath::Max(Agent.Radius, 12.0f);
        const auto Height = FMath::Max(Agent.Height, Radius * 2.0f);
        const auto Selected = ck::IsValid(InSelectedHandle) && Agent.Handle == InSelectedHandle;
        _Snapshot._Agents.Add(FCkCrowdDebugger_3dAgentSnapshot{
            Identity,
            Agent.Position,
            Agent.Velocity,
            Radius,
            Height,
            Selected ? FLinearColor{1.0f, 0.9f, 0.2f, 1.0f}
                : ck_crowd_debugger_3d_viewport::Get_AgentColor(Agent.Status),
            Agent.PlannedPath});
        if (Selected)
        {
            _Snapshot._SelectedIdentity = Identity;
        }
    }

    if (_CommonViewport.IsValid())
    {
        _CommonViewport->Set_SelectionKeys(_Snapshot._SelectedIdentity.IsSet()
            ? TArray<uint64>{*_Snapshot._SelectedIdentity}
            : TArray<uint64>{});
    }

    // Only mark dirty when something the scene actually renders has changed. Set_NavmeshTriangles
    // already gates on a revision; agents did not, so "dirty" meant "a frame happened" and a full
    // reconcile ran unconditionally. The stamp covers exactly the fields copied into the snapshot
    // above, so nothing the scene draws can go stale behind it.
    // Unqualified GetTypeHash throughout: FVector is UE::Math::TVector<double>, so its overload
    // lives in UE::Math and is reached by ADL — a ::-qualified call does not find it.
    auto Revision = GetTypeHash(_Snapshot._Agents.Num());
    Revision = HashCombineFast(Revision, _Snapshot._SelectedIdentity.IsSet()
        ? GetTypeHash(*_Snapshot._SelectedIdentity) : 0u);
    for (const auto& Agent : _Snapshot._Agents)
    {
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._Identity));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._Position));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._Velocity));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._Radius));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._Height));
        // FLinearColor has no GetTypeHash overload; its components do.
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._StatusColor.R));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._StatusColor.G));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._StatusColor.B));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._StatusColor.A));
        Revision = HashCombineFast(Revision, GetTypeHash(Agent._PlannedPath.Num()));
        for (const auto& Point : Agent._PlannedPath)
        { Revision = HashCombineFast(Revision, GetTypeHash(Point)); }
    }

    if (Revision == _AgentRevision)
    { return; }

    _AgentRevision = Revision;
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Set_NavmeshTriangles(
        const TArray<FVector>& InNavTriVerts,
        uint64 InGeometryRevision)
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SetNavmeshTriangles);
    if (_Snapshot._Recast._Revision == InGeometryRevision)
    {
        return;
    }
    _Snapshot._Recast._Revision = InGeometryRevision;
    _Snapshot._Recast._Triangles = InNavTriVerts;
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Set_PathNetworkRibbons(
        const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>& InRibbons)
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SetPathNetworkRibbons);
    const auto* Settings = GetDefault<UCkCrowdDebuggerSettings>();
    const auto Opacity = Settings != nullptr
        ? FMath::Clamp(Settings->PathNetworkOpacity, 0.0f, 1.0f)
        : 0.0f;
    ck_crowd_debugger_3d_viewport::Trace_PathNetworkPublication(
        InRibbons,
        _Snapshot._Recast._Triangles.Num() / 3,
        Opacity);

    auto Signature = GetTypeHash(InRibbons.Num());
    for (const auto& Ribbon : InRibbons)
    {
        Signature = HashCombineFast(Signature, GetTypeHash(Ribbon.Points.Num()));
        for (const auto& Point : Ribbon.Points)
        {
            Signature = HashCombineFast(Signature, GetTypeHash(Point));
        }
        for (const auto HalfWidth : Ribbon.HalfWidths)
        {
            Signature = HashCombineFast(Signature, GetTypeHash(HalfWidth));
        }
    }
    const auto GeometryChanged = _PathNetworkSignature != Signature;
    const auto OpacityChanged = NOT FMath::IsNearlyEqual(_Snapshot._PathNetwork._Opacity, Opacity);
    if (NOT GeometryChanged && NOT OpacityChanged)
    {
        return;
    }

    _Snapshot._PathNetwork._Opacity = Opacity;
    _Snapshot._PathNetwork._Revision = ++_PathNetworkRevision;
    if (GeometryChanged)
    {
        _PathNetworkSignature = Signature;
        _Snapshot._PathNetwork._Ribbons.Reset(InRibbons.Num());
        for (const auto& Ribbon : InRibbons)
        {
            _Snapshot._PathNetwork._Ribbons.Add({Ribbon.Points, Ribbon.HalfWidths});
        }
    }
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Set_QueueSnapshots(const TArray<FCkCrowdDebugger_QueueSnapshot>& InQueues)
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SetQueueSnapshots);
    // Gated the same way Set_PathNetworkRibbons already is. This setter used to mark the snapshot
    // dirty unconditionally on every frame, which on its own forced a full scene reconcile even
    // when no queue had moved — and would have defeated the agent gate below it. The stamp covers
    // the reservation geometry too, not just the queue Revision, so a rank change cannot go stale
    // behind a producer that forgot to bump.
    auto Signature = GetTypeHash(InQueues.Num());
    for (const auto& Queue : InQueues)
    {
        Signature = HashCombineFast(Signature, GetTypeHash(Queue.Identity));
        Signature = HashCombineFast(Signature, GetTypeHash(Queue.Revision));
        Signature = HashCombineFast(Signature, GetTypeHash(Queue.State));
        for (const auto& Origin : Queue.Origins)
        {
            Signature = HashCombineFast(Signature, GetTypeHash(Origin.Location));
            Signature = HashCombineFast(Signature, GetTypeHash(Origin.Forward));
        }
        for (const auto& Member : Queue.Members)
        {
            Signature = HashCombineFast(Signature, GetTypeHash(Member.AgentIdentity));
            Signature = HashCombineFast(Signature, GetTypeHash(Member.OriginIndex));
            Signature = HashCombineFast(Signature, GetTypeHash(Member.Rank));
            Signature = HashCombineFast(Signature, GetTypeHash(Member.ReservationLocation));
            Signature = HashCombineFast(Signature, GetTypeHash(Member.ReservationForward));
            Signature = HashCombineFast(Signature, GetTypeHash(Member.HasReservation ? 1 : 0));
        }
    }
    if (Signature == _QueueSignature)
    { return; }
    _QueueSignature = Signature;

    auto Queues = TArray<FCkCrowdDebugger_3dQueueSnapshot>{};
    Queues.Reserve(InQueues.Num());
    for (const auto& Queue : InQueues)
    {
        auto Copy = FCkCrowdDebugger_3dQueueSnapshot{};
        Copy._Identity = Queue.Identity;
        Copy._Revision = Queue.Revision;
        Copy._DebugName = Queue.DebugName;
        Copy._Category = Queue.Category;
        Copy._State = Queue.State;
        for (const auto& Origin : Queue.Origins)
        {
            Copy._Origins.Add({Origin.Location, Origin.Location + Origin.Forward.GetSafeNormal() * 100.0f});
        }
        for (const auto& Member : Queue.Members)
        {
            const auto SlotWithinQueue = HashCombineFast(GetTypeHash(Member.OriginIndex), GetTypeHash(Member.Rank));
            Copy._Members.Add({Member.AgentIdentity,
                HashCombineFast(GetTypeHash(Queue.Identity), SlotWithinQueue), Member.OriginIndex, Member.Rank,
                Member.ReservationLocation, Member.ReservationForward, Member.HasReservation});
        }
        Queues.Add(MoveTemp(Copy));
    }
    _Snapshot._Queues = MoveTemp(Queues);
    _SnapshotDirty = true;
}

auto
    SCkCrowdDebugger_3dViewport::
    Apply_CameraPreset(ECkCrowdDebugger_CameraPreset InPreset)
    -> void
{
    if (NOT _CommonViewport.IsValid())
    {
        return;
    }
    _CommonViewport->Apply_CameraPreset(static_cast<ECkDebug3dCameraPreset>(InPreset));
}

auto
    SCkCrowdDebugger_3dViewport::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (_SnapshotDirty && _PreviewAdapter.IsValid())
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_ViewportReconcile);
        _SnapshotDirty = NOT _PreviewAdapter->Reconcile(_Snapshot);
    }
}
