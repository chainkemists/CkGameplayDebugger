#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "Widgets/Text/STextBlock.h"

// =====================================================================================================================

namespace ck_debug_graph_canvas_tests
{
    auto MakeNode(uint64 InId, const FVector2D& InPosition, const FVector2D& InSize)
        -> FCkDebug_GraphCanvasNodeGeometry
    {
        auto Result = FCkDebug_GraphCanvasNodeGeometry{};
        Result.Id = InId;
        Result.Position = InPosition;
        Result.Size = InSize;
        return Result;
    }
} // namespace ck_debug_graph_canvas_tests

// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_TransformRoundTrip,
                                 "Ck.DebuggerCommon.GraphCanvas.TransformRoundTrip",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_TransformRoundTrip::RunTest(const FString&)
{
    const auto Transform = FCkDebug_GraphCanvasTransform{FVector2D{31.0f, -17.0f}, 1.75f};
    const auto World = FVector2D{22.0f, 48.0f};
    const auto Screen = SCkDebug_GraphCanvas::World_To_Screen(World, Transform);
    const auto RoundTrip = SCkDebug_GraphCanvas::Screen_To_World(Screen, Transform);

    TestTrue(TEXT("Screen transform round-trips X"), FMath::IsNearlyEqual(World.X, RoundTrip.X));
    TestTrue(TEXT("Screen transform round-trips Y"), FMath::IsNearlyEqual(World.Y, RoundTrip.Y));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_FitBounds,
                                 "Ck.DebuggerCommon.GraphCanvas.FitBounds",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_FitBounds::RunTest(const FString&)
{
    const auto Nodes = TArray<FCkDebug_GraphCanvasNodeGeometry>{
        ck_debug_graph_canvas_tests::MakeNode(1, FVector2D{0.0f, 0.0f}, FVector2D{100.0f, 40.0f}),
        ck_debug_graph_canvas_tests::MakeNode(2,
                                              FVector2D{300.0f, 160.0f},
                                              FVector2D{120.0f, 80.0f})};
    const auto Viewport = FVector2D{500.0f, 300.0f};
    const auto Transform =
        SCkDebug_GraphCanvas::Compute_FitTransform(Nodes, Viewport, 20.0f, 0.25f, 3.0f);

    TestTrue(TEXT("Fit zoom stays within requested bounds"),
             Transform.Zoom >= 0.25f && Transform.Zoom <= 3.0f);
    for (const auto& Node : Nodes)
    {
        const auto ScreenMin = SCkDebug_GraphCanvas::World_To_Screen(Node.Position, Transform);
        const auto ScreenMax = SCkDebug_GraphCanvas::World_To_Screen(Node.Position + Node.Size,
                                                                     Transform);
        TestTrue(TEXT("Fit keeps node inside horizontal padding"),
                 ScreenMin.X >= 20.0f - KINDA_SMALL_NUMBER &&
                     ScreenMax.X <= 480.0f + KINDA_SMALL_NUMBER);
        TestTrue(TEXT("Fit keeps node inside vertical padding"),
                 ScreenMin.Y >= 20.0f - KINDA_SMALL_NUMBER &&
                     ScreenMax.Y <= 280.0f + KINDA_SMALL_NUMBER);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_EdgeAnchorsAndRoutes,
                                 "Ck.DebuggerCommon.GraphCanvas.EdgeAnchorsAndRoutes",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_EdgeAnchorsAndRoutes::RunTest(const FString&)
{
    const auto Source =
        ck_debug_graph_canvas_tests::MakeNode(1, FVector2D{0.0f, 0.0f}, FVector2D{100.0f, 40.0f});
    const auto Target =
        ck_debug_graph_canvas_tests::MakeNode(2, FVector2D{240.0f, 0.0f}, FVector2D{100.0f, 40.0f});
    auto Edge = FCkDebug_GraphCanvasEdge{};
    Edge.SourceId = Source.Id;
    Edge.TargetId = Target.Id;
    Edge.SourceAnchor = ECkDebug_GraphAnchor::Right;
    Edge.TargetAnchor = ECkDebug_GraphAnchor::Left;
    const auto Direct = SCkDebug_GraphCanvas::Compute_EdgeGeometry(Source,
                                                                   Target,
                                                                   Edge,
                                                                   FCkDebug_GraphCanvasTransform{});
    TestEqual(TEXT("Explicit right anchor starts at source right"), Direct.Start.X, 100.0);
    TestEqual(TEXT("Explicit left anchor ends at target left"), Direct.End.X, 240.0);

    Edge.LineSeparation = 4.5f;
    Edge.ArrowRadius = 8.0f;
    const auto Biased = SCkDebug_GraphCanvas::Compute_EdgeGeometry(
        Source, Target, Edge, FCkDebug_GraphCanvasTransform{});
    TestEqual(TEXT("Legacy line bias shortens the source by arrow radius"), Biased.Start.X, 108.0);
    TestEqual(TEXT("Legacy line bias shortens the target by arrow radius"), Biased.End.X, 232.0);
    TestEqual(TEXT("Legacy line bias separates reciprocal wires"), Biased.Start.Y, 15.5);

    Edge.LineSeparation = 0.0f;
    Edge.RoutePoints = {FVector2D{160.0f, 80.0f}};
    const auto Routed = SCkDebug_GraphCanvas::Compute_EdgeGeometry(Source,
                                                                   Target,
                                                                   Edge,
                                                                   FCkDebug_GraphCanvasTransform{});
    TestEqual(TEXT("Routed edge keeps route point"), Routed.Points.Num(), 3);
    TestEqual(TEXT("Routed point is preserved"), Routed.Points[1], FVector2D{160.0f, 80.0f});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugGraphCanvas_SceneReconciliationPreservesSelection,
    "Ck.DebuggerCommon.GraphCanvas.SceneReconciliationPreservesSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_SceneReconciliationPreservesSelection::RunTest(const FString&)
{
    auto CallbackCount = 0;
    auto LastSelection = TSet<uint64>{};
    const auto Canvas = SNew(SCkDebug_GraphCanvas)
                            .OnSelectionChanged_Lambda(
                                [&CallbackCount, &LastSelection](const TSet<uint64>& InSelection)
                                {
                                    ++CallbackCount;
                                    LastSelection = InSelection;
                                });

    auto Scene = FCkDebug_GraphCanvasScene{};
    auto Node = FCkDebug_GraphCanvasNode{};
    Node.Id = 7;
    Scene.Nodes.Add(Node);
    Canvas->Set_Scene(Scene);
    Canvas->Set_SelectedNodeIds({7});
    Canvas->Set_Scene(Scene);

    TestEqual(TEXT("Stable child slots do not re-emit selection"), CallbackCount, 2);
    TestTrue(TEXT("Stable selected ID survives the scene replacement"), LastSelection.Contains(7));

    Scene.Nodes[0].Widget = SNew(STextBlock).Text(FText::FromString(TEXT("Replacement")));
    Canvas->Set_Scene(MoveTemp(Scene));
    TestEqual(TEXT("Replacing a card re-emits selection for the new widget"), CallbackCount, 3);
    TestTrue(TEXT("Selected ID survives card replacement"), LastSelection.Contains(7));
    Canvas->Clear_InteractionDelegates();
    return true;
}

#endif
