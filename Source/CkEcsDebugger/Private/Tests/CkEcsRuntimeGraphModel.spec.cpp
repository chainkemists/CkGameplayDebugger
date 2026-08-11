#if WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Graph/CkEcsRuntimeGraphModel.h"

#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsRuntimeGraphLayout_UsesLegacyMeasuredSpacing,
                                 "Ck.EcsDebugger.RuntimeGraph.Layout.UsesLegacyMeasuredSpacing",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FCkEcsRuntimeGraphLayout_UsesLegacyMeasuredSpacing::RunTest(const FString& /*InParameters*/)
    -> bool
{
    auto Input = FCkEcsRuntimeGraphLayoutInput{};
    Input.CenterLabel = TEXT("Center");
    Input.OwnerLabels = {TEXT("Owner")};
    for (auto Index = 0; Index < 7; ++Index)
    {
        Input.DependentLabels.Add(TEXT("Dependent"));
    }

    const auto Layout = ck::ecs_runtime_graph::BuildLayout(Input);
    TestEqual(TEXT("Measured six-node row widens vertical spacing"), Layout.DynamicSpacingY, 241);
    TestEqual(TEXT("Center sits on the dynamic spacing row"),
              Layout.CenterPosition,
              FIntPoint(0, 241));
    TestEqual(TEXT("Owner is centered"), Layout.OwnerPositions[0], FIntPoint(0, 0));
    TestEqual(TEXT("First dependent is centered in a six-node row"),
              Layout.DependentPositions[0],
              FIntPoint(-625, 482));
    TestEqual(TEXT("Sixth dependent is centered in a six-node row"),
              Layout.DependentPositions[5],
              FIntPoint(625, 482));
    TestEqual(TEXT("Seventh dependent wraps to its own row"),
              Layout.DependentPositions[6],
              FIntPoint(0, 682));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsRuntimeGraphLayout_EstimatesLegacyNodeWidth,
                                 "Ck.EcsDebugger.RuntimeGraph.Layout.EstimatesLegacyNodeWidth",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FCkEcsRuntimeGraphLayout_EstimatesLegacyNodeWidth::RunTest(const FString& /*InParameters*/)
    -> bool
{
    TestEqual(TEXT("Short labels use the legacy minimum width"),
              ck::ecs_runtime_graph::EstimateNodeWidth(TEXT("Short")),
              200);
    TestEqual(TEXT("Long labels use eight pixels per character plus padding"),
              ck::ecs_runtime_graph::EstimateNodeWidth(FString::ChrN(30, TEXT('x'))),
              280);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsRuntimeGraphLayout_PreservesDependentInputOrder,
                                 "Ck.EcsDebugger.RuntimeGraph.Layout.PreservesDependentInputOrder",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FCkEcsRuntimeGraphLayout_PreservesDependentInputOrder::RunTest(const FString& /*InParameters*/)
    -> bool
{
    auto Input = FCkEcsRuntimeGraphLayoutInput{};
    Input.CenterLabel = TEXT("Center");
    Input.DependentLabels = {FString::ChrN(30, TEXT('L')), TEXT("Short")};

    const auto Layout = ck::ecs_runtime_graph::BuildLayout(Input);
    TestTrue(TEXT("First returned dependent remains left of the second"),
             Layout.DependentPositions[0].X < Layout.DependentPositions[1].X);
    TestEqual(TEXT("First dependent keeps its measured width"), Layout.DependentWidths[0], 280);
    TestEqual(TEXT("Second dependent keeps its measured width"), Layout.DependentWidths[1], 200);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsRuntimeGraphTopologyHash_DetectsSameCountDependentReplacement,
    "Ck.EcsDebugger.RuntimeGraph.TopologyHash.DetectsSameCountDependentReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkEcsRuntimeGraphTopologyHash_DetectsSameCountDependentReplacement::RunTest(
    const FString& /*InParameters*/) -> bool
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    {
        const auto Center = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto DependentA = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto DependentB = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto DependentC = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};

        const auto OriginalHash = FCkEcsRuntimeGraphModel::ComputeTopologyHash(
            Center, {}, {}, TArray<FCk_Handle>{DependentA, DependentB});
        const auto ReplacementHash = FCkEcsRuntimeGraphModel::ComputeTopologyHash(
            Center, {}, {}, TArray<FCk_Handle>{DependentA, DependentC});
        const auto ReorderedHash = FCkEcsRuntimeGraphModel::ComputeTopologyHash(
            Center, {}, {}, TArray<FCk_Handle>{DependentB, DependentA});

        TestNotEqual(TEXT("Same-count dependent replacement changes topology"),
                     OriginalHash,
                     ReplacementHash);
        TestEqual(TEXT("Dependent input order does not change topology"),
                  OriginalHash,
                  ReorderedHash);
    }
    Free(RegistrySlot);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsRuntimeGraphTopologyHash_DetectsOwnerChanges,
                                 "Ck.EcsDebugger.RuntimeGraph.TopologyHash.DetectsOwnerChanges",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FCkEcsRuntimeGraphTopologyHash_DetectsOwnerChanges::RunTest(const FString& /*InParameters*/)
    -> bool
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    {
        const auto Center = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto OwnerA = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto OwnerB = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};

        const auto OriginalHash =
            FCkEcsRuntimeGraphModel::ComputeTopologyHash(Center, OwnerA, OwnerA, {});
        const auto LifetimeOwnerChangedHash =
            FCkEcsRuntimeGraphModel::ComputeTopologyHash(Center, OwnerB, OwnerA, {});
        const auto ContextOwnerChangedHash =
            FCkEcsRuntimeGraphModel::ComputeTopologyHash(Center, OwnerA, OwnerB, {});

        TestNotEqual(TEXT("Lifetime-owner replacement changes topology"),
                     OriginalHash,
                     LifetimeOwnerChangedHash);
        TestNotEqual(TEXT("Context-owner replacement changes topology"),
                     OriginalHash,
                     ContextOwnerChangedHash);
    }
    Free(RegistrySlot);
    return true;
}

#endif
