#include "CkNavmeshDebugDraw/Subsystem/CkNavmeshDebugDraw_Subsystem.h"

#if WITH_CK_NAVMESH_DEBUG_DRAW

#include "CkNavmeshDebugDraw/CkNavmeshDebugDraw_Log.h"
#include "CkNavmeshDebugDraw/Rendering/CkNavmeshDebugDraw_MeshComponent.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Time/CkTime.h"
#include "CkCore/Validation/CkIsValid.h"

#include <AI/NavigationSystemBase.h>
#include <Containers/Ticker.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <GameTime.h>
#include <NavMesh/RecastNavMesh.h>
#include <NavigationData.h>
#include <NavigationSystem.h>
#include <ProfilingDebugging/CpuProfilerTrace.h>
#include <UObject/StrongObjectPtr.h>

namespace ck_navmesh_debug_draw_subsystem
{
constexpr auto BucketCellSize = 5000.0;
constexpr auto WorkBudget = ck::time::Milliseconds(0.5);
constexpr auto SourceRetryInterval = ck::time::Milliseconds(250.0);
constexpr auto AreaPreviewInterval = ck::time::Milliseconds(500.0);
constexpr auto NavigationSettleInterval = ck::time::Seconds(2.0);
constexpr auto DefaultNavmeshColor = FColor{140, 255, 0, 164};
constexpr auto FillAlpha = uint8{160};
constexpr auto MaxGhostTrianglesPerBucket = 4096;
constexpr auto MaxGhostTrianglesTotal = 65536;
constexpr auto MaxAreaPreviewBucketsPerScan = 8;

struct FBucketWork
{
    uint64 _Key = 0;
    double _DistanceSquared = 0.0;
    TArray<FNavTileRef> _TileRefs;
    TArray<FIntVector> _TileCoordinates;
    int32 _NextTileIndex = 0;
    TArray<TArray<FVector3f>> _AreaTriangleVertices;
    TArray<FVector3f> _EdgeVertices;
    bool _WasRejected = false;
};

struct FCompletedBucketSnapshot
{
    FCk_NavmeshDebugDraw_MeshSnapshot _Snapshot;
    TArray<FIntVector> _TileCoordinates;
};

auto
GetBucketKey(
    const FVector& InWorldPosition) -> uint64
{
    const auto X = FMath::FloorToInt(InWorldPosition.X / BucketCellSize);
    const auto Y = FMath::FloorToInt(InWorldPosition.Y / BucketCellSize);
    return (static_cast<uint64>(static_cast<uint32>(X)) << 32) |
           static_cast<uint64>(static_cast<uint32>(Y));
}

auto
RetainsPublishedTileCoverage(
    const TArray<FIntVector>& InPublishedTileCoordinates,
    const TArray<FIntVector>& InCandidateTileCoordinates) -> bool
{
    return NOT InPublishedTileCoordinates.ContainsByPredicate(
        [&InCandidateTileCoordinates](const FIntVector& InPublishedCoordinate)
        {
            return NOT InCandidateTileCoordinates.Contains(InPublishedCoordinate);
        });
}

auto
IsFinite(
    const FVector& InValue) -> bool
{
    return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
}

auto
TryAppendTileGeometry(
    const FRecastDebugGeometry& InGeometry,
    const FVector& InDrawOffset,
    const bool InGatherEdges,
    TArray<TArray<FVector3f>>& InOutAreaTriangleVertices,
    TArray<FVector3f>& InOutEdgeVertices) -> bool
{
    const auto AreaStorageIsReady = InOutAreaTriangleVertices.Num() == RECAST_MAX_AREAS;
    CK_ENSURE_IF_NOT(AreaStorageIsReady,
                     TEXT("Ck navmesh draw requires [{}] area buffers"),
                     RECAST_MAX_AREAS)
    {
        return false;
    }

    auto PendingAreaTriangleVertices = TArray<TArray<FVector3f>>{};
    PendingAreaTriangleVertices.SetNum(RECAST_MAX_AREAS);
    auto PendingEdgeVertices = TArray<FVector3f>{};
    for (auto AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; ++AreaIndex)
    {
        const auto& Indices = InGeometry.AreaIndices[AreaIndex];
        const auto HasTriangleIndices = Indices.Num() % 3 == 0;
        CK_ENSURE_IF_NOT(HasTriangleIndices,
                         TEXT("Ck navmesh draw rejected non-triangular Recast area indices [{}]"),
                         Indices.Num())
        {
            return false;
        }

        auto& PendingVertices = PendingAreaTriangleVertices[AreaIndex];
        PendingVertices.Reserve(Indices.Num());
        if (InGatherEdges)
        {
            PendingEdgeVertices.Reserve(PendingEdgeVertices.Num() + Indices.Num() * 2);
        }
        for (auto Index = 0; Index < Indices.Num(); Index += 3)
        {
            const auto AIndex = Indices[Index];
            const auto BIndex = Indices[Index + 1];
            const auto CIndex = Indices[Index + 2];
            const auto IndicesAreValid = InGeometry.MeshVerts.IsValidIndex(AIndex) &&
                                         InGeometry.MeshVerts.IsValidIndex(BIndex) &&
                                         InGeometry.MeshVerts.IsValidIndex(CIndex);
            CK_ENSURE_IF_NOT(IndicesAreValid, TEXT("Ck navmesh draw rejected an invalid Recast vertex index"))
            {
                return false;
            }

            const auto A = InGeometry.MeshVerts[AIndex] + InDrawOffset;
            const auto B = InGeometry.MeshVerts[BIndex] + InDrawOffset;
            const auto C = InGeometry.MeshVerts[CIndex] + InDrawOffset;
            const auto TriangleIsFinite = IsFinite(A) && IsFinite(B) && IsFinite(C);
            CK_ENSURE_IF_NOT(TriangleIsFinite, TEXT("Ck navmesh draw rejected non-finite Recast geometry"))
            {
                return false;
            }

            if (FVector::CrossProduct(B - A, C - A).SizeSquared() <= SMALL_NUMBER)
            {
                continue;
            }

            PendingVertices.Append({FVector3f{A}, FVector3f{B}, FVector3f{C}});
            if (InGatherEdges)
            {
                PendingEdgeVertices.Append({FVector3f{A}, FVector3f{B},
                                            FVector3f{B}, FVector3f{C},
                                            FVector3f{C}, FVector3f{A}});
            }
        }
    }

    for (auto AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; ++AreaIndex)
    {
        InOutAreaTriangleVertices[AreaIndex].Append(MoveTemp(PendingAreaTriangleVertices[AreaIndex]));
    }
    InOutEdgeVertices.Append(MoveTemp(PendingEdgeVertices));
    return true;
}

auto HasTriangleGeometry(const FBucketWork& InBucket) -> bool
{
    return InBucket._AreaTriangleVertices.ContainsByPredicate([](const TArray<FVector3f>& InVertices)
    {
        return NOT InVertices.IsEmpty();
    });
}

auto
ResolveViewLocation(
    UWorld& InWorld) -> FVector
{
    auto* PlayerController = InWorld.GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PlayerController))
    {
        return FVector::ZeroVector;
    }

    auto Location = FVector::ZeroVector;
    auto Rotation = FRotator::ZeroRotator;
    PlayerController->GetPlayerViewPoint(Location, Rotation);
    return Location;
}
} // namespace ck_navmesh_debug_draw_subsystem

