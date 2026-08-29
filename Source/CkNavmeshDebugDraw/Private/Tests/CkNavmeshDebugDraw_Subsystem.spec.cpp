#include "CkNavmeshDebugDraw/Subsystem/CkNavmeshDebugDraw_Subsystem.h"

#include "CkNavmeshDebugDraw/Rendering/CkNavmeshDebugDraw_MeshComponent.h"

#include <Engine/World.h>
#include <Misc/AutomationTest.h>
#include <NavMesh/RecastNavMesh.h>
#include <UObject/StrongObjectPtr.h>

#if WITH_DEV_AUTOMATION_TESTS && WITH_CK_NAVMESH_DEBUG_DRAW

namespace ck_navmesh_debug_draw_subsystem_spec
{
    auto MakeValidSnapshot(
        uint64 InRevision,
        int32 InTriangleCount = 1,
        uint8 InAreaId = 0) -> FCk_NavmeshDebugDraw_MeshSnapshot
    {
        auto Snapshot = FCk_NavmeshDebugDraw_MeshSnapshot{};
        Snapshot._Revision = InRevision;
        auto& AreaMesh = Snapshot._AreaMeshes.AddDefaulted_GetRef();
        AreaMesh._AreaId = InAreaId;
        AreaMesh._Color = FColor::Green;
        AreaMesh._TriangleVertices = {
                FVector3f{0.0f, 0.0f, 0.0f},
                FVector3f{100.0f, 0.0f, 0.0f},
                FVector3f{0.0f, 100.0f, 0.0f},
        };
        Snapshot._EdgeVertices = {
            FVector3f{0.0f, 0.0f, 10.0f},
            FVector3f{100.0f, 0.0f, 10.0f},
        };
        if (InTriangleCount > 1)
        {
            AreaMesh._TriangleVertices.Append({
                FVector3f{200.0f, 0.0f, 0.0f},
                FVector3f{300.0f, 0.0f, 0.0f},
                FVector3f{200.0f, 100.0f, 0.0f},
            });
            Snapshot._EdgeVertices.Append({
                FVector3f{200.0f, 0.0f, 10.0f},
                FVector3f{300.0f, 0.0f, 10.0f},
            });
        }
        return Snapshot;
    }
} // namespace ck_navmesh_debug_draw_subsystem_spec

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_UnseenBucketRetainsStaleTombstone,
    "Ck.NavmeshDebugDraw.Subsystem.UnseenBucketRetainsStaleTombstone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_UnseenBucketRetainsStaleTombstone::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto BucketKey = uint64{42};
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawStaleTombstoneTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            TestTrue(
                TEXT("Fresh retained bucket is published"),
                Subsystem->Test_PublishRetainedBucket(
                    BucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7),
                    {FIntVector{0, 0, 0}}));
            TestEqual(TEXT("One bucket is retained"), Subsystem->Get_RetainedBucketCount(), 1);
            TestEqual(TEXT("Fresh retained bucket is not stale"), Subsystem->Get_StaleBucketCount(), 0);

            Subsystem->Test_ReconcileCompletedScan(TSet<uint64>{});
            TestEqual(TEXT("Unseen bucket remains retained"), Subsystem->Get_RetainedBucketCount(), 1);
            TestEqual(TEXT("Unseen bucket becomes stale"), Subsystem->Get_StaleBucketCount(), 1);

            TestTrue(
                TEXT("Same bucket accepts fresh replacement"),
                Subsystem->Test_PublishRetainedBucket(
                    BucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8),
                    {FIntVector{0, 0, 0}}));
            TestEqual(TEXT("Fresh replacement does not duplicate retained bucket"),
                      Subsystem->Get_RetainedBucketCount(), 1);
            TestEqual(TEXT("Fresh replacement clears stale state"), Subsystem->Get_StaleBucketCount(), 0);
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_PartialTileCoverageRetainsPreviousSnapshot,
    "Ck.NavmeshDebugDraw.Subsystem.PartialTileCoverageRetainsPreviousSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_PartialTileCoverageRetainsPreviousSnapshot::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto BucketKey = uint64{43};
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawPartialCoverageTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            TestTrue(
                TEXT("Complete two-tile bucket is published"),
                Subsystem->Test_PublishRetainedBucket(
                    BucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7, 2),
                    {FIntVector{0, 0, 0}, FIntVector{1, 0, 0}}));
            TestEqual(TEXT("Complete bucket retains two triangles"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 2);

            TestFalse(
                TEXT("Partial one-tile replacement is rejected"),
                Subsystem->Test_PublishRetainedBucket(
                    BucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1),
                    {FIntVector{0, 0, 0}}));
            TestEqual(TEXT("Partial replacement preserves prior triangle coverage"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 2);
            TestEqual(TEXT("Partial replacement preserves prior revision"),
                      Subsystem->Test_GetRetainedBucketRevision(BucketKey), uint64{7});
            TestEqual(TEXT("Partial replacement marks retained bucket stale"),
                      Subsystem->Get_StaleBucketCount(), 1);

            TestTrue(
                TEXT("Complete tile coverage is accepted after partial rejection"),
                Subsystem->Test_PublishRetainedBucket(
                    BucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(9, 2),
                    {FIntVector{0, 0, 0}, FIntVector{1, 0, 0}}));
            TestEqual(TEXT("Complete replacement advances revision"),
                      Subsystem->Test_GetRetainedBucketRevision(BucketKey), uint64{9});
            TestEqual(TEXT("Complete replacement clears stale state"),
                      Subsystem->Get_StaleBucketCount(), 0);
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_CompletedScanStagesNewBucketsAtomically,
    "Ck.NavmeshDebugDraw.Subsystem.CompletedScanStagesNewBucketsAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_CompletedScanStagesNewBucketsAtomically::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawStageNewBucketsTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            Subsystem->Test_StageCompletedBucket(
                uint64{51},
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7),
                {FIntVector{0, 0, 0}});
            Subsystem->Test_StageCompletedBucket(
                uint64{52},
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7),
                {FIntVector{1, 0, 0}});
            TestEqual(TEXT("Staged buckets are not retained before commit"), Subsystem->Get_RetainedBucketCount(), 0);
            TestEqual(TEXT("Staged buckets are not visible before commit"), Subsystem->Test_GetVisibleBucketCount(), 0);

            TestTrue(TEXT("Completed scan commits staged buckets"), Subsystem->Test_CommitCompletedScan());
            TestEqual(TEXT("Commit retains both staged buckets"), Subsystem->Get_RetainedBucketCount(), 2);
            TestEqual(TEXT("Commit makes both staged buckets visible"), Subsystem->Test_GetVisibleBucketCount(), 2);
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_CompletedScanStagesReplacementAtomically,
    "Ck.NavmeshDebugDraw.Subsystem.CompletedScanStagesReplacementAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_CompletedScanStagesReplacementAtomically::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto BucketKey = uint64{53};
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawStageReplacementTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            TestTrue(TEXT("Two-triangle bucket is directly published"),
                     Subsystem->Test_PublishRetainedBucket(
                         BucketKey,
                         ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7, 2),
                         {FIntVector{0, 0, 0}}));
            TestEqual(TEXT("Direct publish exposes two live triangles"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 2);
            TestEqual(TEXT("Direct publish exposes one visible bucket"), Subsystem->Test_GetVisibleBucketCount(), 1);

            Subsystem->Test_StageCompletedBucket(
                BucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1),
                {FIntVector{0, 0, 0}});
            TestEqual(TEXT("Staged replacement preserves old live triangles before commit"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 2);
            TestEqual(TEXT("Staged replacement preserves old revision before commit"),
                      Subsystem->Test_GetRetainedBucketRevision(BucketKey), uint64{7});
            TestEqual(TEXT("Staged replacement preserves the visible bucket before commit"),
                      Subsystem->Test_GetVisibleBucketCount(), 1);

            TestTrue(TEXT("Completed scan commits the staged replacement"), Subsystem->Test_CommitCompletedScan());
            TestEqual(TEXT("Committed replacement exposes one live triangle"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 1);
            TestEqual(TEXT("Committed replacement advances revision"),
                      Subsystem->Test_GetRetainedBucketRevision(BucketKey), uint64{8});
            TestEqual(TEXT("Committed replacement retains one visible bucket"),
                      Subsystem->Test_GetVisibleBucketCount(), 1);
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_InvalidatedCompletedScanIsDiscardedAtomically,
    "Ck.NavmeshDebugDraw.Subsystem.InvalidatedCompletedScanIsDiscardedAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_InvalidatedCompletedScanIsDiscardedAtomically::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto ExistingBucketKey = uint64{54};
    constexpr auto NewBucketKey = uint64{55};
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawDiscardInvalidatedScanTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            TestTrue(TEXT("Two-triangle bucket is directly published"),
                     Subsystem->Test_PublishRetainedBucket(
                         ExistingBucketKey,
                         ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7, 2),
                         {FIntVector{0, 0, 0}}));
            Subsystem->Test_StageCompletedBucket(
                ExistingBucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1),
                {FIntVector{0, 0, 0}});
            Subsystem->Test_StageCompletedBucket(
                NewBucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8),
                {FIntVector{1, 0, 0}});
            Subsystem->Test_DiscardInvalidatedCompletedScan();

            TestEqual(TEXT("Discard preserves the existing bucket revision"),
                      Subsystem->Test_GetRetainedBucketRevision(ExistingBucketKey), uint64{7});
            TestEqual(TEXT("Discard preserves the existing bucket triangles"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(ExistingBucketKey), 2);
            TestEqual(TEXT("Discard keeps only the existing bucket visible"),
                      Subsystem->Test_GetVisibleBucketCount(), 1);
            TestEqual(TEXT("Discard does not retain the staged new bucket"), Subsystem->Get_RetainedBucketCount(), 1);

            Subsystem->Test_StageCompletedBucket(
                ExistingBucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1),
                {FIntVector{0, 0, 0}});
            Subsystem->Test_StageCompletedBucket(
                NewBucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8),
                {FIntVector{1, 0, 0}});
            TestTrue(TEXT("Stable completed scan commits after discard"), Subsystem->Test_CommitCompletedScan());
            TestEqual(TEXT("Stable commit advances the existing bucket revision"),
                      Subsystem->Test_GetRetainedBucketRevision(ExistingBucketKey), uint64{8});
            TestEqual(TEXT("Stable commit replaces the existing bucket geometry"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(ExistingBucketKey), 1);
            TestEqual(TEXT("Stable commit makes both buckets visible"),
                      Subsystem->Test_GetVisibleBucketCount(), 2);
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_NavigationGenerationSettleCoalescesRefresh,
    "Ck.NavmeshDebugDraw.Subsystem.NavigationGenerationSettleCoalescesRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_NavigationGenerationSettleCoalescesRefresh::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto InitialTimeSeconds = 100.0;
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawNavigationSettleTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            Subsystem->Test_ScheduleNavigationSettleDelay(InitialTimeSeconds);
            const auto InitialSettleDeadline = Subsystem->Test_GetNavigationSettleDeadlineRealTimeSeconds();
            TestTrue(TEXT("Initial navigation generation schedules settling"),
                     Subsystem->Test_Get_IsNavigationSettlePending());
            TestTrue(TEXT("Initial navigation generation owns one settle ticker"),
                     Subsystem->Get_HasActiveNavigationSettleTicker());
            TestFalse(TEXT("Scan start is blocked while navigation settles"), Subsystem->Test_CanStartScan());
            TestFalse(TEXT("Settling does not queue a refresh immediately"),
                      Subsystem->Test_Get_IsRefreshRequested());

            Subsystem->Test_ScheduleNavigationSettleDelay(InitialTimeSeconds + 0.1);
            const auto ResetSettleDeadline = Subsystem->Test_GetNavigationSettleDeadlineRealTimeSeconds();
            TestTrue(TEXT("Repeated generation event keeps settling pending"),
                     Subsystem->Test_Get_IsNavigationSettlePending());
            TestTrue(TEXT("Repeated generation event still owns one settle ticker"),
                     Subsystem->Get_HasActiveNavigationSettleTicker());
            TestTrue(TEXT("Repeated generation event resets the settle deadline"),
                     ResetSettleDeadline > InitialSettleDeadline);

            Subsystem->Test_TickNavigationSettleDelay(ResetSettleDeadline - 0.001);
            TestTrue(TEXT("Settle remains pending before its reset deadline"),
                     Subsystem->Test_Get_IsNavigationSettlePending());
            TestFalse(TEXT("Refresh remains unqueued before the reset deadline"),
                      Subsystem->Test_Get_IsRefreshRequested());

            Subsystem->Test_TickNavigationSettleDelay(ResetSettleDeadline + 0.001);
            TestFalse(TEXT("Settle completes after the reset deadline"),
                      Subsystem->Test_Get_IsNavigationSettlePending());
            TestTrue(TEXT("Settle completion queues one refresh"), Subsystem->Test_Get_IsRefreshRequested());
            TestFalse(TEXT("Settle completion releases the delayed ticker"),
                      Subsystem->Get_HasActiveNavigationSettleTicker());

            Subsystem->Test_ScheduleNavigationSettleDelay(ResetSettleDeadline + 1.0);
            TestTrue(TEXT("A later generation event creates another delayed ticker"),
                     Subsystem->Get_HasActiveNavigationSettleTicker());
            Subsystem->Set_IsEnabled(false);
            TestFalse(TEXT("Disable cancels pending navigation settlement"),
                      Subsystem->Get_HasActiveNavigationSettleTicker());
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_AreaPreviewIsNonDestructiveAndBounded,
    "Ck.NavmeshDebugDraw.Subsystem.AreaPreviewIsNonDestructiveAndBounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_AreaPreviewIsNonDestructiveAndBounded::RunTest(
    const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    constexpr auto BucketKey = uint64{56};
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawAreaPreviewTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            TestTrue(TEXT("Authoritative base bucket is published"),
                     Subsystem->Test_PublishRetainedBucket(
                         BucketKey,
                         ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7),
                         {FIntVector{0, 0, 0}}));
            const auto ProcessedTilesBeforePreview = Subsystem->Get_ProcessedTileCount();
            const auto ProxyReplacementsBeforePreview = Subsystem->Get_ProxyReplacementCount();

            Subsystem->Test_StageCompletedBucket(
                BucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1, uint8{5}),
                {FIntVector{0, 0, 0}});
            Subsystem->Test_PublishStagedAreaPreview();
            TestEqual(TEXT("Area preview adds one overlay triangle"),
                      Subsystem->Test_GetRetainedBucketAreaPreviewTriangleCount(BucketKey), 1);
            TestEqual(TEXT("Area preview preserves authoritative triangle count"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 1);
            TestEqual(TEXT("Area preview preserves authoritative revision"),
                      Subsystem->Test_GetRetainedBucketRevision(BucketKey), uint64{7});
            TestEqual(TEXT("Direct preview publication performs no Recast tile scan"),
                      Subsystem->Get_ProcessedTileCount(), ProcessedTilesBeforePreview);
            TestTrue(TEXT("Changed preview recreates exactly one retained proxy"),
                     Subsystem->Get_ProxyReplacementCount() == ProxyReplacementsBeforePreview + 1);

            Subsystem->Test_StageCompletedBucket(
                BucketKey,
                ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(
                    9,
                    1,
                    static_cast<uint8>(RECAST_DEFAULT_AREA)),
                {FIntVector{0, 0, 0}});
            Subsystem->Test_PublishStagedAreaPreview();
            TestEqual(TEXT("A valid default-only preview clears the prior colored overlay"),
                      Subsystem->Test_GetRetainedBucketAreaPreviewTriangleCount(BucketKey), 0);
            TestEqual(TEXT("Default-only preview still preserves authoritative geometry"),
                      Subsystem->Test_GetRetainedBucketTriangleCount(BucketKey), 1);

            constexpr auto PreviewBucketCountBeyondLimit = 9;
            for (auto PreviewBucketIndex = 0;
                 PreviewBucketIndex < PreviewBucketCountBeyondLimit;
                 ++PreviewBucketIndex)
            {
                const auto PreviewBucketKey = uint64{100} + static_cast<uint64>(PreviewBucketIndex);
                TestTrue(TEXT("Large preview fixture publishes an authoritative bucket"),
                         Subsystem->Test_PublishRetainedBucket(
                             PreviewBucketKey,
                             ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(7),
                             {FIntVector{PreviewBucketIndex, 0, 0}}));
                Subsystem->Test_StageCompletedBucket(
                    PreviewBucketKey,
                    ck_navmesh_debug_draw_subsystem_spec::MakeValidSnapshot(8, 1, uint8{5}),
                    {FIntVector{PreviewBucketIndex, 0, 0}});
            }
            Subsystem->Test_PublishStagedAreaPreview();
            TestEqual(TEXT("One preview publication is capped to eight retained buckets"),
                      Subsystem->Get_AreaPreviewTriangleCount(), 8);

            Subsystem->Test_ScheduleAreaPreviewDelay();
            TestTrue(TEXT("Preview refresh owns one delayed ticker"),
                     Subsystem->Get_HasActiveAreaPreviewTicker());
            Subsystem->Test_ScheduleAreaPreviewDelay();
            TestTrue(TEXT("Repeated preview request remains represented by one delayed ticker"),
                     Subsystem->Get_HasActiveAreaPreviewTicker());
            Subsystem->Set_IsEnabled(false);
            TestFalse(TEXT("Disable cancels the delayed preview ticker"),
                      Subsystem->Get_HasActiveAreaPreviewTicker());
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_Subsystem_RetriesUntilSourceExists,
    "Ck.NavmeshDebugDraw.Subsystem.RetriesUntilSourceExists",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_Subsystem_RetriesUntilSourceExists::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    constexpr auto InformEngineOfWorld = false;
    auto World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(
        EWorldType::Game,
        InformEngineOfWorld,
        FName{TEXT("CkNavmeshDebugDrawRetryTest")})};
    TestNotNull(TEXT("Transient game world is created"), World.Get());
    if (World.IsValid())
    {
        auto* Subsystem = World->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
        TestNotNull(TEXT("Navmesh debug-draw subsystem is created"), Subsystem);
        if (Subsystem != nullptr)
        {
            AddExpectedError(
                TEXT("No default Recast navmesh is available"),
                EAutomationExpectedErrorFlags::Contains,
                1);
            Subsystem->Set_IsEnabled(true);
            TestTrue(TEXT("Missing source leaves the requested feature enabled"), Subsystem->Get_IsEnabled());
            TestTrue(TEXT("Missing source keeps a bounded retry ticker"), Subsystem->Get_HasActiveWorkTicker());
            TestEqual(TEXT("Missing source retains no render buckets"), Subsystem->Get_RetainedBucketCount(), 0);

            Subsystem->Set_IsEnabled(false);
            TestFalse(TEXT("Disable removes the retry ticker"), Subsystem->Get_HasActiveWorkTicker());
            TestFalse(TEXT("Disable clears enabled state"), Subsystem->Get_IsEnabled());
        }

        World->DestroyWorld(InformEngineOfWorld);
        World.Reset();
    }
    return true;
}

#endif
