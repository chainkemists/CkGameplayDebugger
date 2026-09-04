#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// The status panel splits into a provider-neutral header and a Recast-only detail section, and both
// halves are decided by the sampled status alone. That is what these assert: no world, no view-model,
// no Slate tree - only what the panel would show for a status a given provider produces.
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"

namespace ck_crowd_debugger_navmesh_status_spec
{
constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

auto
MakeSampledStatus(ECk_NavSurface_Provider InProvider) -> FCkCrowdDebugger_NavmeshStatus
{
    auto Status = FCkCrowdDebugger_NavmeshStatus{};
    Status._Sampled = true;
    Status._Provider = InProvider;
    Status._ProviderIsRecast = InProvider == ECk_NavSurface_Provider::Recast;
    Status._ProviderHealth = ECk_NavSurface_ProviderHealth::Ready;
    Status._SurfaceRevision = 7;
    Status._NavBoundsValid = true;
    Status._NavBoundsMin = FVector{-100.0, -200.0, -50.0};
    Status._NavBoundsMax = FVector{100.0, 200.0, 50.0};
    return Status;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerNavmeshStatus_GroundNavHeaderOnly,
                                 "Ck.CrowdDebugger.NavmeshStatus.GroundNavHeaderOnly",
                                 ck_crowd_debugger_navmesh_status_spec::TestFlags)
auto
    FCkCrowdDebuggerNavmeshStatus_GroundNavHeaderOnly::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_navmesh_status_spec;
    using FPanel = SCkCrowdDebugger_NavmeshStatusPanel;

    const auto Status = MakeSampledStatus(ECk_NavSurface_Provider::GroundNav);

    TestEqual(TEXT("the header names the provider answering this world"),
              FPanel::Format_ProviderText(Status).ToString(), FString{TEXT("GroundNav")});
    TestEqual(TEXT("the header reports that provider's own health"),
              FPanel::Format_HealthText(Status).ToString(), FString{TEXT("Ready")});
    TestTrue(TEXT("the header reports the surface revision it was read at"),
             FPanel::Format_SurfaceRevisionText(Status).ToString().Contains(TEXT("7")));
    TestTrue(TEXT("the header reports the surface bounds as valid"),
             FPanel::Format_BoundsText(Status).ToString().Contains(TEXT("Valid")));
    TestTrue(TEXT("a world Recast does not answer renders no Recast detail, so no row calls it missing"),
             FPanel::Resolve_RecastDetailVisibility(Status) == EVisibility::Collapsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerNavmeshStatus_RecastHeaderAndDetail,
                                 "Ck.CrowdDebugger.NavmeshStatus.RecastHeaderAndDetail",
                                 ck_crowd_debugger_navmesh_status_spec::TestFlags)
auto
    FCkCrowdDebuggerNavmeshStatus_RecastHeaderAndDetail::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_navmesh_status_spec;
    using FPanel = SCkCrowdDebugger_NavmeshStatusPanel;

    auto Status = MakeSampledStatus(ECk_NavSurface_Provider::Recast);
    Status._NavSystemPresent = true;
    Status._NavDataClassName = TEXT("RecastNavMesh");
    Status._DefaultFilterValid = true;
    Status._SupportedAgents = 1;

    TestEqual(TEXT("the header names Recast like any other provider"),
              FPanel::Format_ProviderText(Status).ToString(), FString{TEXT("Recast")});
    TestTrue(TEXT("the Recast detail is shown when Recast is the provider"),
             FPanel::Resolve_RecastDetailVisibility(Status) == EVisibility::Visible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerNavmeshStatus_UnsampledHeaderStaysNeutral,
                                 "Ck.CrowdDebugger.NavmeshStatus.UnsampledHeaderStaysNeutral",
                                 ck_crowd_debugger_navmesh_status_spec::TestFlags)
auto
    FCkCrowdDebuggerNavmeshStatus_UnsampledHeaderStaysNeutral::
    RunTest(const FString&)
    -> bool
{
    using FPanel = SCkCrowdDebugger_NavmeshStatusPanel;

    const auto Status = FCkCrowdDebugger_NavmeshStatus{};

    TestFalse(TEXT("an unsampled status names no provider"),
              FPanel::Format_ProviderText(Status).ToString().Contains(TEXT("Recast")));
    TestFalse(TEXT("an unsampled status reports no health"),
              FPanel::Format_HealthText(Status).ToString().Contains(TEXT("NoData")));
    TestTrue(TEXT("an unsampled status renders no Recast detail"),
             FPanel::Resolve_RecastDetailVisibility(Status) == EVisibility::Collapsed);
    return true;
}

#endif
