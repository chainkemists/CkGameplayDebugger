#pragma once

#include <CoreMinimal.h>
#include <Subsystems/WorldSubsystem.h>

#include "CkNavmeshDebugDraw_Subsystem.generated.h"

class ANavigationData;
struct FCk_NavmeshDebugDraw_MeshSnapshot;

UCLASS(NotBlueprintable, Transient)
class CKNAVMESHDEBUGDRAW_API UCk_NavmeshDebugDraw_Subsystem_UE : public UWorldSubsystem
{
    GENERATED_BODY()

  public:
    virtual bool ShouldCreateSubsystem(UObject* InOuter) const override;
    virtual void Deinitialize() override;

  public:
    auto Set_IsEnabled(bool InIsEnabled) -> void;
    auto Request_Refresh() -> void;

    auto Get_IsEnabled() const -> bool;
    auto Get_IsScanning() const -> bool;
    auto Get_IsAreaPreviewScanning() const -> bool;
    auto Get_IsWaitingForNavigationSettle() const -> bool;
    auto Get_HasActiveWorkTicker() const -> bool;
    auto Get_HasActiveAreaPreviewTicker() const -> bool;
    auto Get_HasActiveNavigationSettleTicker() const -> bool;
    auto Get_HasActiveGhostFadeTicker() const -> bool;
    auto Get_RetainedBucketCount() const -> int32;
    auto Get_StaleBucketCount() const -> int32;
    auto Get_AreaPreviewTriangleCount() const -> int32;
    auto Get_GhostTriangleCount() const -> int32;
    auto Get_ProcessedTileCount() const -> uint64;
    auto Get_ProxyReplacementCount() const -> uint64;

#if WITH_DEV_AUTOMATION_TESTS && WITH_CK_NAVMESH_DEBUG_DRAW
    auto Test_PublishRetainedBucket(
        uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        TArray<FIntVector> InTileCoordinates) -> bool;
    auto Test_ReconcileCompletedScan(const TSet<uint64>& InSeenBucketKeys) -> void;
    auto Test_GetRetainedBucketTriangleCount(uint64 InBucketKey) const -> int32;
    auto Test_GetRetainedBucketRevision(uint64 InBucketKey) const -> uint64;
    auto Test_StageCompletedBucket(
        uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        TArray<FIntVector> InTileCoordinates) -> void;
    auto Test_CommitCompletedScan() -> bool;
    auto Test_DiscardInvalidatedCompletedScan() -> void;
    auto Test_GetVisibleBucketCount() const -> int32;
    auto Test_ScheduleNavigationSettleDelay(double InCurrentRealTimeSeconds) -> void;
    auto Test_GetNavigationSettleDeadlineRealTimeSeconds() const -> double;
    auto Test_Get_IsNavigationSettlePending() const -> bool;
    auto Test_CanStartScan() const -> bool;
    auto Test_TickNavigationSettleDelay(double InCurrentRealTimeSeconds) -> void;
    auto Test_Get_IsRefreshRequested() const -> bool;
    auto Test_ScheduleAreaPreviewDelay() -> void;
    auto Test_PublishStagedAreaPreview() -> void;
    auto Test_GetRetainedBucketAreaPreviewTriangleCount(uint64 InBucketKey) const -> int32;
#endif

  private:
    UFUNCTION()
    void OnNavigationGenerationFinished(ANavigationData* InNavigationData);

  private:
#if WITH_CK_NAVMESH_DEBUG_DRAW
    auto DoActivate() -> void;
    auto DoDeactivate() -> void;
    auto DoStartScan(bool InIsAreaPreview = false) -> bool;
    auto DoTick(float InDeltaSeconds) -> bool;
    auto DoTickAreaPreview(float InDeltaSeconds) -> bool;
    auto DoTickNavigationSettle(float InDeltaSeconds) -> bool;
    auto DoTickGhostFade(float InDeltaSeconds) -> bool;
    auto DoProcessOneStep() -> void;
    auto DoFinalizeCurrentBucket() -> void;
    auto DoTryApplySnapshotToExistingBucket(
        uint64 InBucketKey,
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        const TArray<FIntVector>& InTileCoordinates) -> bool;
    auto DoReconcileCompletedScan() -> void;
    auto DoFinishScan() -> void;
    auto DoCommitCompletedScan() -> bool;
    auto DoPublishAreaPreview() -> void;
    auto DoEnsureWorkTicker() -> void;
    auto DoScheduleAreaPreview() -> void;
    auto DoCancelAreaPreview() -> void;
    auto DoScheduleNavigationSettle(double InCurrentRealTimeSeconds) -> void;
    auto DoSetNavigationSettleDeadline(double InCurrentRealTimeSeconds) -> void;
    auto DoTryCompleteNavigationSettle(double InCurrentRealTimeSeconds) -> bool;
    auto DoCancelNavigationSettle() -> void;
    auto DoEnsureGhostFadeTicker() -> void;
    auto DoMarkAllGeometryStale() -> void;
    auto DoDestroyAllGeometry() -> void;
    auto DoRemoveNavigationBinding() -> void;

#endif

  private:
    struct FImpl;
    TSharedPtr<FImpl> _Impl;
};
