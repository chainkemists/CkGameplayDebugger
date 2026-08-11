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

#endif