struct UCk_NavmeshDebugDraw_Subsystem_UE::FImpl
{
    bool _IsEnabled = false;
    bool _IsScanning = false;
    bool _IsAreaPreviewScan = false;
    bool _IsWaitingForSource = false;
    bool _IsWaitingForNavigationSettle = false;
    bool _RefreshRequested = false;
    bool _ScanHadRejectedBucket = false;
    bool _ScanInvalidated = false;
    uint64 _ScanRevision = 0;
    uint64 _ProcessedTileCount = 0;
    uint64 _ProxyReplacementCount = 0;
    int32 _GhostTriangleCount = 0;

    TWeakObjectPtr<UNavigationSystemV1> _NavigationSystem;
    TWeakObjectPtr<ARecastNavMesh> _NavData;
    FTSTicker::FDelegateHandle _TickerHandle;
    FTSTicker::FDelegateHandle _AreaPreviewTickerHandle;
    FTSTicker::FDelegateHandle _NavigationSettleTickerHandle;
    FTSTicker::FDelegateHandle _GhostFadeTickerHandle;
    double _NavigationSettleDeadlineRealTimeSeconds = -1.0;
    double _GhostFadeTickerDueRealTimeSeconds = -1.0;
    FCk_Time _SourceRetryRemaining;
    FVector _DrawOffset = FVector::ZeroVector;
    TArray<FColor> _AreaColors;

    TMap<uint64, ck_navmesh_debug_draw_subsystem::FBucketWork> _PendingBuckets;
    TArray<uint64> _PendingBucketKeys;
    int32 _CurrentBucketIndex = 0;
    TSet<uint64> _SeenBucketKeys;
    TSet<uint64> _BucketKeysAwaitingRetry;
    TSet<uint64> _GhostBucketKeys;
    TMap<uint64, ck_navmesh_debug_draw_subsystem::FCompletedBucketSnapshot> _CompletedBucketSnapshots;

    TMap<uint64, TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>> _Components;
    TMap<uint64, TArray<FIntVector>> _PublishedBucketTileCoordinates;
};

#endif // WITH_CK_NAVMESH_DEBUG_DRAW

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    ShouldCreateSubsystem(
        UObject* InOuter) const
    -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return Super::ShouldCreateSubsystem(InOuter);
#else
    return false;
#endif
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Deinitialize()
    -> void
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    DoDeactivate();
#endif
    Super::Deinitialize();
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Set_IsEnabled(
        bool InIsEnabled)
    -> void
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    if (InIsEnabled == Get_IsEnabled())
    {
        return;
    }

    if (InIsEnabled)
    {
        DoActivate();
    }
    else
    {
        DoDeactivate();
    }
#endif
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Request_Refresh()
    -> void
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    if (NOT Get_IsEnabled())
    {
        return;
    }

    DoCancelNavigationSettle();
    DoCancelAreaPreview();

    if (_Impl->_IsScanning)
    {
        _Impl->_RefreshRequested = true;
        _Impl->_ScanInvalidated = true;
        return;
    }

    DoStartScan();
    DoEnsureWorkTicker();
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_IsEnabled() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_IsEnabled;
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_IsScanning() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_IsScanning;
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_IsAreaPreviewScanning() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_IsScanning && _Impl->_IsAreaPreviewScan;
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_IsWaitingForNavigationSettle() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_IsWaitingForNavigationSettle;
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_HasActiveWorkTicker() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_TickerHandle.IsValid();
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_HasActiveAreaPreviewTicker() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_AreaPreviewTickerHandle.IsValid();
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_HasActiveNavigationSettleTicker() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_NavigationSettleTickerHandle.IsValid();
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_HasActiveGhostFadeTicker() const -> bool
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() && _Impl->_GhostFadeTickerHandle.IsValid();
#else
    return false;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_RetainedBucketCount() const -> int32
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() ? _Impl->_Components.Num() : 0;
#else
    return 0;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_StaleBucketCount() const -> int32
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    if (NOT _Impl.IsValid())
    {
        return 0;
    }

    auto StaleBucketCount = 0;
    for (const auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()) && ComponentPair.Value->Get_IsStale())
        {
            ++StaleBucketCount;
        }
    }
    return StaleBucketCount;
#else
    return 0;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_AreaPreviewTriangleCount() const -> int32
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    if (NOT _Impl.IsValid())
    {
        return 0;
    }

    auto AreaPreviewTriangleCount = 0;
    for (const auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()))
        {
            AreaPreviewTriangleCount += ComponentPair.Value->Get_AreaPreviewTriangleCount();
        }
    }
    return AreaPreviewTriangleCount;
#else
    return 0;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_GhostTriangleCount() const -> int32
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() ? _Impl->_GhostTriangleCount : 0;
#else
    return 0;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_ProcessedTileCount() const -> uint64
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() ? _Impl->_ProcessedTileCount : 0;
#else
    return 0;
