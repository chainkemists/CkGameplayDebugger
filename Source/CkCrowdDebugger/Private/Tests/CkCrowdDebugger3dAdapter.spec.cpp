#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// The adapter takes copied Crowd DTOs and submits only opaque uint64 identities and Foundation scene instances to
// FCk_DebugScene_Target. No public CkDebugScene type may expose FCk_Handle or Crowd.
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"
#include "CkDebugScene/CkDebugScene_Materials.h"
#include "CkDebugScene/CkDebugScene_Target.h"

#include "Engine/World.h"

#include <limits>
namespace ck_crowd_debugger_3d_adapter_spec
{
constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
constexpr uint64 AgentA = 101;
constexpr uint64 AgentB = 202;
constexpr uint64 ReplacementAgent = 303;

struct FScopedTarget
{
    FScopedTarget()
    {
        constexpr auto InformEngineOfWorld = false;
        _World = UWorld::CreateWorld(EWorldType::Game, InformEngineOfWorld,
                                    FName{TEXT("CkCrowdDebugger3dAdapter")});
        _Target = MakeShared<FCk_DebugScene_Target>(
            FCk_DebugScene_TargetConfig{}.Set_World(_World).Set_MaxItems(128).Set_MaxInstances(20000));
    }

    ~FScopedTarget()
    {
        _Target.Reset();
        if (IsValid(_World))
        {
            constexpr auto InformEngineOfWorld = false;
            _World->DestroyWorld(InformEngineOfWorld);
            _World = nullptr;
        }
    }

