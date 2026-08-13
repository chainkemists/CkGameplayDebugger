#if WITH_DEV_AUTOMATION_TESTS
#include "CkSmDebugger/Graph/CkSmRuntimeGraphModel.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedGraphMotionSelection
    {
        FScopedGraphMotionSelection()
        {
            if (const auto* Settings = UCkDebuggerStyleSettings::Get())
            {
                Selection = Settings->Selection;
            }
        }

        ~FScopedGraphMotionSelection()
        {
            if (auto* Settings = UCkDebuggerStyleSettings::Get_Mutable())
            {
                Settings->Selection = Selection;
            }
        }

        FCkDebuggerStyleSelection Selection;
    };
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_GraphMotionTest,
                                 "Ck.SmDebugger.RuntimeGraph.GraphMotion",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_GraphMotionTest::RunTest(const FString&) -> bool
{
    const auto Quick = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Quick);
    const auto Measured = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Measured);
    const auto Deliberate = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Deliberate);
    TestTrue(TEXT("motion presets increase current-state duration"),
             Quick.CurrentStateFadeSeconds < Measured.CurrentStateFadeSeconds
             && Measured.CurrentStateFadeSeconds < Deliberate.CurrentStateFadeSeconds);
    TestTrue(TEXT("motion presets increase transition duration"),
             Quick.TransitionFlashSeconds < Measured.TransitionFlashSeconds
             && Measured.TransitionFlashSeconds < Deliberate.TransitionFlashSeconds);

    const auto StyleGuard = FScopedGraphMotionSelection{};
    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();
    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }
    Settings->Selection.GraphMotion = ECkDebugAxis_GraphMotion::Measured;

    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(2);
    Info.States[0].StateName = TEXT("A");
    Info.States[0].IsCurrentState = true;
    Info.States[1].StateName = TEXT("B");
    Info.Transitions.SetNum(1);
    Info.Transitions[0].SourceStateIndex = 0;
    Info.Transitions[0].TargetStateIndex = 1;

    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1);
    Info.States[0].IsCurrentState = false;
    Info.States[1].IsCurrentState = true;
    Model.UpdateRuntimeState(Info);
    Model.TriggerLivePresentation(0, 1, {TEXT("A")});
    Model.TickLivePresentation(0.30f);

    const auto* Previous = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(0));
    const auto* Current = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(1));
    const auto* TransitionEdge = Model.GetScene().Edges.FindByPredicate(
        [](const FCkSmRuntimeGraphEdge& InEdge)
        {
            return InEdge.SourceId == FCkSmRuntimeGraphModel::GetStateId(0)
                   && InEdge.TargetId == FCkSmRuntimeGraphModel::GetStateId(1);
        });
    TestTrue(TEXT("current-state border reaches full strength on its measured duration"),
             Current && FMath::IsNearlyEqual(Current->BorderGlowAlpha, 1.0f));
    TestTrue(TEXT("previous-state border uses the slower fade-out duration"),
             Previous && FMath::IsNearlyEqual(Previous->BorderGlowAlpha, 2.0f / 3.0f, 0.001f));
    TestTrue(TEXT("source state does not receive the yellow event pulse"),
             Previous && FMath::IsNearlyZero(Previous->StateEventAlpha));
    TestTrue(TEXT("target state event uses the selected motion duration"),
             Current && FMath::IsNearlyEqual(Current->StateEventAlpha, 8.0f / 11.0f, 0.001f));
    TestTrue(TEXT("transition flash uses the selected motion duration"),
             TransitionEdge
              && FMath::IsNearlyEqual(TransitionEdge->LiveFlashAlpha, 0.75f, 0.001f));
    Model.TriggerLivePresentation(1, 1, {TEXT("A")});
    TestTrue(TEXT("an unchanged data refresh preserves the in-flight transition flash"),
             TransitionEdge
             && FMath::IsNearlyEqual(TransitionEdge->LiveFlashAlpha, 0.75f, 0.001f));
    Model.TickLivePresentation(0.30f);
    TestTrue(TEXT("the preserved transition flash continues decaying on presentation ticks"),
             TransitionEdge
             && FMath::IsNearlyEqual(TransitionEdge->LiveFlashAlpha, 0.50f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_LiveEventBatchTest,
                                 "Ck.SmDebugger.RuntimeGraph.LiveEventBatch",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_LiveEventBatchTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(4);
    const auto StateNames = TArray<FString>{TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y")};
    for (auto Index = 0; Index < Info.States.Num(); ++Index)
    {
        Info.States[Index].StateName = StateNames[Index];
    }
    Info.States[2].IsSubSmNode = true;
    Info.States[2].SubSmParentStateName = TEXT("Parent");
    Info.States[2].SubSmParentStateIndex = 1;
    Info.States[3].IsSubSmNode = true;
    Info.States[3].SubSmParentStateName = TEXT("Parent");
    Info.States[3].SubSmParentStateIndex = 1;
    Info.Transitions.SetNum(3);
    Info.Transitions[0].SourceStateIndex = 0;
    Info.Transitions[0].TargetStateIndex = 1;
    Info.Transitions[0].SourceStateName = TEXT("A");
    Info.Transitions[0].TargetStateName = TEXT("B");
    Info.Transitions[0].Conditions.AddDefaulted_GetRef().ClassName = TEXT("OtherCondition");
    Info.Transitions[1].SourceStateIndex = 0;
    Info.Transitions[1].TargetStateIndex = 1;
    Info.Transitions[1].SourceStateName = TEXT("A");
    Info.Transitions[1].TargetStateName = TEXT("B");
    Info.Transitions[1].Order = 1;
    Info.Transitions[1].Conditions.AddDefaulted_GetRef().ClassName = TEXT("FiredCondition");
    Info.Transitions[2].SourceStateIndex = 2;
    Info.Transitions[2].TargetStateIndex = 3;
    Info.Transitions[2].SourceStateName = TEXT("X");
    Info.Transitions[2].TargetStateName = TEXT("Y");
    Info.Transitions[2].IsSubSmTransition = true;

    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1);
    auto RootEvent = FCkSmDebugger_HistoryEntry{};
    RootEvent.LiveEventId = 1;
    RootEvent.FromStateName = TEXT("A");
    RootEvent.ToStateName = TEXT("B");
    RootEvent.ConditionNames.Add(TEXT("FiredCondition"));
    auto NestedEvent = FCkSmDebugger_HistoryEntry{};
    NestedEvent.LiveEventId = 2;
    NestedEvent.FromStateName = TEXT("X");
    NestedEvent.ToStateName = TEXT("Y");
    NestedEvent.SubSmParentStateName = TEXT("Parent");
    Model.TriggerLivePresentation({RootEvent, NestedEvent});

    for (const auto Index : {0, 2})
    {
        const auto* Node = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(Index));
        TestTrue(*FString::Printf(TEXT("event does not outline source state %d"), Index),
                 Node && FMath::IsNearlyZero(Node->StateEventAlpha));
    }
    for (const auto Index : {1, 3})
    {
        const auto* Node = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(Index));
        TestTrue(*FString::Printf(TEXT("event outlines target state %d"), Index),
                 Node && FMath::IsNearlyEqual(Node->StateEventAlpha, 1.0f));
    }
    auto FlashedRootEdges = 0;
    auto FlashedNestedEdges = 0;
    for (const auto& Edge : Model.GetScene().Edges)
    {
        if (Edge.TransitionId == 0 || Edge.LiveFlashAlpha <= 0.0f)
        {
            continue;
        }
        const auto* TransitionNode = Model.FindNodeById(Edge.TransitionId);
        if (TransitionNode && TransitionNode->Transition)
        {
            FlashedRootEdges += TransitionNode->Transition->SourceStateIndex == 0 ? 1 : 0;
            FlashedNestedEdges += TransitionNode->Transition->SourceStateIndex == 2 ? 1 : 0;
        }
    }
    TestEqual(TEXT("only the condition-matched parallel root edge flashes"), FlashedRootEdges, 1);
    TestEqual(TEXT("the nested edge flashes from the same collected batch"), FlashedNestedEdges, 1);

    Model.TickLivePresentation(0.25f);
    const auto AlphaBeforeRebuild =
        Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(3))->StateEventAlpha;
    Model.Rebuild(Info, true, 1, 351, 120, false);
    const auto* RebuiltNested = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(3));
    TestTrue(TEXT("topology rebuild preserves a nested event already fading"),
             RebuiltNested
             && FMath::IsNearlyEqual(RebuiltNested->StateEventAlpha, AlphaBeforeRebuild));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_EntryLayoutTest,
                                 "Ck.SmDebugger.RuntimeGraph.EntryLayout",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_EntryLayoutTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(2);
    Info.States[0].StateName = TEXT("FirstInArray");
    Info.States[0].StateClass = UCk_SmState_EntityScript::StaticClass();
    Info.States[1].StateName = TEXT("ConfiguredInitial");
    Info.States[1].StateClass = UCk_SmState_EntityScript::StaticClass();
    Info.InitialStateClass = Info.States[1].StateClass;

    // Give the first state a distinct class identity so the configured initial state is index 1.
    Info.States[0].StateClass = nullptr;
    auto Model = FCkSmRuntimeGraphModel{};
    Model.Rebuild(Info, true, 1, 240, 100, false);

    const auto* Entry = Model.FindNodeById(FCkSmRuntimeGraphModel::GetEntryId());
    TestNotNull(TEXT("Configured initial state produces an Entry node"), Entry);
    if (Entry)
    {
        TestEqual<double>(TEXT("Entry uses state-family width"), Entry->Size.X, 140.0);
        TestEqual<double>(TEXT("Entry uses state-family height"), Entry->Size.Y, 62.0);
    }
    const auto* EntryEdge = Model.GetScene().Edges.FindByPredicate(
        [](const FCkSmRuntimeGraphEdge& InEdge)
        {
            return InEdge.SourceId == FCkSmRuntimeGraphModel::GetEntryId();
        });
    TestNotNull(TEXT("Entry contributes a layout edge"), EntryEdge);
    if (EntryEdge)
    {
        TestEqual(TEXT("Entry targets the configured initial state"),
                  EntryEdge->TargetId,
                  FCkSmRuntimeGraphModel::GetStateId(1));
    }
    const auto* Initial = Model.FindNodeById(FCkSmRuntimeGraphModel::GetStateId(1));
    if (Entry && Initial)
    {
        TestTrue(TEXT("Entry occupies the layout rank before the initial state"),
                 Entry->Position.X + Entry->Size.X < Initial->Position.X);
    }

    Info.InitialStateClass = UCk_SmState_EntityScript::StaticClass();
    Info.States[1].StateClass = nullptr;
    Model.Rebuild(Info, true, 1, 240, 100, false);
    TestNull(TEXT("Unresolved configured initial state fails closed without a false Entry"),
             Model.FindNodeById(FCkSmRuntimeGraphModel::GetEntryId()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_StructureHashTest,
                                 "Ck.SmDebugger.RuntimeGraph.StructureHash",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_StructureHashTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(2);
    Info.States[0].StateName = TEXT("A");
    Info.States[1].StateName = TEXT("B");
    Info.States[0].Tasks.Add({});
    Info.States[0].Tasks[0].ClassName = TEXT("Task");
    Info.Transitions.SetNum(1);
    Info.Transitions[0].SourceStateIndex = 0;
    Info.Transitions[0].TargetStateIndex = 1;
    Info.Transitions[0].Conditions.Add({});
    Info.Transitions[0].Conditions[0].ClassName = TEXT("Condition");

    const auto Initial = FCkSmRuntimeGraphModel::ComputeStructureHash(
        Info, true, 1, 350, 120, false);
    Info.States[0].IsCurrentState = true;
    Info.States[0].DwellTimeSeconds = 7.5;
    Info.States[0].HasBeenVisited = true;
    Info.States[0].HasEntryBreakpoint = true;
    Info.States[0].Tasks[0].LastResult = ECk_SmTaskResult::Succeeded;
    Info.Transitions[0].Conditions[0].Result = ECk_SmConditionResult::Pass;
    Info.Transitions[0].SatisfiedCount = 1;
    Info.Transitions[0].TotalCount = 1;
    Info.Transitions[0].HasBreakpoint = true;
    TestEqual(TEXT("Live values retain the existing card structure"),
              FCkSmRuntimeGraphModel::ComputeStructureHash(
                  Info, true, 1, 350, 120, false),
              Initial);

    Info.States[0].Tasks[0].Mode = ECk_SmTaskMode::Tick;
    TestNotEqual(TEXT("Task mode invalidates the geometry-bearing structure"),
                 FCkSmRuntimeGraphModel::ComputeStructureHash(
                     Info, true, 1, 350, 120, false),
                 Initial);
    Info.States[0].Tasks[0].Mode = ECk_SmTaskMode::EnterExitOnly;

    Info.Transitions[0].Conditions[0].Mode = ECk_SmConditionMode::EventDriven;
    TestNotEqual(TEXT("Condition mode invalidates the geometry-bearing structure"),
                 FCkSmRuntimeGraphModel::ComputeStructureHash(
                     Info, true, 1, 350, 120, false),
                 Initial);
    Info.Transitions[0].Conditions[0].Mode = ECk_SmConditionMode::Polled;

    Info.States[0].ScriptClass = UCk_SmState_EntityScript::StaticClass();
    TestNotEqual(TEXT("Effective state script invalidates the override structure"),
                 FCkSmRuntimeGraphModel::ComputeStructureHash(
                     Info, true, 1, 350, 120, false),
                 Initial);
    Info.States[0].ScriptClass = nullptr;

    Info.States[0].RequestedScriptClass = UCk_SmState_EntityScript::StaticClass();
    TestNotEqual(TEXT("Requested state script invalidates the override structure"),
                 FCkSmRuntimeGraphModel::ComputeStructureHash(
                     Info, true, 1, 350, 120, false),
                 Initial);
    Info.States[0].RequestedScriptClass = nullptr;

    Info.States[0].StateName = TEXT("Renamed");
    TestNotEqual(TEXT("Topology-facing state names invalidate the structure"),
                 FCkSmRuntimeGraphModel::ComputeStructureHash(
                     Info, true, 1, 350, 120, false),
                 Initial);
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
    const auto* Badge = Model.GetScene().Nodes.FindByPredicate(
        [](const FCkSmRuntimeGraphNode& Node)
        {
            return Node.Kind == ECkSmRuntimeGraphNodeKind::Transition;
        });
    TestNotNull(TEXT("Transition emits a brush-sized badge"), Badge);
    TestEqual<double>(TEXT("Transition badge keeps the authored 25px brush footprint"),
                      Badge ? Badge->Size.X : 0.0,
                      25.0);
    TestEqual<double>(TEXT("Transition badge does not clip vertically"),
                      Badge ? Badge->Size.Y : 0.0,
                      25.0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_ManualCompoundGeometryTest,
                                 "Ck.SmDebugger.RuntimeGraph.ManualCompoundGeometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_ManualCompoundGeometryTest::RunTest(const FString&) -> bool
{
    auto Scene = FCkSmRuntimeGraphScene{};
    auto Compound = FCkSmRuntimeGraphNode{};
    Compound.Id = FCkSmRuntimeGraphModel::GetCompoundId(0);
    Compound.Kind = ECkSmRuntimeGraphNodeKind::Compound;
    Compound.StateIndex = 0;
    Compound.Position = FVector2D{100.0f, 200.0f};
    Compound.Size = FVector2D{400.0f, 200.0f};
    Scene.Nodes.Add(Compound);
    for (const auto [StateIndex, Position] : TArray<TPair<int32, FVector2D>>{
             {1, FVector2D{120.0f, 250.0f}},
             {2, FVector2D{350.0f, 250.0f}}})
    {
        auto Child = FCkSmRuntimeGraphNode{};
        Child.Id = FCkSmRuntimeGraphModel::GetStateId(StateIndex);
        Child.Kind = ECkSmRuntimeGraphNodeKind::State;
        Child.StateIndex = StateIndex;
        Child.Position = Position;
        Child.Size = FVector2D{100.0f, 50.0f};
        Child.State = MakeShared<FCkSmDebugger_StateInfo>();
        Child.State->IsSubSmNode = true;
        Child.State->SubSmParentStateIndex = 0;
        Scene.Nodes.Add(MoveTemp(Child));
    }

    auto Overrides = TMap<uint64, FVector2D>{};
    Overrides.Add(FCkSmRuntimeGraphModel::GetStateId(1), FVector2D{200.0f, 290.0f});
    Overrides.Add(FCkSmRuntimeGraphModel::GetStateId(2), FVector2D{430.0f, 290.0f});
    auto Geometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(Scene, Scene.Nodes[0], Overrides);
    TestEqual(TEXT("uniform child movement translates the compound wrapper"),
              Geometry.Position,
              FVector2D{180.0f, 240.0f});
    TestEqual(TEXT("uniform child movement preserves the compound size"),
              Geometry.Size,
              FVector2D{400.0f, 200.0f});

    Overrides[FCkSmRuntimeGraphModel::GetStateId(2)].X += 100.0f;
    Geometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(Scene, Scene.Nodes[0], Overrides);
    TestEqual(TEXT("rearranging one child keeps the wrapper attached to its leading margin"),
              Geometry.Position,
              FVector2D{180.0f, 240.0f});
    TestEqual(TEXT("rearranging one child expands the wrapper to retain containment"),
              Geometry.Size,
              FVector2D{500.0f, 200.0f});
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
    Info.States[0].StateName = TEXT("Game.Sm.ParentState");
    Info.States[1].StateName = TEXT("Game.Sm.ChildState");
    Info.States[1].IsSubSmNode = true;
    Info.States[1].SubSmParentStateIndex = 0;
    Info.States[2].StateName = TEXT("Game.Sm.GrandchildState");
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

    const auto CountEdges = [&Model](const uint64 InSourceId, const uint64 InTargetId)
    {
        auto Count = 0;
        for (const auto& Edge : Model.GetScene().Edges)
        {
            Count += Edge.SourceId == InSourceId && Edge.TargetId == InTargetId ? 1 : 0;
        }
        return Count;
    };
    const auto OuterIngress = Model.GetScene().Edges.FindByPredicate(
        [](const FCkSmRuntimeGraphEdge& InEdge)
        {
            return InEdge.SourceId == FCkSmRuntimeGraphModel::GetStateId(0)
                   && InEdge.TargetId == FCkSmRuntimeGraphModel::GetCompoundId(0);
        });
    const auto InnerIngress = Model.GetScene().Edges.FindByPredicate(
        [](const FCkSmRuntimeGraphEdge& InEdge)
        {
            return InEdge.SourceId == FCkSmRuntimeGraphModel::GetStateId(1)
                   && InEdge.TargetId == FCkSmRuntimeGraphModel::GetCompoundId(1);
        });
    TestTrue(TEXT("Outer compound has one directed parent ingress"),
             OuterIngress && OuterIngress->bDirected);
    TestTrue(TEXT("Nested compound has one directed parent ingress"),
             InnerIngress && InnerIngress->bDirected);
    TestEqual(TEXT("Outer compound ingress is not duplicated"),
              CountEdges(FCkSmRuntimeGraphModel::GetStateId(0),
                         FCkSmRuntimeGraphModel::GetCompoundId(0)),
              1);
    TestEqual(TEXT("Nested compound ingress is not duplicated"),
              CountEdges(FCkSmRuntimeGraphModel::GetStateId(1),
                         FCkSmRuntimeGraphModel::GetCompoundId(1)),
              1);
    TestEqual(TEXT("Parent ingress does not substitute its first child"),
              CountEdges(FCkSmRuntimeGraphModel::GetStateId(0),
                         FCkSmRuntimeGraphModel::GetStateId(1)),
              0);
    TestEqual(TEXT("Depth one shortens the outer compound title"),
              Outer ? Outer->Label : FString{},
              FString{TEXT("ParentState")});
    TestEqual(TEXT("Depth one shortens the nested compound title"),
              Inner ? Inner->Label : FString{},
              FString{TEXT("ChildState")});

    Model.Rebuild(Info, true, 0);
    const auto* FullOuter = Model.FindNodeById(FCkSmRuntimeGraphModel::GetCompoundId(0));
    TestEqual(TEXT("Depth zero preserves the full compound title"),
              FullOuter ? FullOuter->Label : FString{},
              FString{TEXT("Game.Sm.ParentState")});
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_ValueOwnedPreviewTest,
                                 "Ck.SmDebugger.RuntimeGraph.ValueOwnedPreview",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_ValueOwnedPreviewTest::RunTest(const FString&) -> bool
{
    // Preview/Test feed the canvas an owned DTO, never a transient UEdGraph. This
    // remains deterministic in every target which compiles developer tools.
    auto Preview = FCkSmDebugger_SmInfo{};
    Preview.States.SetNum(2);
    Preview.States[0].StateName = TEXT("Preview.Initial");
    Preview.States[1].StateName = TEXT("Preview.Nested");
    Preview.States[1].IsSubSmNode = true;
    Preview.States[1].SubSmParentStateIndex = 0;
    auto Transition = FCkSmDebugger_TransitionInfo{};
    Transition.SourceStateIndex = 0;
    Transition.TargetStateIndex = 1;
    Transition.IsSubSmTransition = true;
    Preview.Transitions.Add(MoveTemp(Transition));

    auto Model = FCkSmRuntimeGraphModel{};
    // NameDepth 0 = full names: the assertion below matches the state's full
    // "Preview.Nested" label, which any positive depth would shorten to
    // "Nested" via SCkDebug_NameLabel::Get_ShortName and fail spuriously.
    Model.Rebuild(Preview, true, 0);
    TestTrue(TEXT("Runtime preview DTO emits nested state"),
             Model.GetScene().Nodes.ContainsByPredicate([](const FCkSmRuntimeGraphNode& Node)
             { return Node.Kind == ECkSmRuntimeGraphNodeKind::State && Node.Label == TEXT("Preview.Nested"); }));
    TestTrue(TEXT("Runtime preview DTO emits transition"), NOT Model.GetScene().Edges.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_RecursiveManualCompoundGeometryTest,
                                 "Ck.SmDebugger.RuntimeGraph.RecursiveManualCompoundGeometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_RecursiveManualCompoundGeometryTest::RunTest(const FString&) -> bool
{
    auto Scene = FCkSmRuntimeGraphScene{};
    auto MakeState = [](const int32 InIndex, const int32 InParent, const FVector2D InPosition)
    {
        auto Node = FCkSmRuntimeGraphNode{};
        Node.Id = FCkSmRuntimeGraphModel::GetStateId(InIndex);
        Node.Kind = ECkSmRuntimeGraphNodeKind::State;
        Node.StateIndex = InIndex;
        Node.Position = InPosition;
        Node.Size = FVector2D{100.0f, 50.0f};
        Node.State = MakeShared<FCkSmDebugger_StateInfo>();
        Node.State->IsSubSmNode = true;
        Node.State->SubSmParentStateIndex = InParent;
        return Node;
    };
    auto Outer = FCkSmRuntimeGraphNode{};
    Outer.Id = FCkSmRuntimeGraphModel::GetCompoundId(0);
    Outer.Kind = ECkSmRuntimeGraphNodeKind::Compound;
    Outer.StateIndex = 0;
    Outer.Position = FVector2D{0.0f, 0.0f};
    Outer.Size = FVector2D{500.0f, 400.0f};
    Scene.Nodes.Add(Outer);
    Scene.Nodes.Add(MakeState(1, 0, FVector2D{40.0f, 50.0f}));
    auto Inner = FCkSmRuntimeGraphNode{};
    Inner.Id = FCkSmRuntimeGraphModel::GetCompoundId(1);
    Inner.Kind = ECkSmRuntimeGraphNodeKind::Compound;
    Inner.StateIndex = 1;
    Inner.Position = FVector2D{40.0f, 150.0f};
    Inner.Size = FVector2D{220.0f, 140.0f};
    Scene.Nodes.Add(Inner);
    Scene.Nodes.Add(MakeState(2, 1, FVector2D{60.0f, 200.0f}));

    auto Overrides = TMap<uint64, FVector2D>{};
    Overrides.Add(FCkSmRuntimeGraphModel::GetStateId(1), FVector2D{140.0f, 150.0f});
    Overrides.Add(FCkSmRuntimeGraphModel::GetStateId(2), FVector2D{160.0f, 300.0f});
    const auto OuterGeometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(Scene, Scene.Nodes[0], Overrides);
    const auto InnerGeometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(Scene, Scene.Nodes[2], Overrides);
    TestEqual(TEXT("recursive manual states translate the outer compound"),
              OuterGeometry.Position, FVector2D{100.0f, 100.0f});
    TestEqual(TEXT("recursive manual states preserve the outer compound size"),
              OuterGeometry.Size, FVector2D{500.0f, 400.0f});
    TestTrue(TEXT("derived outer compound contains the derived inner compound"),
             OuterGeometry.Position.X <= InnerGeometry.Position.X &&
             OuterGeometry.Position.Y <= InnerGeometry.Position.Y &&
             OuterGeometry.Position.X + OuterGeometry.Size.X >= InnerGeometry.Position.X + InnerGeometry.Size.X &&
             OuterGeometry.Position.Y + OuterGeometry.Size.Y >= InnerGeometry.Position.Y + InnerGeometry.Size.Y);

    auto ShallowOuterScene = Scene;
    ShallowOuterScene.Nodes[0].Size = FVector2D{180.0f, 140.0f};
    const auto ShallowOuterGeometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(
        ShallowOuterScene, ShallowOuterScene.Nodes[0], {});
    const auto StoredInner = FCkSmRuntimeGraphModel::ResolveNodeGeometry(
        ShallowOuterScene, ShallowOuterScene.Nodes[2], {});
    TestTrue(TEXT("an initially shallow ancestor expands around its nested compound"),
             ShallowOuterGeometry.Position.X <= StoredInner.Position.X &&
             ShallowOuterGeometry.Position.Y <= StoredInner.Position.Y &&
             ShallowOuterGeometry.Position.X + ShallowOuterGeometry.Size.X >=
                 StoredInner.Position.X + StoredInner.Size.X &&
             ShallowOuterGeometry.Position.Y + ShallowOuterGeometry.Size.Y >=
                 StoredInner.Position.Y + StoredInner.Size.Y);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphModel_CyclicCompoundGeometryTest,
                                 "Ck.SmDebugger.RuntimeGraph.CyclicCompoundGeometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphModel_CyclicCompoundGeometryTest::RunTest(const FString&) -> bool
{
    auto Scene = FCkSmRuntimeGraphScene{};
    const auto CyclicStates = TArray<TPair<int32, FVector2D>>{
        TPair<int32, FVector2D>{0, FVector2D{20.0f, 40.0f}},
        TPair<int32, FVector2D>{1, FVector2D{180.0f, 40.0f}}};
    for (const auto& [Index, Position] : CyclicStates)
    {
        auto State = FCkSmRuntimeGraphNode{};
        State.Id = FCkSmRuntimeGraphModel::GetStateId(Index);
        State.Kind = ECkSmRuntimeGraphNodeKind::State;
        State.StateIndex = Index;
        State.Position = Position;
        State.Size = FVector2D{80.0f, 40.0f};
        State.State = MakeShared<FCkSmDebugger_StateInfo>();
        State.State->IsSubSmNode = true;
        State.State->SubSmParentStateIndex = Index == 0 ? 1 : 0;
        Scene.Nodes.Add(MoveTemp(State));
    }
    for (const auto Index : TArray<int32>{0, 1})
    {
        auto Compound = FCkSmRuntimeGraphNode{};
        Compound.Id = FCkSmRuntimeGraphModel::GetCompoundId(Index);
        Compound.Kind = ECkSmRuntimeGraphNodeKind::Compound;
        Compound.StateIndex = Index;
        Compound.Position = FVector2D{0.0f, static_cast<float>(Index) * 120.0f};
        Compound.Size = FVector2D{320.0f, 100.0f};
        Scene.Nodes.Add(MoveTemp(Compound));
    }
    auto Overrides = TMap<uint64, FVector2D>{};
    Overrides.Add(FCkSmRuntimeGraphModel::GetStateId(0), FVector2D{40.0f, 80.0f});
    const auto Geometry = FCkSmRuntimeGraphModel::ResolveNodeGeometry(Scene, Scene.Nodes[2], Overrides);
    TestTrue(TEXT("cyclic compound geometry returns finite stored-or-derived values"),
             FMath::IsFinite(Geometry.Position.X) && FMath::IsFinite(Geometry.Position.Y) &&
             FMath::IsFinite(Geometry.Size.X) && FMath::IsFinite(Geometry.Size.Y));
    TestTrue(TEXT("cyclic compound geometry retains a positive stored-or-contained footprint"),
             Geometry.Size.X >= 160.0f && Geometry.Size.Y >= 100.0f);
    return true;
}
#endif
