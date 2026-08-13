#if WITH_DEV_AUTOMATION_TESTS

#include "CkGoapDebugger/Graph/CkGoapRuntimeGraphModel.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapRuntimeGraphModelTopologyTest,
                                 "Ck.Goap.RuntimeGraph.TopologyHashTracksGoalShape",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapRuntimeGraphModelSelectedGoalTest,
                                 "Ck.Goap.RuntimeGraph.SelectedDualRoleGoalChangesScene",
                                 EAutomationTestFlags::EditorContext |
                                      EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapRuntimeGraphModelNameDepthTest,
                                 "Ck.Goap.RuntimeGraph.NameDepthMatchesEditorNormalization",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapRuntimeGraphModelMeasuredSizeTest,
                                 "Ck.Goap.RuntimeGraph.MeasuredSizeBecomesLayoutGeometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FCkGoapRuntimeGraphModelTopologyTest::RunTest(const FString& Parameters) -> bool
{
    FCkGoapDebugger_PlannerInfo Planner;
    Planner.GoalResolved.Add({FGameplayTag::RequestGameplayTag(TEXT("State.Alert"), false), true});
    const uint32 First = FCkGoapRuntimeGraphModel::ComputeTopologyHash(Planner);
    Planner.GoalResolved[0].Value = false;
    const uint32 Second = FCkGoapRuntimeGraphModel::ComputeTopologyHash(Planner);
    TestNotEqual(TEXT("Goal value participates in topology"), First, Second);

    FCkGoapRuntimeGraphModel Model;
    Model.Rebuild(Planner, FCk_Handle_Goap_Action{}, 1);
    TestEqual(TEXT("Goal-only planner creates one node"), Model.GetNodes().Num(), 1);
    Model.Reset();
    TestEqual(TEXT("Reset clears nodes"), Model.GetNodes().Num(), 0);
    return true;
}

auto FCkGoapRuntimeGraphModelSelectedGoalTest::RunTest(const FString& Parameters) -> bool
{
    FCkGoapDebugger_PlannerInfo Planner;
    Planner.DisplayName = TEXT("Root");
    Planner.GoalResolved.Add(
        {FGameplayTag::RequestGameplayTag(TEXT("State.RootGoal"), false), true});

    FCkGoapDebugger_ActionInfo FirstDualRole;
    FirstDualRole.IsPlannerRole = true;
    FirstDualRole.ClassName = TEXT("Action.FirstDualRole");
    FirstDualRole.Goal.Add(
        {FGameplayTag::RequestGameplayTag(TEXT("State.FirstGoal"), false), true});

    FCkGoapDebugger_ActionInfo SecondDualRole;
    SecondDualRole.IsPlannerRole = true;
    SecondDualRole.ClassName = TEXT("Action.SecondDualRole");
    SecondDualRole.Goal.Add(
        {FGameplayTag::RequestGameplayTag(TEXT("State.SecondGoal"), false), false});

    const uint32 FirstHash = FCkGoapRuntimeGraphModel::ComputeEffectiveGoalHash(Planner,
                                                                                &FirstDualRole);
    const uint32 SecondHash = FCkGoapRuntimeGraphModel::ComputeEffectiveGoalHash(Planner,
                                                                                 &SecondDualRole);
    const uint32 RootHash = FCkGoapRuntimeGraphModel::ComputeEffectiveGoalHash(Planner, nullptr);

    TestNotEqual(TEXT("Changing selected dual-role Action changes effective goal"),
                 FirstHash,
                 SecondHash);
    TestNotEqual(TEXT("First selected goal differs from root goal"), FirstHash, RootHash);
    TestNotEqual(TEXT("Second selected goal differs from root goal"), SecondHash, RootHash);
    return true;
}

auto FCkGoapRuntimeGraphModelNameDepthTest::RunTest(const FString& Parameters) -> bool
{
    TestEqual(TEXT("Default object and generated-class suffix are ignored"),
              FCkGoapRuntimeGraphModel::ComputeMaxNameDepth(TEXT("Default__Ck_Goap_MakeTea_C")),
              3);
    TestEqual(TEXT("Atomic class keeps one depth"),
              FCkGoapRuntimeGraphModel::ComputeMaxNameDepth(TEXT("MakeTea")),
              1);
    return true;
}

auto FCkGoapRuntimeGraphModelMeasuredSizeTest::RunTest(const FString& Parameters) -> bool
{
    auto Planner = FCkGoapDebugger_PlannerInfo{};
    Planner.GoalResolved.Add(
        {FGameplayTag::RequestGameplayTag(TEXT("State.MeasuredGoal"), false), true});

    auto Model = FCkGoapRuntimeGraphModel{};
    Model.Rebuild(Planner, FCk_Handle_Goap_Action{}, 1);
    const auto MeasuredSize = FVector2D{333.0f, 127.0f};
    const auto Changed = Model.ApplyMeasuredNodeSizes({{MAX_uint64, MeasuredSize}});

    TestTrue(TEXT("A realized widget size replaces the estimated geometry"), Changed);
    TestEqual(TEXT("Edges and arrangement consume the realized widget size"),
              Model.GetNodes()[0]->Size,
              MeasuredSize);
    TestFalse(TEXT("Applying the same realized size is stable"),
              Model.ApplyMeasuredNodeSizes({{MAX_uint64, MeasuredSize}}));
    return true;
}

#endif