    UWorld* _World = nullptr;
    TSharedPtr<FCk_DebugScene_Target> _Target;
};

auto
MakeAgent(uint64 InIdentity, const FVector& InPosition) -> FCkCrowdDebugger_3dAgentSnapshot
{
    auto Agent = FCkCrowdDebugger_3dAgentSnapshot{};
    Agent._Identity = InIdentity;
    Agent._Position = InPosition;
    Agent._Radius = 42.0f;
    Agent._Height = 192.0f;
    Agent._StatusColor = FLinearColor::Green;
    return Agent;
}

auto
MakeSnapshot() -> FCkCrowdDebugger_3dSceneSnapshot
{
    auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
    Snapshot._WorldEpoch = 17;
    Snapshot._Agents = {MakeAgent(AgentA, FVector::ZeroVector), MakeAgent(AgentB, FVector{400.0f, 0.0f, 0.0f})};
    return Snapshot;
}

auto
MakeAvoidanceVolume(uint64 InIdentity, const FVector& InLocation = FVector::ZeroVector)
    -> FCkCrowdDebugger_3dAvoidanceVolumeSnapshot
{
    auto Volume = FCkCrowdDebugger_3dAvoidanceVolumeSnapshot{};
    Volume._Identity = InIdentity;
    Volume._YawWorldTransform = FTransform{FRotator{0.0f, 35.0f, 0.0f}, InLocation};
    Volume._PhysicalWorldHalfExtents = FVector{100.0f, 60.0f, 80.0f};
    Volume._InfluenceWorldHalfExtents = FVector{160.0f, 120.0f, 80.0f};
    Volume._PaintedWorldHalfExtents = FVector{220.0f, 180.0f, 80.0f};
    Volume._State = ECkCrowdDebugger_AvoidanceVolumeState::Confirmed;
	Volume._TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::AvoidIfPossible;
    Volume._HasValidGeometry = true;
    return Volume;
}
} // namespace ck_crowd_debugger_3d_adapter_spec

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_CopiedInputsOutliveProducer,
                                 "Ck.CrowdDebugger.Viewport3d.CopiedInputsOutliveProducer",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_CopiedInputsOutliveProducer::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto ProducerSnapshot = MakeSnapshot();
    const auto CopiedSnapshot = ProducerSnapshot;
    ProducerSnapshot = {};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("a copied snapshot reconciles after the producer-side source is cleared"),
             Adapter.Reconcile(CopiedSnapshot, *Fixture._Target));
    TestEqual(TEXT("the target receives the copied agent population"), Fixture._Target->Get_Stats().Get_ItemCount(), 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_StableIdentitySurvivesReorder,
                                 "Ck.CrowdDebugger.Viewport3d.StableIdentitySurvivesReorder",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_StableIdentitySurvivesReorder::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto First = MakeSnapshot();
    auto Reordered = First;
    Swap(Reordered._Agents[0], Reordered._Agents[1]);

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(First, *Fixture._Target);
    Fixture._Target->Reset_FrameStats();
    Adapter.Reconcile(Reordered, *Fixture._Target);

    const auto& Stats = Fixture._Target->Get_Stats();
    TestEqual(TEXT("reorder adds no target instances"), Stats.Get_InstancesAdded(), 0);
    TestEqual(TEXT("reorder removes no target instances"), Stats.Get_InstancesRemoved(), 0);
    const auto CurrentIndex = Adapter.Get_CurrentAgentIndex(AgentA);
    TestTrue(TEXT("stable identity still has a current source row"), CurrentIndex.IsSet());
    if (CurrentIndex.IsSet())
    {
        TestEqual(TEXT("identity maps to its new current index, not its old array slot"), *CurrentIndex, 1);
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_AgentCapsuleAndPickMap,
                                 "Ck.CrowdDebugger.Viewport3d.AgentCapsuleAndPickMap",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_AgentCapsuleAndPickMap::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(MakeSnapshot(), *Fixture._Target);

    TestEqual(TEXT("each agent submits one capsule item"),
              Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AgentCapsule), 2);
    const TArray<FCk_DebugScene_Instance>& AgentInstances = Adapter.Get_SubmittedInstances(AgentA);
    TestEqual(TEXT("an agent capsule submits one target instance"), AgentInstances.Num(), 1);
    const TOptional<FCk_DebugScene_Pick> Pick =
        Fixture._Target->TryPick(FVector{-500.0f, 0.0f, 96.0f}, FVector::ForwardVector);
    TestTrue(TEXT("the target returns an opaque generic pick"), Pick.IsSet());
    if (Pick.IsSet())
    {
        const auto Resolution = Adapter.Resolve_Pick(*Pick);
        TestTrue(TEXT("the adapter resolves its generic target pick"), Resolution.IsSet());
        if (Resolution.IsSet())
        {
            TestEqual(TEXT("the pick resolves stable Crowd identity"), Resolution->_Identity, AgentA);
            TestEqual(TEXT("the pick resolves current Crowd row"), Resolution->_CurrentAgentIndex, 0);
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_SelectionBoundsAndPath,
                                 "Ck.CrowdDebugger.Viewport3d.SelectionBoundsAndPath",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_SelectionBoundsAndPath::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._SelectedIdentity = AgentB;
    Snapshot._Agents[1]._PlannedPath = {FVector{500.0f, 0.0f, 0.0f}, FVector{600.0f, 100.0f, 0.0f}};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    const auto Bounds = Adapter.Get_SelectionBounds(*Fixture._Target);
    TestTrue(TEXT("selection bounds frame the selected capsule"),
             Bounds.IsSet() && Bounds->IsInsideOrOn(FVector{400.0f, 0.0f, 96.0f}));
    TestEqual(TEXT("only selected agent contributes planned path"),
              Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::SelectedPath), 1);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_AllBoundsUnionAndStaticLayerChurn,
                                 "Ck.CrowdDebugger.Viewport3d.AllBoundsUnionAndStaticLayerChurn",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_AllBoundsUnionAndStaticLayerChurn::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._Voxel._NavigationBounds = FBox{FVector{-1000.0f, -1000.0f, -100.0f}, FVector{-800.0f, -800.0f, 100.0f}};
    Snapshot._Recast._Revision = 41;
    Snapshot._Recast._Triangles = {FVector{800.0f, 800.0f, 0.0f}, FVector{1000.0f, 800.0f, 0.0f},
                                   FVector{800.0f, 1000.0f, 0.0f}};
    Snapshot._PathNetwork._Revision = 73;
    Snapshot._PathNetwork._Ribbons = {FCkCrowdDebugger_3dRibbonSnapshot{
        {FVector{0.0f, -1000.0f, 0.0f}, FVector{0.0f, -800.0f, 0.0f}}, {50.0f, 50.0f}}};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    const auto Bounds = Fixture._Target->Get_ContentBounds();
    TestTrue(TEXT("content bounds include VoxelNav"), Bounds.IsInsideOrOn(FVector{-900.0f, -900.0f, 0.0f}));
    TestTrue(TEXT("content bounds include Recast"), Bounds.IsInsideOrOn(FVector{900.0f, 900.0f, 0.0f}));
    TestTrue(TEXT("content bounds include agents"), Bounds.IsInsideOrOn(FVector{400.0f, 0.0f, 96.0f}));
    TestTrue(TEXT("content bounds include width-aware ribbons"), Bounds.IsInsideOrOn(FVector{50.0f, -900.0f, 0.0f}));

    Fixture._Target->Reset_FrameStats();
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    const auto& Stats = Fixture._Target->Get_Stats();
    TestEqual(TEXT("unchanged nav/ribbon revisions add no topology"), Stats.Get_InstancesAdded(), 0);
    TestEqual(TEXT("unchanged nav/ribbon revisions update no topology"), Stats.Get_InstancesUpdated(), 0);
    TestEqual(TEXT("unchanged nav/ribbon revisions remove no topology"), Stats.Get_InstancesRemoved(), 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_RibbonGeometryPreservesOrderAndWidth,
                                 "Ck.CrowdDebugger.Viewport3d.RibbonGeometryPreservesOrderAndWidth",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_RibbonGeometryPreservesOrderAndWidth::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
    Snapshot._WorldEpoch = 17;
    Snapshot._PathNetwork._Revision = 1;
    Snapshot._PathNetwork._Ribbons = {FCkCrowdDebugger_3dRibbonSnapshot{
        {FVector{0.0f, 0.0f, 0.0f}, FVector{100.0f, 0.0f, 0.0f}, FVector{100.0f, 100.0f, 0.0f}},
        {10.0f, 40.0f, 20.0f}}};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    const auto RibbonItem = Adapter.Get_TargetItemId(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, 0);
    TestTrue(TEXT("a source ribbon has an opaque target item identity"), RibbonItem.IsSet());
    if (RibbonItem.IsSet())
    {
        TestEqual(TEXT("one retained ribbon mesh occupies one scene instance"),
                  Fixture._Target->Get_Stats().Get_InstanceCount(), 1);
        TestEqual(TEXT("two ordered spans build four fill triangles"), Adapter.Get_RibbonTriangleCount(0), 4);
        TestEqual(TEXT("the retained outline preserves all three authored points"),
                  Adapter.Get_RibbonOutlinePointCount(0), 3);
        const auto Bounds = Fixture._Target->Get_ItemBounds(*RibbonItem);
        TestTrue(TEXT("widest authored point expands bounds"),
                 Bounds.IsSet() && Bounds->IsInsideOrOn(FVector{100.0f, 40.0f, 0.0f}));
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_RetainedSurfacesSanitizeAndRenderTwoSided,
                                 "Ck.CrowdDebugger.Viewport3d.RetainedSurfacesSanitizeAndRenderTwoSided",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_RetainedSurfacesSanitizeAndRenderTwoSided::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._Recast._Revision = 1;
    Snapshot._Recast._Triangles = {FVector{0.0f, 0.0f, 0.0f}, FVector{100.0f, 0.0f, 0.0f},
                                   FVector{0.0f, 100.0f, 0.0f}, FVector{200.0f, 0.0f, 0.0f},
                                   FVector{200.0f, 0.0f, 0.0f}, FVector{200.0f, 0.0f, 0.0f}};
    Snapshot._PathNetwork._Revision = 1;
    Snapshot._PathNetwork._Ribbons = {
        FCkCrowdDebugger_3dRibbonSnapshot{
            {FVector::ZeroVector, FVector{50.0f, 0.0f, 0.0f}},
            {std::numeric_limits<float>::quiet_NaN(), 10.0f}},
        FCkCrowdDebugger_3dRibbonSnapshot{
            {FVector{0.0f, 200.0f, 0.0f}, FVector{0.0f, 200.0f, 0.0f}, FVector{100.0f, 200.0f, 0.0f}},
            {10.0f, 10.0f, 10.0f}}};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("mixed valid and degenerate retained surfaces reconcile"), Adapter.Reconcile(Snapshot, *Fixture._Target));
    TestTrue(TEXT("a valid Recast triangle remains published"),
             Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::Recast));
    TestEqual(TEXT("Recast keeps only valid source triangles"), Adapter.Get_RecastTriangleCount(), 1);
    TestEqual(TEXT("Recast publishes reverse winding for two-sided rendering"),
              Adapter.Get_RecastRenderedTriangleCount(), 2);
    TestTrue(TEXT("a valid ribbon span remains published after a degenerate span"),
             Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon));
    TestEqual(TEXT("an invalid earlier ribbon retains its source metric slot"), Adapter.Get_RibbonTriangleCount(0), 0);
    TestEqual(TEXT("ribbon keeps only its valid source triangles"), Adapter.Get_RibbonTriangleCount(1), 2);
    TestEqual(TEXT("ribbon publishes reverse winding for two-sided rendering"),
              Adapter.Get_RibbonRenderedTriangleCount(1), 4);
    TestEqual(TEXT("ribbon outline retains its authored points"), Adapter.Get_RibbonOutlinePointCount(1), 3);
    const auto RecastItem = Adapter.Get_TargetItemId(ECkCrowdDebugger_3dSceneRole::Recast, 0);
    const auto RibbonItem = Adapter.Get_TargetItemId(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon, 0);
    TestTrue(TEXT("Recast has a retained target item"), RecastItem.IsSet());
    TestTrue(TEXT("PathNetwork has a retained target item"), RibbonItem.IsSet());
    if (RecastItem.IsSet())
    {
        const auto Instances = Fixture._Target->Get_ItemInstances(*RecastItem);
        TestEqual(TEXT("Recast submits one retained instance"), Instances.Num(), 1);
        if (Instances.Num() == 1)
        {
            const auto& Appearance = Instances[0].Get_Appearance();
            TestEqual(TEXT("Recast uses the shared translucent debug material"), Appearance.Get_BaseMaterial(),
                      ck::debug_scene::materials::TryGet_Translucent());
            TestEqual(TEXT("Recast remains world depth priority"), Appearance.Get_DepthPriority(),
                      ECk_DebugScene_DepthPriority::World);
            TestEqual(TEXT("Recast keeps default translucent sort priority"), Appearance.Get_TranslucencySortPriority(), 0);
        }
    }
    if (RibbonItem.IsSet())
    {
        const auto Instances = Fixture._Target->Get_ItemInstances(*RibbonItem);
        TestEqual(TEXT("PathNetwork submits one retained instance"), Instances.Num(), 1);
        if (Instances.Num() == 1)
        {
            const auto& Appearance = Instances[0].Get_Appearance();
            TestEqual(TEXT("PathNetwork uses the shared translucent debug material"), Appearance.Get_BaseMaterial(),
                      ck::debug_scene::materials::TryGet_Translucent());
            TestEqual(TEXT("PathNetwork preserves legacy foreground depth priority"), Appearance.Get_DepthPriority(),
                      ECk_DebugScene_DepthPriority::Foreground);
            TestTrue(TEXT("PathNetwork has a stable positive translucent sort priority"),
                     Appearance.Get_TranslucencySortPriority() > 0);
        }
    }
    TestEqual(TEXT("surface depth/sort separation uses dedicated retained buckets"),
              Fixture._Target->Get_Stats().Get_BucketCount(), 3);
    TestEqual(TEXT("surface overlap retains two agents and two static surfaces"),
              Fixture._Target->Get_Stats().Get_ItemCount(), 4);

    Fixture._Target->Reset_FrameStats();
    TestTrue(TEXT("unchanged retained surfaces reconcile without topology churn"),
             Adapter.Reconcile(Snapshot, *Fixture._Target));
    const auto& SteadyStats = Fixture._Target->Get_Stats();
    TestEqual(TEXT("unchanged retained surfaces add no instances"), SteadyStats.Get_InstancesAdded(), 0);
    TestEqual(TEXT("unchanged retained surfaces update no instances"), SteadyStats.Get_InstancesUpdated(), 0);
    TestEqual(TEXT("unchanged retained surfaces remove no instances"), SteadyStats.Get_InstancesRemoved(), 0);
    TestEqual(TEXT("unchanged retained surfaces preserve dedicated buckets"), SteadyStats.Get_BucketCount(), 3);
    TestEqual(TEXT("unchanged retained surfaces preserve overlap item count"), SteadyStats.Get_ItemCount(), 4);

    auto DegenerateOnly = Snapshot;
    DegenerateOnly._Recast._Revision = 2;
    DegenerateOnly._Recast._Triangles = {FVector{0.0f, 0.0f, 0.0f}, FVector{0.0f, 0.0f, 0.0f},
                                         FVector{0.0f, 0.0f, 0.0f}};
    DegenerateOnly._PathNetwork._Revision = 2;
    DegenerateOnly._PathNetwork._Ribbons = {FCkCrowdDebugger_3dRibbonSnapshot{
        {FVector{0.0f, 200.0f, 0.0f}, FVector{0.0f, 200.0f, 0.0f}}, {10.0f, 10.0f}}};
    TestTrue(TEXT("degenerate-only retained sources reconcile as an empty rendered result"),
             Adapter.Reconcile(DegenerateOnly, *Fixture._Target));
    TestFalse(TEXT("degenerate-only Recast does not retain a stale mesh"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::Recast));
    TestFalse(TEXT("degenerate-only ribbon does not retain a stale mesh"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon));
    Fixture._Target->Reset_FrameStats();
    TestTrue(TEXT("a latched degenerate revision does not republish a missing mesh"),
             Adapter.Reconcile(DegenerateOnly, *Fixture._Target));
    TestFalse(TEXT("latched degenerate Recast stays absent"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::Recast));
    TestFalse(TEXT("latched degenerate ribbon stays absent"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon));
    TestEqual(TEXT("latched degenerate revision adds no retained mesh"),
              Fixture._Target->Get_Stats().Get_InstancesAdded(), 0);

    TestTrue(TEXT("valid retained surfaces can be republished after a degenerate revision"),
             Adapter.Reconcile(Snapshot, *Fixture._Target));
    TestTrue(TEXT("Recast is live before the empty-source removal"),
             Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::Recast));
    TestTrue(TEXT("ribbon is live before the empty-source removal"),
             Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon));

    auto Empty = Snapshot;
    Empty._Recast._Revision = 3;
    Empty._Recast._Triangles.Reset();
    Empty._PathNetwork._Revision = 3;
    Empty._PathNetwork._Ribbons.Reset();
    TestTrue(TEXT("empty retained sources remove their prior published surfaces"), Adapter.Reconcile(Empty, *Fixture._Target));
    TestFalse(TEXT("empty Recast source removes retained surface"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::Recast));
    TestFalse(TEXT("empty ribbon source removes retained surface"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_VoxelContentAndCapsPreserved,
                                 "Ck.CrowdDebugger.Viewport3d.VoxelContentAndCapsPreserved",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_VoxelContentAndCapsPreserved::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
    Snapshot._WorldEpoch = 17;
    Snapshot._Voxel._AuthoredBounds = FBox{FVector{-100.0f}, FVector{100.0f}};
    Snapshot._Voxel._Cells._Occupied = {FBox{FVector{-30.0f}, FVector{-10.0f}}};
    Snapshot._Voxel._Cells._MergedFree = {FBox{FVector{-10.0f}, FVector{10.0f}}};
    Snapshot._Voxel._Cells._RawFree = {FBox{FVector{10.0f}, FVector{30.0f}}};
    Snapshot._Voxel._Chunks = {FBox{FVector{-50.0f}, FVector::ZeroVector}};
    Snapshot._Voxel._Portals = {{FVector{-10.0f, 0.0f, 0.0f}, FVector{10.0f, 0.0f, 0.0f}}};
    Snapshot._Voxel._RepairLinks = {{FVector{0.0f, -10.0f, 0.0f}, FVector{0.0f, 10.0f, 0.0f}}};
    Snapshot._Voxel._RawFreeCellCap = 10000;

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    TestTrue(TEXT("VoxelNav bounds enter content framing"),
             Fixture._Target->Get_ContentBounds().IsInsideOrOn(FVector{100.0f, 100.0f, 100.0f}));
    TestTrue(TEXT("merged free remains independent"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::VoxelMergedFree));
    TestTrue(TEXT("chunk bounds remain present"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::VoxelChunk));
    TestTrue(TEXT("portals remain present"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::VoxelPortal));
    TestTrue(TEXT("repair links remain present"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::VoxelRepair));
    TestTrue(TEXT("raw free obeys Crowd cap"),
             Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::VoxelRawFree) <= 10000);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_ResetAndStaleIdentityFailClosed,
                                 "Ck.CrowdDebugger.Viewport3d.ResetAndStaleIdentityFailClosed",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_ResetAndStaleIdentityFailClosed::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(MakeSnapshot(), *Fixture._Target);

    Adapter.Reset_ForWorldChange(*Fixture._Target);
    TestEqual(TEXT("world reset removes target scene"), Fixture._Target->Get_Stats().Get_ItemCount(), 0);
    TestFalse(TEXT("world reset clears mappings"), Adapter.Get_CurrentAgentIndex(AgentA).IsSet());
    TestFalse(TEXT("world reset clears selected identity"), Adapter.Get_SelectedIdentity().IsSet());

    auto Replacement = MakeSnapshot();
    Replacement._WorldEpoch = 18;
    Replacement._Agents[0] = MakeAgent(ReplacementAgent, FVector::ZeroVector);
    Adapter.Reconcile(Replacement, *Fixture._Target);
    TestFalse(TEXT("stale identity cannot select same-position replacement"),
              Adapter.TrySelect_Identity(AgentA, *Fixture._Target));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_SourceAppearanceStaysOwned,
                                 "Ck.CrowdDebugger.Viewport3d.SourceAppearanceStaysOwned",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_SourceAppearanceStaysOwned::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._Agents[0]._StatusColor = FLinearColor::Red;
    Snapshot._PathNetwork._Opacity = 0.35f;
    Snapshot._Voxel._Cells._RawFree = {FBox{FVector{-10.0f}, FVector{10.0f}}};
    constexpr auto IsLayerVisible = false;
    Snapshot._Voxel._LayerVisibility.Add(ECkCrowdDebugger_3dVoxelLayer::RawFree, IsLayerVisible);

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(Snapshot, *Fixture._Target);
    const FCk_DebugScene_Appearance AgentAppearance = Adapter.Get_Appearance(AgentA);
    TestEqual(TEXT("status colour stays adapter-owned"), AgentAppearance.Get_Color(), FLinearColor::Red);
    TestEqual(TEXT("ribbon opacity stays adapter-owned"),
              Adapter.Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole::PathNetworkRibbon).Get_Opacity(), 0.35f);
    TestFalse(TEXT("source visibility stays adapter-owned"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::VoxelRawFree));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_LaterBadAgentAbortsWithoutPublishing,
                                 "Ck.CrowdDebugger.Viewport3d.LaterBadAgentAbortsWithoutPublishing",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_LaterBadAgentAbortsWithoutPublishing::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    const auto First = MakeSnapshot();
    TestTrue(TEXT("baseline scene commits"), Adapter.Reconcile(First, *Fixture._Target));
    TestTrue(TEXT("baseline retained line commits"),
             Fixture._Target->Set_LineChannel(
                 FName{TEXT("Baseline")},
                 {FCk_DebugScene_Line{FVector::ZeroVector, FVector{100.0f, 0.0f, 0.0f}, FLinearColor::White, 1.0f}}));
    const auto AgentItem = Adapter.Get_TargetItemId(ECkCrowdDebugger_3dSceneRole::AgentCapsule, 0);
    TestTrue(TEXT("baseline agent item exists"), AgentItem.IsSet());
    if (NOT AgentItem.IsSet())
    {
        return false;
    }
    const auto BeforeIds = Fixture._Target->Get_InstanceIds(*AgentItem);
    const auto BeforeBounds = Fixture._Target->Get_ContentBounds();
    const auto BeforeStats = Fixture._Target->Get_Stats();
    const auto BeforeLines = Fixture._Target->Get_RenderedLineCount();

    auto Invalid = First;
    Invalid._Agents.Add(FCkCrowdDebugger_3dAgentSnapshot{});
    AddExpectedError(TEXT("Crowd debug-scene adapter rejected"), EAutomationExpectedErrorFlags::Contains, 2);
    TestFalse(TEXT("later invalid agent aborts the target transaction"), Adapter.Reconcile(Invalid, *Fixture._Target));
    TestEqual(TEXT("prior target identity remains"), Fixture._Target->Get_InstanceIds(*AgentItem)[0], BeforeIds[0]);
    TestTrue(TEXT("prior bounds remain"), Fixture._Target->Get_ContentBounds().Equals(BeforeBounds));
    TestEqual(TEXT("prior rendered lines remain"), Fixture._Target->Get_RenderedLineCount(), BeforeLines);
    TestEqual(TEXT("prior item count remains"), Fixture._Target->Get_Stats().Get_ItemCount(),
              BeforeStats.Get_ItemCount());
    TestTrue(TEXT("prior agent mapping remains"), Adapter.Get_CurrentAgentIndex(AgentA).IsSet());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_Scale240UsesStableInstancing,
                                 "Ck.CrowdDebugger.Viewport3d.Scale240UsesStableInstancing",
                                 ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto
    FCkCrowdDebugger3dAdapter_Scale240UsesStableInstancing::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    constexpr auto InformEngineOfWorld = false;
    auto* World = UWorld::CreateWorld(EWorldType::Game, InformEngineOfWorld,
                                     FName{TEXT("CkCrowdDebugger3dScale")});
    if (NOT TestNotNull(TEXT("scale world exists"), World))
    {
        return false;
    }

    {
        auto Target = MakeShared<FCk_DebugScene_Target>(
            FCk_DebugScene_TargetConfig{}.Set_World(World).Set_MaxItems(512).Set_MaxInstances(1024));
        auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
        Snapshot._WorldEpoch = 71;
        constexpr auto AgentCount = 240;
        Snapshot._Agents.Reserve(AgentCount);
        for (auto Index = 0; Index < AgentCount; ++Index)
        {
            Snapshot._Agents.Add(
                MakeAgent(static_cast<uint64>(Index + 1), FVector{static_cast<double>(Index % 24) * 150.0,
                                                                  static_cast<double>(Index / 24) * 150.0, 0.0}));
        }

        auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
        const auto FirstStart = FPlatformTime::Seconds();
        TestTrue(TEXT("240-agent first reconcile succeeds"), Adapter.Reconcile(Snapshot, *Target));
        const auto FirstMilliseconds = (FPlatformTime::Seconds() - FirstStart) * 1000.0;
        TestEqual(TEXT("240 agents share one capsule component"), Target->Get_Stats().Get_ComponentCount(), 1);
        TestEqual(TEXT("240 agents share one capsule bucket"), Target->Get_Stats().Get_BucketCount(), 1);
        TestEqual(TEXT("all 240 instances are retained"), Target->Get_Stats().Get_InstanceCount(), AgentCount);

        const auto SteadyStart = FPlatformTime::Seconds();
        TestTrue(TEXT("240-agent steady reconcile succeeds"), Adapter.Reconcile(Snapshot, *Target));
        const auto SteadyMilliseconds = (FPlatformTime::Seconds() - SteadyStart) * 1000.0;
        TestEqual(TEXT("steady reconcile adds nothing"), Target->Get_Stats().Get_InstancesAdded(), 0);
        TestEqual(TEXT("steady reconcile updates nothing"), Target->Get_Stats().Get_InstancesUpdated(), 0);
        TestEqual(TEXT("steady reconcile removes nothing"), Target->Get_Stats().Get_InstancesRemoved(), 0);
        TestEqual(TEXT("steady reconcile reuses every slot"), Target->Get_Stats().Get_InstancesUnchanged(), AgentCount);
        UE_LOG(
            LogTemp, Display,
            TEXT("[CrowdDebugSceneBench] agents=%d first_ms=%.3f steady_ms=%.3f components=%d buckets=%d instances=%d"),
            AgentCount, FirstMilliseconds, SteadyMilliseconds, Target->Get_Stats().Get_ComponentCount(),
            Target->Get_Stats().Get_BucketCount(), Target->Get_Stats().Get_InstanceCount());
    }
    World->DestroyWorld(InformEngineOfWorld);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_QueueReservationsAreRetained,
    "Ck.CrowdDebugger.Viewport3d.QueueReservationsAreRetained", ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto FCkCrowdDebugger3dAdapter_QueueReservationsAreRetained::RunTest(const FString&) -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    auto Queue = FCkCrowdDebugger_3dQueueSnapshot{};
    Queue._Identity = 9001;
    Queue._Revision = 1;
    Queue._OwnerTarget = {FVector{100.0f, 0.0f, 0.0f}, FVector{200.0f, 0.0f, 0.0f}};
    const auto SlotAtRank0 = HashCombineFast(GetTypeHash(Queue._Identity), GetTypeHash(0));
    const auto SlotAtRank1 = HashCombineFast(GetTypeHash(Queue._Identity), GetTypeHash(1));
    const auto SlotAtRank2 = HashCombineFast(GetTypeHash(Queue._Identity), GetTypeHash(2));
    Queue._Members.Add({AgentA, SlotAtRank0, 0, FVector{0.0f, 100.0f, 0.0f}, FVector::ForwardVector, true});
    Queue._Members.Add({AgentB, SlotAtRank1, 1, FVector{0.0f, 220.0f, 0.0f}, FVector::ForwardVector, true});
    Queue._Members.Add({0, SlotAtRank2, 2, FVector{120.0f, 100.0f, 0.0f}, FVector::RightVector, true});
    Snapshot._Queues.Add(Queue);
    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("queue reservations reconcile"), Adapter.Reconcile(Snapshot, *Fixture._Target));
    TestTrue(TEXT("queue owner target has a dedicated scene role"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::QueueOwnerTarget));
    TestNotEqual(TEXT("queue identity plus rank produces distinct slot identities"), SlotAtRank0, SlotAtRank1);
    TestEqual(TEXT("ranked reservations retain distinct items"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::QueueReservation), 3);
    Snapshot._Queues.Reset();
    TestTrue(TEXT("empty queue snapshot removes retained queue geometry"), Adapter.Reconcile(Snapshot, *Fixture._Target));
    TestFalse(TEXT("empty queue snapshot removes owner target"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::QueueOwnerTarget));
    TestFalse(TEXT("empty queue snapshot removes reservations"), Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::QueueReservation));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_AvoidanceVolumesRetainDistinctRoles,
    "Ck.CrowdDebugger.Viewport3d.AvoidanceVolumesRetainDistinctRoles",
    ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto FCkCrowdDebugger3dAdapter_AvoidanceVolumesRetainDistinctRoles::RunTest(const FString&) -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Producer = MakeSnapshot();
    Producer._AvoidanceVolumes = {MakeAvoidanceVolume(7001)};
    const auto CopiedSnapshot = Producer;
    Producer = {};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("a copied avoidance-volume snapshot outlives its producer"),
        Adapter.Reconcile(CopiedSnapshot, *Fixture._Target));
    TestEqual(TEXT("physical footprint has one retained item"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical), 1);
    TestEqual(TEXT("influence footprint has one retained item"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumeInfluence), 1);
    TestEqual(TEXT("painted footprint has one retained item"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted), 1);

    const auto Physical = Adapter.Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical);
    const auto Influence = Adapter.Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumeInfluence);
    const auto Painted = Adapter.Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted);
    TestNotEqual(TEXT("physical and influence roles remain visually distinct"), Physical.Get_Color(), Influence.Get_Color());
    TestNotEqual(TEXT("influence and painted roles remain visually distinct"), Influence.Get_Color(), Painted.Get_Color());

	// Physical geometry carries traversal policy while painted geometry continues to carry runtime state.
	const auto AvoidIfPossiblePhysical = Physical.Get_Color();
	auto HardExclude = CopiedSnapshot;
	HardExclude._AvoidanceVolumes[0]._TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::HardExclude;
	TestTrue(TEXT("hard-exclude policy reconciles"), Adapter.Reconcile(HardExclude, *Fixture._Target));
	TestNotEqual(TEXT("hard-exclude policy has a distinct physical color"),
		Adapter.Get_RoleAppearance(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical).Get_Color(), AvoidIfPossiblePhysical);

    auto WithUnrenderableInvalid = CopiedSnapshot;
    auto InvalidVolume = FCkCrowdDebugger_3dAvoidanceVolumeSnapshot{};
    InvalidVolume._Identity = 7002;
    InvalidVolume._State = ECkCrowdDebugger_AvoidanceVolumeState::Invalid;
    WithUnrenderableInvalid._AvoidanceVolumes.Add(InvalidVolume);
    TestTrue(TEXT("an intentionally geometry-less invalid volume does not poison the scene"),
        Adapter.Reconcile(WithUnrenderableInvalid, *Fixture._Target));
    TestEqual(TEXT("geometry-less invalid volume adds no misleading physical item"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical), 1);

    Fixture._Target->Reset_FrameStats();
    TestTrue(TEXT("unchanged avoidance volumes reconcile"), Adapter.Reconcile(WithUnrenderableInvalid, *Fixture._Target));
    const auto& SteadyStats = Fixture._Target->Get_Stats();
    TestEqual(TEXT("unchanged avoidance volumes add no instances"), SteadyStats.Get_InstancesAdded(), 0);
    TestEqual(TEXT("unchanged avoidance volumes update no instances"), SteadyStats.Get_InstancesUpdated(), 0);
    TestEqual(TEXT("unchanged avoidance volumes remove no instances"), SteadyStats.Get_InstancesRemoved(), 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_AvoidanceVolumesReconcileLifecycle,
    "Ck.CrowdDebugger.Viewport3d.AvoidanceVolumesReconcileLifecycle",
    ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto FCkCrowdDebugger3dAdapter_AvoidanceVolumesReconcileLifecycle::RunTest(const FString&) -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._AvoidanceVolumes = {MakeAvoidanceVolume(7001), MakeAvoidanceVolume(7002, FVector{500.0f, 0.0f, 0.0f})};

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("initial avoidance volumes reconcile"), Adapter.Reconcile(Snapshot, *Fixture._Target));
    Fixture._Target->Reset_FrameStats();

    auto Reordered = Snapshot;
    Swap(Reordered._AvoidanceVolumes[0], Reordered._AvoidanceVolumes[1]);
    TestTrue(TEXT("reordered avoidance volumes reconcile"), Adapter.Reconcile(Reordered, *Fixture._Target));
    TestEqual(TEXT("reorder preserves retained identities without additions"), Fixture._Target->Get_Stats().Get_InstancesAdded(), 0);
    TestEqual(TEXT("reorder preserves retained identities without removals"), Fixture._Target->Get_Stats().Get_InstancesRemoved(), 0);

    const auto PaintedItemBefore = Adapter.Get_TargetItemId(
        ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted, 0);
    if (NOT TestTrue(TEXT("updated volume has a retained painted item"), PaintedItemBefore.IsSet()))
    { return false; }
    const auto PaintedBoundsBefore = Fixture._Target->Get_ItemBounds(*PaintedItemBefore);
    if (NOT TestTrue(TEXT("updated volume has painted bounds before mutation"), PaintedBoundsBefore.IsSet()))
    { return false; }

    auto Updated = Reordered;
    Updated._AvoidanceVolumes[0]._PaintedWorldHalfExtents.X += 50.0f;
    Updated._AvoidanceVolumes[0]._State = ECkCrowdDebugger_AvoidanceVolumeState::Pending;
	Updated._AvoidanceVolumes[0]._TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::CostOnly;
    Fixture._Target->Reset_FrameStats();
    TestTrue(TEXT("geometry and state update reconcile"), Adapter.Reconcile(Updated, *Fixture._Target));
    const auto PaintedItemAfter = Adapter.Get_TargetItemId(
        ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted, 0);
    TestTrue(TEXT("geometry/state change preserves the retained painted identity"),
        PaintedItemAfter.IsSet() && *PaintedItemAfter == *PaintedItemBefore);
    const auto PaintedBoundsAfter = PaintedItemAfter.IsSet()
        ? Fixture._Target->Get_ItemBounds(*PaintedItemAfter) : TOptional<FBox>{};
    TestTrue(TEXT("geometry change updates the retained painted bounds"),
        PaintedBoundsAfter.IsSet() && NOT PaintedBoundsAfter->Equals(*PaintedBoundsBefore));

    auto Retiring = Updated;
    Retiring._AvoidanceVolumes[0]._State = ECkCrowdDebugger_AvoidanceVolumeState::Retiring;
    TestTrue(TEXT("retiring avoidance volume reconciles"), Adapter.Reconcile(Retiring, *Fixture._Target));
    TestEqual(TEXT("retiring volume omits its no-longer-live influence region"),
        Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumeInfluence), 1);

    Retiring._AvoidanceVolumes.Reset();
    TestTrue(TEXT("empty avoidance snapshot reconciles"), Adapter.Reconcile(Retiring, *Fixture._Target));
    TestFalse(TEXT("empty avoidance snapshot removes physical geometry"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical));
    TestFalse(TEXT("empty avoidance snapshot removes influence geometry"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumeInfluence));
    TestFalse(TEXT("empty avoidance snapshot removes painted geometry"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dAdapter_AvoidanceVolumesFailClosedAcrossWorldChange,
    "Ck.CrowdDebugger.Viewport3d.AvoidanceVolumesFailClosedAcrossWorldChange",
    ck_crowd_debugger_3d_adapter_spec::TestFlags)

auto FCkCrowdDebugger3dAdapter_AvoidanceVolumesFailClosedAcrossWorldChange::RunTest(const FString&) -> bool
{
    using namespace ck_crowd_debugger_3d_adapter_spec;
    auto Fixture = FScopedTarget{};
    auto Snapshot = MakeSnapshot();
    Snapshot._AvoidanceVolumes = {MakeAvoidanceVolume(7001)};
    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("baseline avoidance geometry commits"), Adapter.Reconcile(Snapshot, *Fixture._Target));
    const auto PhysicalItem = Adapter.Get_TargetItemId(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical, 0);
    TestTrue(TEXT("baseline physical item exists"), PhysicalItem.IsSet());
    if (NOT PhysicalItem.IsSet())
    {
        return false;
    }
    const auto BeforeBounds = Fixture._Target->Get_ContentBounds();
    const auto BeforeItems = Fixture._Target->Get_Stats().Get_ItemCount();

    auto Invalid = Snapshot;
    auto MalformedGeometry = FCkCrowdDebugger_3dAvoidanceVolumeSnapshot{};
    MalformedGeometry._Identity = 7002;
    MalformedGeometry._HasValidGeometry = true;
    Invalid._AvoidanceVolumes.Add(MalformedGeometry);
    AddExpectedError(TEXT("Crowd debug-scene adapter rejected"), EAutomationExpectedErrorFlags::Contains, 2);
    TestFalse(TEXT("a later invalid avoidance volume aborts the target transaction"),
        Adapter.Reconcile(Invalid, *Fixture._Target));
    TestTrue(TEXT("failed transaction preserves prior physical item"),
        Fixture._Target->Get_ItemBounds(*PhysicalItem).IsSet());
    TestTrue(TEXT("failed transaction preserves prior bounds"), Fixture._Target->Get_ContentBounds().Equals(BeforeBounds));
    TestEqual(TEXT("failed transaction preserves prior item count"), Fixture._Target->Get_Stats().Get_ItemCount(), BeforeItems);

    Adapter.Reset_ForWorldChange(*Fixture._Target);
    TestEqual(TEXT("world reset removes retained avoidance geometry"), Fixture._Target->Get_Stats().Get_ItemCount(), 0);
    TestFalse(TEXT("world reset clears avoidance physical role"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePhysical));
    TestFalse(TEXT("world reset clears avoidance influence role"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumeInfluence));
    TestFalse(TEXT("world reset clears avoidance painted role"),
        Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::AvoidanceVolumePainted));
    return true;
}

#endif
