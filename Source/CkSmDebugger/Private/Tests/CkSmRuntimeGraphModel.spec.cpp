#if WITH_DEV_AUTOMATION_TESTS
#include "CkSmDebugger/Graph/CkSmRuntimeGraphModel.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_SizeTest,
                                 "Ck.SmDebugger.RuntimeGraph.StateSize",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_SizeTest::RunTest(const FString&) -> bool
{
    auto State = FCkSmDebugger_StateInfo{};
    State.StateName = TEXT("Test.State");
    const auto MinimumSize = FCkSmRuntimeGraphModel::EstimateStateSize(State, false, 1);
    TestEqual<double>(TEXT("State uses editor minimum width"), MinimumSize.X, 140.0);
    TestEqual<double>(TEXT("State reserves the dwell/status row"), MinimumSize.Y, 62.0);
    State.Tasks.Add({});
    TestEqual<double>(TEXT("Expanded task row uses editor height rule"),
                      FCkSmRuntimeGraphModel::EstimateStateSize(State, true, 1).Y,
                      83.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_IdTest,
                                 "Ck.SmDebugger.RuntimeGraph.StableIds",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_IdTest::RunTest(const FString&) -> bool
{
    TestNotEqual(TEXT("State and transition namespaces do not collide"),
                 FCkSmRuntimeGraphModel::GetStateId(0),
                 FCkSmRuntimeGraphModel::GetTransitionId(0));
    TestNotEqual(TEXT("Entry and compound namespaces do not collide"),
                 FCkSmRuntimeGraphModel::GetEntryId(),
                 FCkSmRuntimeGraphModel::GetCompoundId(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_EdgeKindsTest,
                                 "Ck.SmDebugger.RuntimeGraph.EdgeKinds",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_EdgeKindsTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(2);
    Info.States[0].StateName = TEXT("A");
    Info.States[1].StateName = TEXT("B");
    auto Forward = FCkSmDebugger_TransitionInfo{};
    Forward.SourceStateIndex = 0;
    Forward.TargetStateIndex = 1;
    Info.Transitions.Add(Forward);
    auto Reverse = FCkSmDebugger_TransitionInfo{};
    Reverse.SourceStateIndex = 1;
    Reverse.TargetStateIndex = 0;
    Info.Transitions.Add(Reverse);
    auto Self = FCkSmDebugger_TransitionInfo{};
    Self.SourceStateIndex = 1;
    Self.TargetStateIndex = 1;
    Info.Transitions.Add(Self);
    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1);
    const auto& Edges = Model.GetScene().Edges;
    TestTrue(TEXT("Forward edge detects reverse companion"), Edges[1].bReverse);
    TestEqual(TEXT("Reverse companion receives one separating waypoint"),
              Edges[1].RoutePoints.Num(),
              1);
    TestTrue(TEXT("Self transition remains explicit"), Edges[3].bSelfLoop);
    TestEqual(TEXT("Self transition receives a three-segment loop route"),
              Edges[3].RoutePoints.Num(),
              3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_CachesExitedSubSmTest,
                                 "Ck.SmDebugger.RuntimeGraph.CachesExitedSubSm",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_CachesExitedSubSmTest::RunTest(const FString&) -> bool
{
    auto Live = FCkSmDebugger_SmInfo{};
    Live.States.SetNum(2);
    Live.States[0].StateName = TEXT("Parent");
    Live.States[1].StateName = TEXT("Child");
    Live.States[1].IsSubSmNode = true;
    Live.States[1].SubSmParentStateIndex = 0;
    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Live, true, 1);

    auto Exited = FCkSmDebugger_SmInfo{};
    Exited.States.SetNum(1);
    Exited.States[0].StateName = TEXT("Parent");
    Model.Rebuild(Exited, true, 1);
    const auto* CachedChild = Model.GetScene().Nodes.FindByPredicate(
        [](const FCkSmRuntimeGraphNode& Node)
        {
            return Node.Kind == ECkSmRuntimeGraphNodeKind::State && Node.Label == TEXT("Child");
        });
    TestNotNull(TEXT("Exited child remains in the runtime scene"), CachedChild);
    TestTrue(TEXT("Cached child is marked historical"),
             CachedChild && CachedChild->State && CachedChild->State->IsHistoricalSubSm);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_NestedCompoundTest,
                                 "Ck.SmDebugger.RuntimeGraph.NestedCompounds",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_NestedCompoundTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(3);
    Info.States[0].StateName = TEXT("Parent");
    Info.States[1].StateName = TEXT("Child");
    Info.States[1].IsSubSmNode = true;
    Info.States[1].SubSmParentStateIndex = 0;
    Info.States[2].StateName = TEXT("Grandchild");
    Info.States[2].IsSubSmNode = true;
    Info.States[2].SubSmParentStateIndex = 1;
    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1);
    const auto* Outer = Model.GetScene().Nodes.FindByPredicate(
        [](const FCkSmRuntimeGraphNode& Node)
        {
            return Node.Id == FCkSmRuntimeGraphModel::GetCompoundId(0);
        });
    const auto* Inner = Model.GetScene().Nodes.FindByPredicate(
        [](const FCkSmRuntimeGraphNode& Node)
        {
            return Node.Id == FCkSmRuntimeGraphModel::GetCompoundId(1);
        });
    TestNotNull(TEXT("Parent compound is emitted"), Outer);
    TestNotNull(TEXT("Nested compound is emitted"), Inner);
    TestTrue(TEXT("Nested compound has a measured minimum footprint"),
             Inner && Inner->Size.X >= 160.0f && Inner->Size.Y >= 120.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_CopyPayloadTest,
                                 "Ck.SmDebugger.RuntimeGraph.CopyPayload",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_CopyPayloadTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(2);
    Info.States[0].StateName = TEXT("Game.Sm.Parent");
    Info.States[1].StateName = TEXT("Game.Sm.Child");
    Info.States[1].IsSubSmNode = true;
    Info.States[1].SubSmParentStateIndex = 0;

    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1);

    const auto StatePayload = Model.BuildCopyPayload(FCkSmRuntimeGraphModel::GetStateId(1));
    TestTrue(TEXT("State copy payload is available"), StatePayload.IsSet());
    TestEqual(TEXT("State copy payload preserves full class name"),
              StatePayload.IsSet() ? StatePayload->ClassName : FString{},
              FString{TEXT("Game.Sm.Child")});

    const auto GroupPayload = Model.BuildCopyPayload(FCkSmRuntimeGraphModel::GetCompoundId(0));
    TestTrue(TEXT("Compound copy payload is available"), GroupPayload.IsSet());
    TestEqual(TEXT("Compound copy payload routes its child"),
              GroupPayload.IsSet() ? GroupPayload->ChildClassNames.Num() : 0,
              1);
    TestTrue(TEXT("Non-copyable entry node is rejected"),
             NOT Model.BuildCopyPayload(FCkSmRuntimeGraphModel::GetEntryId()).IsSet());
    return true;
}
#endif