#endif
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Get_ProxyReplacementCount() const -> uint64
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    return _Impl.IsValid() ? _Impl->_ProxyReplacementCount : 0;
#else
    return 0;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Test_PublishRetainedBucket(
        const uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        TArray<FIntVector> InTileCoordinates)
    -> bool
{
    if (NOT _Impl.IsValid())
    {
        _Impl = MakeShared<FImpl>();
    }

    if (_Impl->_Components.Contains(InBucketKey))
    {
        return DoTryApplySnapshotToExistingBucket(
            InBucketKey,
            MoveTemp(InSnapshot),
            InTileCoordinates);
    }

    auto* Component = NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>(GetWorld());
    if (ck::Is_NOT_Valid(Component))
    {
        return false;
    }

    auto ComponentOwner = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{Component};
    if (NOT Component->TryApplySnapshot(
        MoveTemp(InSnapshot),
        ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesPerBucket))
    {
        Component->DestroyComponent();
        return false;
    }

    _Impl->_Components.Emplace(InBucketKey, MoveTemp(ComponentOwner));
    _Impl->_PublishedBucketTileCoordinates.Emplace(InBucketKey, MoveTemp(InTileCoordinates));
    return true;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Test_ReconcileCompletedScan(
        const TSet<uint64>& InSeenBucketKeys)
    -> void
{
    if (NOT _Impl.IsValid())
    {
        _Impl = MakeShared<FImpl>();
    }

    _Impl->_SeenBucketKeys = InSeenBucketKeys;
    DoReconcileCompletedScan();
    _Impl->_SeenBucketKeys.Reset();
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_GetRetainedBucketTriangleCount(
    const uint64 InBucketKey) const -> int32
{
    if (NOT _Impl.IsValid())
    {
        return 0;
    }

    const auto* Component = _Impl->_Components.Find(InBucketKey);
    return Component != nullptr && ck::IsValid(Component->Get())
        ? Component->Get()->Get_TriangleCount()
        : 0;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_GetRetainedBucketRevision(
    const uint64 InBucketKey) const -> uint64
{
    if (NOT _Impl.IsValid())
    {
        return 0;
    }

    const auto* Component = _Impl->_Components.Find(InBucketKey);
    return Component != nullptr && ck::IsValid(Component->Get())
        ? Component->Get()->Get_Revision()
        : 0;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    Test_StageCompletedBucket(
        const uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        TArray<FIntVector> InTileCoordinates)
    -> void
{
    if (NOT _Impl.IsValid())
    {
        _Impl = MakeShared<FImpl>();
    }
    _Impl->_CompletedBucketSnapshots.Emplace(
        InBucketKey,
        ck_navmesh_debug_draw_subsystem::FCompletedBucketSnapshot{
            MoveTemp(InSnapshot),
            MoveTemp(InTileCoordinates)});
    _Impl->_SeenBucketKeys.Add(InBucketKey);
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_CommitCompletedScan() -> bool
{
    if (NOT _Impl.IsValid())
    {
        return false;
    }
    const auto WasCommitted = DoCommitCompletedScan();
    _Impl->_CompletedBucketSnapshots.Reset();
    _Impl->_SeenBucketKeys.Reset();
    return WasCommitted;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_DiscardInvalidatedCompletedScan() -> void
{
    if (NOT _Impl.IsValid())
    {
        return;
    }
    _Impl->_ScanInvalidated = true;
    _Impl->_RefreshRequested = true;
    DoFinishScan();
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_GetVisibleBucketCount() const -> int32
{
    if (NOT _Impl.IsValid())
    {
        return 0;
    }

    auto VisibleBucketCount = 0;
    for (const auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()) && ComponentPair.Value->IsVisible())
        {
            ++VisibleBucketCount;
        }
    }
    return VisibleBucketCount;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_ScheduleNavigationSettleDelay(
    const double InCurrentRealTimeSeconds) -> void
{
    if (NOT _Impl.IsValid())
    {
        _Impl = MakeShared<FImpl>();
    }
    _Impl->_IsEnabled = true;
    DoScheduleNavigationSettle(InCurrentRealTimeSeconds);
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_GetNavigationSettleDeadlineRealTimeSeconds() const -> double
{
    return _Impl.IsValid() ? _Impl->_NavigationSettleDeadlineRealTimeSeconds : -1.0;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_Get_IsNavigationSettlePending() const -> bool
{
    return _Impl.IsValid() && _Impl->_IsWaitingForNavigationSettle;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_CanStartScan() const -> bool
{
    return _Impl.IsValid() && NOT _Impl->_IsWaitingForNavigationSettle;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_TickNavigationSettleDelay(
    const double InCurrentRealTimeSeconds) -> void
{
    if (DoTryCompleteNavigationSettle(InCurrentRealTimeSeconds) &&
        _Impl->_NavigationSettleTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_NavigationSettleTickerHandle);
        _Impl->_NavigationSettleTickerHandle.Reset();
    }
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_Get_IsRefreshRequested() const -> bool
{
    return _Impl.IsValid() && _Impl->_RefreshRequested;
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_ScheduleAreaPreviewDelay() -> void
{
    if (NOT _Impl.IsValid())
    {
        _Impl = MakeShared<FImpl>();
    }
    _Impl->_IsEnabled = true;
    DoScheduleAreaPreview();
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_PublishStagedAreaPreview() -> void
{
    if (_Impl.IsValid())
    {
        DoPublishAreaPreview();
        _Impl->_CompletedBucketSnapshots.Reset();
        _Impl->_SeenBucketKeys.Reset();
    }
}

auto UCk_NavmeshDebugDraw_Subsystem_UE::Test_GetRetainedBucketAreaPreviewTriangleCount(
    const uint64 InBucketKey) const -> int32
{
    if (NOT _Impl.IsValid())
    {
        return 0;
    }
    const auto* ComponentOwner = _Impl->_Components.Find(InBucketKey);
    const auto* Component = ComponentOwner != nullptr ? ComponentOwner->Get() : nullptr;
    return ck::IsValid(Component) ? Component->Get_AreaPreviewTriangleCount() : 0;
}
#endif

#if WITH_CK_NAVMESH_DEBUG_DRAW
auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoActivate()
    -> void
{
    const auto WorldIsValid = ck::IsValid(GetWorld()) && GetWorld()->IsGameWorld();
    CK_ENSURE_IF_NOT(WorldIsValid, TEXT("Ck navmesh draw activation requires a game world"))
    {
        return;
    }

    _Impl = MakeShared<FImpl>();
    _Impl->_IsEnabled = true;

    auto* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (ck::IsValid(NavigationSystem))
    {
        _Impl->_NavigationSystem = NavigationSystem;
        NavigationSystem->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(
            this,
            &UCk_NavmeshDebugDraw_Subsystem_UE::OnNavigationGenerationFinished);
    }

    DoStartScan();
    DoEnsureWorkTicker();
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoDeactivate()
    -> void
{
    if (NOT _Impl.IsValid())
    {
        return;
    }

    if (_Impl->_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_TickerHandle);
        _Impl->_TickerHandle.Reset();
    }

    if (_Impl->_GhostFadeTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_GhostFadeTickerHandle);
        _Impl->_GhostFadeTickerHandle.Reset();
    }
    _Impl->_GhostFadeTickerDueRealTimeSeconds = -1.0;

    DoCancelNavigationSettle();
    DoCancelAreaPreview();

    DoRemoveNavigationBinding();
    DoDestroyAllGeometry();
    _Impl.Reset();
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoStartScan(
        const bool InIsAreaPreview)
    -> bool
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkNavmeshDebugDraw_StartScan);

    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled || _Impl->_IsScanning ||
        (_Impl->_IsWaitingForNavigationSettle && NOT InIsAreaPreview))
    {
        return false;
    }

    auto* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    auto* NavData = ck::IsValid(NavigationSystem)
        ? Cast<ARecastNavMesh>(NavigationSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
        : nullptr;
    if (ck::Is_NOT_Valid(NavigationSystem) || ck::Is_NOT_Valid(NavData))
    {
        DoMarkAllGeometryStale();
        _Impl->_RefreshRequested = false;
        _Impl->_IsWaitingForSource = true;
        _Impl->_SourceRetryRemaining = ck_navmesh_debug_draw_subsystem::SourceRetryInterval;
        ck::navmesh_debug_draw::Warning(TEXT("No default Recast navmesh is available in world [{}]"), GetWorld());
        return false;
    }

    if (UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
    {
        if (InIsAreaPreview)
        {
            return false;
        }
        _Impl->_RefreshRequested = false;
        _Impl->_IsWaitingForSource = true;
        _Impl->_SourceRetryRemaining = ck_navmesh_debug_draw_subsystem::SourceRetryInterval;
        return false;
    }

    if (_Impl->_NavigationSystem.Get() != NavigationSystem)
    {
        DoRemoveNavigationBinding();
        _Impl->_NavigationSystem = NavigationSystem;
        NavigationSystem->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(
            this,
            &UCk_NavmeshDebugDraw_Subsystem_UE::OnNavigationGenerationFinished);
    }
    _Impl->_NavData = NavData;

    const auto& NavConfig = NavData->GetConfig();
    _Impl->_DrawOffset = FVector{0.0, 0.0, NavData->DrawOffset};
    if (NavData->DrawOffset != 0.0f)
    {
        _Impl->_DrawOffset.Z += NavConfig.AgentRadius / 10.0f;
    }
    _Impl->_AreaColors.SetNum(RECAST_MAX_AREAS);
    for (auto AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; ++AreaIndex)
    {
        _Impl->_AreaColors[AreaIndex] = NavData->GetAreaIDColor(static_cast<uint8>(AreaIndex));
        _Impl->_AreaColors[AreaIndex].A = ck_navmesh_debug_draw_subsystem::FillAlpha;
    }
    _Impl->_AreaColors[RECAST_DEFAULT_AREA] = NavConfig.Color.DWColor() > 0
        ? NavConfig.Color
        : ck_navmesh_debug_draw_subsystem::DefaultNavmeshColor;
    _Impl->_AreaColors[RECAST_DEFAULT_AREA].A = ck_navmesh_debug_draw_subsystem::FillAlpha;

    auto TileRefs = TArray<FNavTileRef>{};
    NavData->BeginBatchQuery();
    NavData->GetAllNavMeshTiles(TileRefs);

    const auto ViewLocation = ck_navmesh_debug_draw_subsystem::ResolveViewLocation(*GetWorld());
    auto PendingBuckets = TMap<uint64, ck_navmesh_debug_draw_subsystem::FBucketWork>{};
    for (const auto TileRef : TileRefs)
    {
        if (NOT TileRef.IsValid())
        {
            continue;
        }

        const auto Bounds = NavData->GetNavMeshTileBounds(TileRef);
        if (NOT Bounds.IsValid)
        {
            continue;
        }

        auto TileX = 0;
        auto TileY = 0;
        auto TileLayer = 0;
        if (NOT NavData->GetNavMeshTileXY(TileRef, TileX, TileY, TileLayer))
        {
            continue;
        }

        const auto Center = Bounds.GetCenter();
        const auto BucketKey = ck_navmesh_debug_draw_subsystem::GetBucketKey(Center);
        auto& Bucket = PendingBuckets.FindOrAdd(BucketKey);
        Bucket._Key = BucketKey;
        if (Bucket._AreaTriangleVertices.IsEmpty())
        {
            Bucket._AreaTriangleVertices.SetNum(RECAST_MAX_AREAS);
        }
        Bucket._DistanceSquared = Bucket._TileRefs.IsEmpty()
            ? FVector::DistSquared(ViewLocation, Center)
            : FMath::Min(Bucket._DistanceSquared, FVector::DistSquared(ViewLocation, Center));
        Bucket._TileRefs.Add(TileRef);
        Bucket._TileCoordinates.Add(FIntVector{TileX, TileY, TileLayer});
    }
    NavData->FinishBatchQuery();

    auto PendingKeys = TArray<uint64>{};
    PendingBuckets.GenerateKeyArray(PendingKeys);
    for (auto& BucketPair : PendingBuckets)
    {
        BucketPair.Value._TileRefs.Sort([](const FNavTileRef InLeft, const FNavTileRef InRight)
        {
            return static_cast<uint64>(InLeft) < static_cast<uint64>(InRight);
        });
        BucketPair.Value._TileCoordinates.Sort([](const FIntVector& InLeft, const FIntVector& InRight)
        {
            if (InLeft.X != InRight.X)
            {
                return InLeft.X < InRight.X;
            }
            if (InLeft.Y != InRight.Y)
            {
                return InLeft.Y < InRight.Y;
            }
            return InLeft.Z < InRight.Z;
        });
    }
    PendingKeys.Sort([&PendingBuckets](const uint64 InLeft, const uint64 InRight)
    {
        const auto LeftDistance = PendingBuckets[InLeft]._DistanceSquared;
        const auto RightDistance = PendingBuckets[InRight]._DistanceSquared;
        return LeftDistance == RightDistance ? InLeft < InRight : LeftDistance < RightDistance;
    });

    _Impl->_PendingBuckets = MoveTemp(PendingBuckets);
    _Impl->_PendingBucketKeys = MoveTemp(PendingKeys);
    _Impl->_CurrentBucketIndex = 0;
    _Impl->_SeenBucketKeys.Reset();
    for (const auto BucketKey : _Impl->_PendingBucketKeys)
    {
        _Impl->_SeenBucketKeys.Add(BucketKey);
    }
    if (NOT InIsAreaPreview)
    {
        _Impl->_RefreshRequested = false;
    }
    _Impl->_IsWaitingForSource = false;
    _Impl->_ScanHadRejectedBucket = false;
    _Impl->_ScanInvalidated = false;
    _Impl->_CompletedBucketSnapshots.Reset();
    _Impl->_IsScanning = true;
    _Impl->_IsAreaPreviewScan = InIsAreaPreview;
    ++_Impl->_ScanRevision;
    return true;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTick(
        float InDeltaSeconds)
    -> bool
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkNavmeshDebugDraw_Tick);

    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return false;
    }

    if (_Impl->_IsWaitingForSource)
    {
        _Impl->_SourceRetryRemaining -= FCk_Time{static_cast<double>(InDeltaSeconds)};
        if (_Impl->_SourceRetryRemaining <= FCk_Time::ZeroSecond())
        {
            DoStartScan();
        }
    }

    if (_Impl->_IsScanning)
    {
        const auto StartTime = FPlatformTime::Seconds();
        auto DidWork = false;
        while (_Impl->_IsScanning &&
               (NOT DidWork || FPlatformTime::Seconds() - StartTime <
                    ck_navmesh_debug_draw_subsystem::WorkBudget.Get_Seconds()))
        {
            DoProcessOneStep();
            DidWork = true;
        }
    }

    if (NOT _Impl->_IsScanning && NOT _Impl->_IsWaitingForNavigationSettle &&
        _Impl->_RefreshRequested)
    {
        DoStartScan();
    }

    const auto KeepTicking = _Impl->_IsScanning || _Impl->_IsWaitingForSource;
    if (NOT KeepTicking)
    {
        _Impl->_TickerHandle.Reset();
    }
    return KeepTicking;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTickAreaPreview(
        float InDeltaSeconds)
    -> bool
{
    static_cast<void>(InDeltaSeconds);
    TRACE_CPUPROFILER_EVENT_SCOPE(CkNavmeshDebugDraw_AreaPreview);

    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return false;
    }

    _Impl->_AreaPreviewTickerHandle.Reset();
    if (NOT _Impl->_IsWaitingForNavigationSettle || _Impl->_Components.IsEmpty())
    {
        return false;
    }

    if (_Impl->_IsScanning || UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
    {
        DoScheduleAreaPreview();
        return false;
    }

    if (NOT DoStartScan(true))
    {
        DoScheduleAreaPreview();
        return false;
    }
    DoEnsureWorkTicker();
    return false;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTickNavigationSettle(
        float InDeltaSeconds)
    -> bool
{
    static_cast<void>(InDeltaSeconds);
    TRACE_CPUPROFILER_EVENT_SCOPE(CkNavmeshDebugDraw_NavigationSettle);

    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return false;
    }

    _Impl->_NavigationSettleTickerHandle.Reset();
    if (NOT _Impl->_IsWaitingForNavigationSettle)
    {
        return false;
    }
    const auto CurrentRealTimeSeconds =
        FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds();
    if (NOT DoTryCompleteNavigationSettle(CurrentRealTimeSeconds))
    {
        const auto RemainingSeconds = FMath::Max(
            0.001,
            _Impl->_NavigationSettleDeadlineRealTimeSeconds - CurrentRealTimeSeconds);
        _Impl->_NavigationSettleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UCk_NavmeshDebugDraw_Subsystem_UE::DoTickNavigationSettle),
            static_cast<float>(RemainingSeconds));
        return false;
    }

    if (UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld()))
    {
        DoScheduleNavigationSettle(CurrentRealTimeSeconds);
        return false;
    }

    DoCancelAreaPreview();
    if (NOT _Impl->_IsScanning)
    {
        DoStartScan();
        DoEnsureWorkTicker();
    }
    return false;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoProcessOneStep()
    -> void
{
    if (_Impl->_CurrentBucketIndex >= _Impl->_PendingBucketKeys.Num())
    {
        DoFinishScan();
        return;
    }

    const auto BucketKey = _Impl->_PendingBucketKeys[_Impl->_CurrentBucketIndex];
    auto* Bucket = _Impl->_PendingBuckets.Find(BucketKey);
    const auto BucketIsValid = Bucket != nullptr;
    CK_ENSURE_IF_NOT(BucketIsValid, TEXT("Ck navmesh draw lost pending bucket [{}]"), BucketKey)
    {
        ++_Impl->_CurrentBucketIndex;
        return;
    }

    if (Bucket->_NextTileIndex >= Bucket->_TileRefs.Num())
    {
        DoFinalizeCurrentBucket();
        ++_Impl->_CurrentBucketIndex;
        return;
    }

    auto* NavData = _Impl->_NavData.Get();
    if (ck::Is_NOT_Valid(NavData))
    {
        if (_Impl->_IsAreaPreviewScan)
        {
            _Impl->_ScanHadRejectedBucket = true;
            DoFinishScan();
            return;
        }
        DoMarkAllGeometryStale();
        _Impl->_RefreshRequested = true;
        _Impl->_IsScanning = false;
        return;
    }

    const auto TileRef = Bucket->_TileRefs[Bucket->_NextTileIndex++];
    auto Geometry = FRecastDebugGeometry{};
    NavData->BeginBatchQuery();
    const auto TileIsCurrent = NavData->GetNavMeshTileBounds(TileRef).IsValid;
    // Unreal returns whether collection is complete, not whether this tile produced geometry.
    // A valid single-tile request intentionally returns false after appending its data.
    if (TileIsCurrent)
    {
        static_cast<void>(NavData->GetDebugGeometryForTile(Geometry, TileRef));
    }
    NavData->FinishBatchQuery();

    if (NOT TileIsCurrent)
    {
        Bucket->_WasRejected = true;
        for (auto& AreaVertices : Bucket->_AreaTriangleVertices)
        {
            AreaVertices.Reset();
        }
        Bucket->_EdgeVertices.Reset();
    }
    else if (NOT Bucket->_WasRejected)
    {
        Bucket->_WasRejected = NOT ck_navmesh_debug_draw_subsystem::TryAppendTileGeometry(
            Geometry,
            _Impl->_DrawOffset,
            NOT _Impl->_IsAreaPreviewScan,
            Bucket->_AreaTriangleVertices,
            Bucket->_EdgeVertices);
        if (Bucket->_WasRejected)
        {
            for (auto& AreaVertices : Bucket->_AreaTriangleVertices)
            {
                AreaVertices.Reset();
            }
            Bucket->_EdgeVertices.Reset();
        }
    }
    ++_Impl->_ProcessedTileCount;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoFinalizeCurrentBucket()
    -> void
{
    const auto BucketKey = _Impl->_PendingBucketKeys[_Impl->_CurrentBucketIndex];
    auto* Bucket = _Impl->_PendingBuckets.Find(BucketKey);
    if (Bucket == nullptr)
    {
        return;
    }

    const auto BucketHasUsableGeometry = NOT Bucket->_WasRejected &&
        ck_navmesh_debug_draw_subsystem::HasTriangleGeometry(*Bucket);
    if (NOT BucketHasUsableGeometry)
    {
        if (_Impl->_IsAreaPreviewScan)
        {
            _Impl->_ScanHadRejectedBucket = true;
            return;
        }
        const auto BucketAlreadyRetried = _Impl->_BucketKeysAwaitingRetry.Contains(BucketKey);
        if (NOT BucketAlreadyRetried)
        {
            _Impl->_BucketKeysAwaitingRetry.Add(BucketKey);
            _Impl->_ScanHadRejectedBucket = true;
            return;
        }

        _Impl->_BucketKeysAwaitingRetry.Remove(BucketKey);
        return;
    }

    auto Snapshot = FCk_NavmeshDebugDraw_MeshSnapshot{};
    Snapshot._Revision = _Impl->_ScanRevision;
    Snapshot._EdgeVertices = MoveTemp(Bucket->_EdgeVertices);
    for (auto AreaIndex = 0; AreaIndex < RECAST_MAX_AREAS; ++AreaIndex)
    {
        if (Bucket->_AreaTriangleVertices[AreaIndex].IsEmpty())
        {
            continue;
        }

        auto& AreaMesh = Snapshot._AreaMeshes.AddDefaulted_GetRef();
        AreaMesh._AreaId = static_cast<uint8>(AreaIndex);
        AreaMesh._Color = _Impl->_AreaColors[AreaIndex];
        AreaMesh._TriangleVertices = MoveTemp(Bucket->_AreaTriangleVertices[AreaIndex]);
    }

    if (const auto* PublishedTileCoordinates =
            _Impl->_PublishedBucketTileCoordinates.Find(BucketKey))
    {
        const auto RetainsPublishedCoverage =
            ck_navmesh_debug_draw_subsystem::RetainsPublishedTileCoverage(
                *PublishedTileCoordinates,
                Bucket->_TileCoordinates);
        if (NOT RetainsPublishedCoverage)
        {
            if (_Impl->_IsAreaPreviewScan)
            {
                _Impl->_ScanHadRejectedBucket = true;
                return;
            }
            const auto BucketAlreadyRetried = _Impl->_BucketKeysAwaitingRetry.Contains(BucketKey);
            if (NOT BucketAlreadyRetried)
            {
                _Impl->_BucketKeysAwaitingRetry.Add(BucketKey);
                _Impl->_ScanHadRejectedBucket = true;
            }
            else
            {
                _Impl->_BucketKeysAwaitingRetry.Remove(BucketKey);
            }
            return;
        }
    }

    _Impl->_CompletedBucketSnapshots.Emplace(
        BucketKey,
        ck_navmesh_debug_draw_subsystem::FCompletedBucketSnapshot{
            MoveTemp(Snapshot),
            Bucket->_TileCoordinates});
    if (NOT _Impl->_IsAreaPreviewScan)
    {
        _Impl->_BucketKeysAwaitingRetry.Remove(BucketKey);
    }
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTryApplySnapshotToExistingBucket(
        const uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        const TArray<FIntVector>& InTileCoordinates)
    -> bool
{
    auto* Existing = _Impl->_Components.Find(InBucketKey);
    if (Existing == nullptr || ck::Is_NOT_Valid(Existing->Get()))
    {
        return false;
    }

    const auto* PublishedTileCoordinates = _Impl->_PublishedBucketTileCoordinates.Find(InBucketKey);
    const auto RetainsPublishedCoverage = PublishedTileCoordinates == nullptr ||
        ck_navmesh_debug_draw_subsystem::RetainsPublishedTileCoverage(
            *PublishedTileCoordinates,
            InTileCoordinates);
    if (NOT RetainsPublishedCoverage)
    {
        if (Existing->Get()->Set_IsStale(true))
        {
            ++_Impl->_ProxyReplacementCount;
        }

        const auto BucketAlreadyRetried = _Impl->_BucketKeysAwaitingRetry.Contains(InBucketKey);
        if (NOT BucketAlreadyRetried)
        {
            _Impl->_BucketKeysAwaitingRetry.Add(InBucketKey);
            _Impl->_ScanHadRejectedBucket = true;
        }
        else
        {
            _Impl->_BucketKeysAwaitingRetry.Remove(InBucketKey);
        }
        return false;
    }

    const auto PreviousGhostTriangleCount = Existing->Get()->Get_GhostTriangleCount();
    const auto AvailableGhostTriangleCount = FMath::Max(
        0,
        ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesTotal -
        _Impl->_GhostTriangleCount +
        PreviousGhostTriangleCount);
    const auto BucketGhostTriangleLimit = FMath::Min(
        ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesPerBucket,
        AvailableGhostTriangleCount);
    const auto SnapshotWasApplied = Existing->Get()->TryApplySnapshot(
        MoveTemp(InSnapshot),
        BucketGhostTriangleLimit);
    if (SnapshotWasApplied)
    {
        _Impl->_GhostTriangleCount = _Impl->_GhostTriangleCount -
            PreviousGhostTriangleCount +
            Existing->Get()->Get_GhostTriangleCount();
        if (Existing->Get()->Get_GhostTriangleCount() > 0)
        {
            _Impl->_GhostBucketKeys.Add(InBucketKey);
        }
        else
        {
            _Impl->_GhostBucketKeys.Remove(InBucketKey);
        }
        DoEnsureGhostFadeTicker();
        _Impl->_BucketKeysAwaitingRetry.Remove(InBucketKey);
        _Impl->_PublishedBucketTileCoordinates.FindOrAdd(InBucketKey) = InTileCoordinates;
        if (Existing->Get()->DidLastApplyRecreateProxy())
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }
    return SnapshotWasApplied;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoCommitCompletedScan()
    -> bool
{
    auto CompletedBucketKeys = TArray<uint64>{};
    _Impl->_CompletedBucketSnapshots.GenerateKeyArray(CompletedBucketKeys);
    CompletedBucketKeys.Sort();

    for (const auto BucketKey : CompletedBucketKeys)
    {
        const auto* Candidate = _Impl->_CompletedBucketSnapshots.Find(BucketKey);
        const auto CandidateIsValid = Candidate != nullptr &&
            UCk_NavmeshDebugDraw_MeshComponent_UE::IsSnapshotValid(Candidate->_Snapshot);
        CK_ENSURE_IF_NOT(CandidateIsValid,
            TEXT("Ck navmesh draw rejected an invalid completed scan bucket [{}]"),
            BucketKey)
        {
            return false;
        }

        if (const auto* PublishedTileCoordinates =
                _Impl->_PublishedBucketTileCoordinates.Find(BucketKey))
        {
            const auto* Existing = _Impl->_Components.Find(BucketKey);
            if (Existing == nullptr || ck::Is_NOT_Valid(Existing->Get()))
            {
                return false;
            }
            if (NOT ck_navmesh_debug_draw_subsystem::RetainsPublishedTileCoverage(
                *PublishedTileCoordinates,
                Candidate->_TileCoordinates))
            {
                return false;
            }
        }
    }

    auto NewComponents = TMap<
        uint64,
        TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>>{};
    for (const auto BucketKey : CompletedBucketKeys)
    {
        if (_Impl->_Components.Contains(BucketKey))
        {
            continue;
        }

        auto* Candidate = _Impl->_CompletedBucketSnapshots.Find(BucketKey);
        auto* Component = NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>(
            GetWorld(),
            MakeUniqueObjectName(GetWorld(), UCk_NavmeshDebugDraw_MeshComponent_UE::StaticClass(),
                                 TEXT("CkNavmeshDebugDrawBucket")));
        const auto ComponentWasCreated = ck::IsValid(Component);
        CK_ENSURE_IF_NOT(ComponentWasCreated,
            TEXT("Ck navmesh draw failed to create a completed-scan bucket component"))
        {
            for (auto& NewComponentPair : NewComponents)
            {
                NewComponentPair.Value->DestroyComponent();
            }
            return false;
        }

        auto ComponentOwner = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{Component};
        const auto SnapshotWasApplied = Component->TryApplySnapshot(
            MoveTemp(Candidate->_Snapshot),
            ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesPerBucket);
        if (NOT SnapshotWasApplied)
        {
            Component->DestroyComponent();
            for (auto& NewComponentPair : NewComponents)
            {
                NewComponentPair.Value->DestroyComponent();
            }
            return false;
        }

        Component->SetVisibility(false, false);
        Component->RegisterComponentWithWorld(GetWorld());
        const auto ComponentWasRegistered = Component->IsRegistered();
        CK_ENSURE_IF_NOT(ComponentWasRegistered,
            TEXT("Ck navmesh draw failed to register a completed-scan bucket component"))
        {
            Component->DestroyComponent();
            for (auto& NewComponentPair : NewComponents)
            {
                NewComponentPair.Value->DestroyComponent();
            }
            return false;
        }
        NewComponents.Emplace(BucketKey, MoveTemp(ComponentOwner));
    }

    for (const auto BucketKey : CompletedBucketKeys)
    {
        if (NOT _Impl->_Components.Contains(BucketKey))
        {
            continue;
        }

        auto* Candidate = _Impl->_CompletedBucketSnapshots.Find(BucketKey);
        auto* Existing = _Impl->_Components.Find(BucketKey);
        const auto PreviousGhostTriangleCount = Existing->Get()->Get_GhostTriangleCount();
        const auto AvailableGhostTriangleCount = FMath::Max(
            0,
            ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesTotal -
            _Impl->_GhostTriangleCount +
            PreviousGhostTriangleCount);
        const auto BucketGhostTriangleLimit = FMath::Min(
            ck_navmesh_debug_draw_subsystem::MaxGhostTrianglesPerBucket,
            AvailableGhostTriangleCount);
        Existing->Get()->ApplyValidatedSnapshot(
            MoveTemp(Candidate->_Snapshot),
            BucketGhostTriangleLimit);
        _Impl->_GhostTriangleCount = _Impl->_GhostTriangleCount -
            PreviousGhostTriangleCount +
            Existing->Get()->Get_GhostTriangleCount();
        if (Existing->Get()->Get_GhostTriangleCount() > 0)
        {
            _Impl->_GhostBucketKeys.Add(BucketKey);
        }
        else
        {
            _Impl->_GhostBucketKeys.Remove(BucketKey);
        }
        _Impl->_BucketKeysAwaitingRetry.Remove(BucketKey);
        _Impl->_PublishedBucketTileCoordinates.FindOrAdd(BucketKey) =
            Candidate->_TileCoordinates;
        if (Existing->Get()->DidLastApplyRecreateProxy())
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }
    DoEnsureGhostFadeTicker();

    for (auto& NewComponentPair : NewComponents)
    {
        const auto BucketKey = NewComponentPair.Key;
        auto* Candidate = _Impl->_CompletedBucketSnapshots.Find(BucketKey);
        NewComponentPair.Value->SetVisibility(true, false);
        _Impl->_PublishedBucketTileCoordinates.Emplace(
            BucketKey,
            MoveTemp(Candidate->_TileCoordinates));
        _Impl->_Components.Emplace(BucketKey, MoveTemp(NewComponentPair.Value));
        ++_Impl->_ProxyReplacementCount;
    }

    for (auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()) && ComponentPair.Value->ClearAreaPreview())
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }

    DoReconcileCompletedScan();
    return true;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoReconcileCompletedScan()
    -> void
{
    for (auto& ComponentPair : _Impl->_Components)
    {
        if (_Impl->_SeenBucketKeys.Contains(ComponentPair.Key))
        {
            continue;
        }

        _Impl->_BucketKeysAwaitingRetry.Remove(ComponentPair.Key);
        if (ck::IsValid(ComponentPair.Value.Get()) && ComponentPair.Value->Set_IsStale(true))
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }

    for (auto It = _Impl->_BucketKeysAwaitingRetry.CreateIterator(); It; ++It)
    {
        if (NOT _Impl->_SeenBucketKeys.Contains(*It))
        {
            It.RemoveCurrent();
        }
    }
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoFinishScan()
    -> void
{
    if (_Impl->_IsAreaPreviewScan)
    {
        DoPublishAreaPreview();
        _Impl->_CompletedBucketSnapshots.Reset();
        _Impl->_PendingBuckets.Reset();
        _Impl->_PendingBucketKeys.Reset();
        _Impl->_SeenBucketKeys.Reset();
        _Impl->_CurrentBucketIndex = 0;
        _Impl->_IsScanning = false;
        _Impl->_IsAreaPreviewScan = false;
        _Impl->_ScanHadRejectedBucket = false;
        _Impl->_ScanInvalidated = false;
        return;
    }

    const auto NavigationIsStable = NOT UNavigationSystemV1::IsNavigationBeingBuilt(GetWorld());
    const auto ScanWasInvalidated = _Impl->_ScanInvalidated;
    const auto NeedsNavigationSettle = NOT NavigationIsStable ||
        (ScanWasInvalidated && _Impl->_IsWaitingForNavigationSettle);
    const auto ScanCanCommit = NavigationIsStable &&
        NOT _Impl->_ScanInvalidated &&
        NOT _Impl->_ScanHadRejectedBucket;
    if (ScanCanCommit)
    {
        if (NOT DoCommitCompletedScan())
        {
            _Impl->_RefreshRequested = true;
        }
    }
    else if (NOT NeedsNavigationSettle)
    {
        _Impl->_RefreshRequested = true;
    }

    _Impl->_CompletedBucketSnapshots.Reset();
    _Impl->_PendingBuckets.Reset();
    _Impl->_PendingBucketKeys.Reset();
    _Impl->_SeenBucketKeys.Reset();
    _Impl->_CurrentBucketIndex = 0;
    _Impl->_IsScanning = false;
    _Impl->_IsAreaPreviewScan = false;
    _Impl->_ScanInvalidated = false;

    if (NeedsNavigationSettle && NOT _Impl->_IsWaitingForNavigationSettle)
    {
        DoScheduleNavigationSettle(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());
    }
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoPublishAreaPreview()
    -> void
{
    auto CandidateKeys = _Impl->_PendingBucketKeys;
    if (CandidateKeys.IsEmpty())
    {
        _Impl->_CompletedBucketSnapshots.GenerateKeyArray(CandidateKeys);
        CandidateKeys.Sort();
    }

    auto PublishedBucketCount = 0;
    for (const auto CandidateKey : CandidateKeys)
    {
        if (PublishedBucketCount >= ck_navmesh_debug_draw_subsystem::MaxAreaPreviewBucketsPerScan)
        {
            break;
        }

        auto* Candidate = _Impl->_CompletedBucketSnapshots.Find(CandidateKey);
        auto* Existing = _Impl->_Components.Find(CandidateKey);
        if (Candidate == nullptr || Existing == nullptr || ck::Is_NOT_Valid(Existing->Get()))
        {
            continue;
        }
        ++PublishedBucketCount;

        auto PreviewAreaMeshes = TArray<FCk_NavmeshDebugDraw_AreaMesh>{};
        for (auto& AreaMesh : Candidate->_Snapshot._AreaMeshes)
        {
            if (AreaMesh._AreaId != RECAST_DEFAULT_AREA)
            {
                PreviewAreaMeshes.Add(MoveTemp(AreaMesh));
            }
        }

        if (PreviewAreaMeshes.IsEmpty())
        {
            if (Existing->Get()->ClearAreaPreview())
            {
                ++_Impl->_ProxyReplacementCount;
            }
            continue;
        }

        if (Existing->Get()->TryApplyAreaPreview(MoveTemp(PreviewAreaMeshes)))
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoEnsureWorkTicker()
    -> void
{
    if (_Impl->_TickerHandle.IsValid())
    {
        return;
    }

    _Impl->_TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_NavmeshDebugDraw_Subsystem_UE::DoTick));
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoScheduleAreaPreview()
    -> void
{
    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled || _Impl->_Components.IsEmpty() ||
        _Impl->_AreaPreviewTickerHandle.IsValid())
    {
        return;
    }

    _Impl->_AreaPreviewTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_NavmeshDebugDraw_Subsystem_UE::DoTickAreaPreview),
        static_cast<float>(ck_navmesh_debug_draw_subsystem::AreaPreviewInterval.Get_Seconds()));
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoCancelAreaPreview()
    -> void
{
    if (NOT _Impl.IsValid() || NOT _Impl->_AreaPreviewTickerHandle.IsValid())
    {
        return;
    }

    FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_AreaPreviewTickerHandle);
    _Impl->_AreaPreviewTickerHandle.Reset();
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoScheduleNavigationSettle(
        const double InCurrentRealTimeSeconds)
    -> void
{
    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return;
    }

    DoSetNavigationSettleDeadline(InCurrentRealTimeSeconds);
    if (_Impl->_NavigationSettleTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_NavigationSettleTickerHandle);
        _Impl->_NavigationSettleTickerHandle.Reset();
    }
    _Impl->_NavigationSettleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_NavmeshDebugDraw_Subsystem_UE::DoTickNavigationSettle),
        static_cast<float>(ck_navmesh_debug_draw_subsystem::NavigationSettleInterval.Get_Seconds()));
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoSetNavigationSettleDeadline(
        const double InCurrentRealTimeSeconds)
    -> void
{
    _Impl->_IsWaitingForNavigationSettle = true;
    _Impl->_IsWaitingForSource = false;
    _Impl->_RefreshRequested = false;
    _Impl->_NavigationSettleDeadlineRealTimeSeconds = InCurrentRealTimeSeconds +
        ck_navmesh_debug_draw_subsystem::NavigationSettleInterval.Get_Seconds();
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTryCompleteNavigationSettle(
        const double InCurrentRealTimeSeconds)
    -> bool
{
    if (NOT _Impl.IsValid() || NOT _Impl->_IsWaitingForNavigationSettle ||
        InCurrentRealTimeSeconds < _Impl->_NavigationSettleDeadlineRealTimeSeconds)
    {
        return false;
    }

    _Impl->_IsWaitingForNavigationSettle = false;
    _Impl->_NavigationSettleDeadlineRealTimeSeconds = -1.0;
    _Impl->_RefreshRequested = true;
    return true;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoCancelNavigationSettle()
    -> void
{
    if (NOT _Impl.IsValid())
    {
        return;
    }

    if (_Impl->_NavigationSettleTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_NavigationSettleTickerHandle);
        _Impl->_NavigationSettleTickerHandle.Reset();
    }
    _Impl->_IsWaitingForNavigationSettle = false;
    _Impl->_NavigationSettleDeadlineRealTimeSeconds = -1.0;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoTickGhostFade(
        float InDeltaSeconds)
    -> bool
{
    static_cast<void>(InDeltaSeconds);
    TRACE_CPUPROFILER_EVENT_SCOPE(CkNavmeshDebugDraw_GhostFadeTick);

    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return false;
    }

    _Impl->_GhostFadeTickerHandle.Reset();
    _Impl->_GhostFadeTickerDueRealTimeSeconds = -1.0;
    const auto CurrentRealTimeSeconds =
        FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds();

    auto RetainedGhostTriangleCount = 0;
    for (auto GhostBucketIt = _Impl->_GhostBucketKeys.CreateIterator(); GhostBucketIt; ++GhostBucketIt)
    {
        auto* ComponentOwner = _Impl->_Components.Find(*GhostBucketIt);
        auto* Component = ComponentOwner != nullptr ? ComponentOwner->Get() : nullptr;
        if (ck::Is_NOT_Valid(Component))
        {
            GhostBucketIt.RemoveCurrent();
            continue;
        }

        if (Component->Remove_ExpiredGhostTriangles(CurrentRealTimeSeconds))
        {
            ++_Impl->_ProxyReplacementCount;
        }
        const auto ComponentGhostTriangleCount = Component->Get_GhostTriangleCount();
        RetainedGhostTriangleCount += ComponentGhostTriangleCount;
        if (ComponentGhostTriangleCount == 0)
        {
            GhostBucketIt.RemoveCurrent();
        }
    }
    _Impl->_GhostTriangleCount = RetainedGhostTriangleCount;

    DoEnsureGhostFadeTicker();
    return false;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoEnsureGhostFadeTicker()
    -> void
{
    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return;
    }

    auto EarliestExpiryRealTimeSeconds = -1.0;
    for (const auto BucketKey : _Impl->_GhostBucketKeys)
    {
        const auto* ComponentOwner = _Impl->_Components.Find(BucketKey);
        const auto* Component = ComponentOwner != nullptr ? ComponentOwner->Get() : nullptr;
        if (ck::Is_NOT_Valid(Component))
        {
            continue;
        }
        const auto ComponentExpiryRealTimeSeconds =
            Component->Get_EarliestGhostExpiryRealTimeSeconds();
        if (ComponentExpiryRealTimeSeconds >= 0.0 &&
            (EarliestExpiryRealTimeSeconds < 0.0 ||
             ComponentExpiryRealTimeSeconds < EarliestExpiryRealTimeSeconds))
        {
            EarliestExpiryRealTimeSeconds = ComponentExpiryRealTimeSeconds;
        }
    }

    if (EarliestExpiryRealTimeSeconds < 0.0)
    {
        if (_Impl->_GhostFadeTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_GhostFadeTickerHandle);
            _Impl->_GhostFadeTickerHandle.Reset();
        }
        _Impl->_GhostFadeTickerDueRealTimeSeconds = -1.0;
        return;
    }

    if (_Impl->_GhostFadeTickerHandle.IsValid() &&
        _Impl->_GhostFadeTickerDueRealTimeSeconds <= EarliestExpiryRealTimeSeconds)
    {
        return;
    }
    if (_Impl->_GhostFadeTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_GhostFadeTickerHandle);
        _Impl->_GhostFadeTickerHandle.Reset();
    }

    const auto CurrentRealTimeSeconds =
        FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds();
    const auto DelaySeconds = FMath::Max(
        0.001,
        EarliestExpiryRealTimeSeconds - CurrentRealTimeSeconds);
    _Impl->_GhostFadeTickerDueRealTimeSeconds = EarliestExpiryRealTimeSeconds;
    _Impl->_GhostFadeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_NavmeshDebugDraw_Subsystem_UE::DoTickGhostFade),
        static_cast<float>(DelaySeconds));
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoMarkAllGeometryStale()
    -> void
{
    if (NOT _Impl.IsValid())
    {
        return;
    }

    for (auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()) && ComponentPair.Value->Set_IsStale(true))
        {
            ++_Impl->_ProxyReplacementCount;
        }
    }
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoDestroyAllGeometry()
    -> void
{
    if (NOT _Impl.IsValid())
    {
        return;
    }

    if (_Impl->_GhostFadeTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_Impl->_GhostFadeTickerHandle);
        _Impl->_GhostFadeTickerHandle.Reset();
    }
    _Impl->_GhostFadeTickerDueRealTimeSeconds = -1.0;
    DoCancelAreaPreview();

    for (auto& ComponentPair : _Impl->_Components)
    {
        if (ck::IsValid(ComponentPair.Value.Get()))
        {
            ComponentPair.Value->DestroyComponent();
        }
    }
    _Impl->_Components.Reset();
    _Impl->_PublishedBucketTileCoordinates.Reset();
    _Impl->_GhostTriangleCount = 0;
    _Impl->_PendingBuckets.Reset();
    _Impl->_PendingBucketKeys.Reset();
    _Impl->_SeenBucketKeys.Reset();
    _Impl->_BucketKeysAwaitingRetry.Reset();
    _Impl->_GhostBucketKeys.Reset();
    _Impl->_CompletedBucketSnapshots.Reset();
    _Impl->_ScanInvalidated = false;
    _Impl->_CurrentBucketIndex = 0;
    _Impl->_IsScanning = false;
    _Impl->_IsAreaPreviewScan = false;
    _Impl->_IsWaitingForSource = false;
    _Impl->_IsWaitingForNavigationSettle = false;
    _Impl->_NavigationSettleDeadlineRealTimeSeconds = -1.0;
}

auto
    UCk_NavmeshDebugDraw_Subsystem_UE::
    DoRemoveNavigationBinding()
    -> void
{
    auto* NavigationSystem = _Impl->_NavigationSystem.Get();
    if (ck::IsValid(NavigationSystem))
    {
        NavigationSystem->OnNavigationGenerationFinishedDelegate.RemoveDynamic(
            this,
            &UCk_NavmeshDebugDraw_Subsystem_UE::OnNavigationGenerationFinished);
    }
    _Impl->_NavigationSystem.Reset();
    _Impl->_NavData.Reset();
}

#endif // WITH_CK_NAVMESH_DEBUG_DRAW

// --------------------------------------------------------------------------------------------------------------------

void
    UCk_NavmeshDebugDraw_Subsystem_UE::
    OnNavigationGenerationFinished(
        ANavigationData* InNavigationData)
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    if (NOT _Impl.IsValid() || NOT _Impl->_IsEnabled)
    {
        return;
    }

    auto* CurrentNavData = _Impl->_NavData.Get();
    if (ck::IsValid(CurrentNavData) && InNavigationData != CurrentNavData)
    {
        auto* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
        auto* DefaultNavData = ck::IsValid(NavigationSystem)
            ? Cast<ARecastNavMesh>(NavigationSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
            : nullptr;
        if (InNavigationData != DefaultNavData)
        {
            return;
        }
    }

    if (_Impl->_IsScanning && NOT _Impl->_IsAreaPreviewScan)
    {
        _Impl->_ScanInvalidated = true;
    }
    DoScheduleNavigationSettle(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());
    DoScheduleAreaPreview();
#else
    static_cast<void>(InNavigationData);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
