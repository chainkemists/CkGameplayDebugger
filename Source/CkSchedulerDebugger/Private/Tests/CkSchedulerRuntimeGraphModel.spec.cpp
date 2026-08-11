#if WITH_DEV_AUTOMATION_TESTS

#include "CkSchedulerDebugger/Graph/CkSchedulerRuntimeGraphModel.h"

#include "Misc/AutomationTest.h"

namespace ck_scheduler_runtime_graph_model_test
{
    auto MakeProcessor(const int32 InNodeIndex,
                       const TCHAR* InName,
                       const bool InIsGroupStart = false) -> FCkSchedulerDebugger_ProcessorInfo
    {
        auto Result = FCkSchedulerDebugger_ProcessorInfo{};
        Result.NodeIndex = InNodeIndex;
        Result.ProcessorName = FName(InName);
        Result.DisplayName = InName;
        Result.GroupName = TEXT("TestGroup");
        Result.ExecutionOrder = InNodeIndex;
        Result.IsGroupStart = InIsGroupStart;
        return Result;
    }
} // namespace ck_scheduler_runtime_graph_model_test

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSchedulerRuntimeGraphModel_Test,
                                 "Ck.Scheduler.RuntimeGraph.SelectedProcessorSubgraph",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkSchedulerRuntimeGraphModel_Test::RunTest(const FString&)
{
    using namespace ck_scheduler_runtime_graph_model_test;

    auto Processors = TArray<FCkSchedulerDebugger_ProcessorInfo>{
        MakeProcessor(10, TEXT("Input"), true),
        MakeProcessor(20, TEXT("Selected")),
        MakeProcessor(30, TEXT("Output")),
        MakeProcessor(40, TEXT("Outside")),
    };
    Processors[0].OutEdges = {20};
    Processors[1].InEdges = {10};
    Processors[1].OutEdges = {30};
    Processors[2].InEdges = {20};
    Processors[2].OutEdges = {40};
    Processors[3].InEdges = {30};

    auto Layout = FCkDebugGraphLayoutParams{};
    Layout.SpacingX = 300;
    Layout.SpacingY = 100;
    Layout.CrossingReductionPasses = 4;
    Layout.IsDirectedBFS = true;

    auto Model = FCkSchedulerRuntimeGraphModel{};
    TestTrue(TEXT("first selected-subgraph build changes topology"),
             Model.Rebuild(Processors, 20, Layout));
    TestEqual(TEXT("direct-neighbour node count"), Model.Get_Nodes().Num(), 3);
    TestEqual(TEXT("only internal direct-neighbour edges remain"), Model.Get_Edges().Num(), 2);
    TestTrue(TEXT("input node is present"), Model.Get_NodeById(10).IsValid());
    TestTrue(TEXT("selected node is present"), Model.Get_NodeById(20).IsValid());
    TestTrue(TEXT("output node is present"), Model.Get_NodeById(30).IsValid());
    TestFalse(TEXT("transitive external node is excluded"), Model.Get_NodeById(40).IsValid());
    TestEqual(TEXT("selected id uses stable node index"), Model.Get_SelectedProcessorId(), 20);
    TestEqual(TEXT("group-start root remains at the layout origin"),
              Model.Get_NodeById(10)->Position.X,
              0);

    const auto SelectedNode = Model.Get_NodeById(20);
    Processors[1].MainPassTimeMs = 3.25;
    Processors[1].PumpCountThisFrame = 2;
    Processors[1].WasDirtyThisFrame = true;
    Processors[1].HasDirtyMarker = true;
    TestTrue(TEXT("live timing and dirty state update in place"),
             Model.Update_LiveState(Processors));
    TestTrue(TEXT("stable node identity survives live update"),
             Model.Get_NodeById(20) == SelectedNode);
    TestEqual(TEXT("updated timing is visible"),
              Model.Get_NodeById(20)->Processor.MainPassTimeMs,
              3.25);
    TestEqual(TEXT("updated pump count is visible"),
              Model.Get_NodeById(20)->Processor.PumpCountThisFrame,
              2);
    TestTrue(TEXT("updated dirty border state is visible"),
             Model.Get_NodeById(20)->Processor.WasDirtyThisFrame);

    TestFalse(TEXT("same topology does not churn nodes"), Model.Rebuild(Processors, 20, Layout));
    TestTrue(TEXT("same topology keeps stable node identity"),
             Model.Get_NodeById(20) == SelectedNode);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
