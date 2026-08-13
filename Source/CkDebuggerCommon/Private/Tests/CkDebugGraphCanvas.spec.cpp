#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_ClipsToViewport,
                                 "Ck.DebuggerCommon.GraphCanvas.ClipsToViewport",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_ClipsToViewport::RunTest(const FString&)
{
    const auto Canvas = SNew(SCkDebug_GraphCanvas);

    TestEqual(TEXT("Graph canvas clips custom paint and child cards to its viewport"),
              Canvas->GetClipping(),
              EWidgetClipping::ClipToBounds);
    return true;
}

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

    const auto HalfZoomBiased = SCkDebug_GraphCanvas::Compute_EdgeGeometry(
        Source, Target, Edge, FCkDebug_GraphCanvasTransform{FVector2D::ZeroVector, 0.5f});
    TestEqual(TEXT("Arrow-radius shortening follows graph zoom"), HalfZoomBiased.End.X, 116.0);
    TestEqual(TEXT("Reciprocal-wire separation follows graph zoom"),
              HalfZoomBiased.End.Y,
              7.75);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_ArrowGeometry,
                                 "Ck.DebuggerCommon.GraphCanvas.ArrowGeometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_ArrowGeometry::RunTest(const FString&)
{
    auto Edge = FCkDebug_GraphCanvasEdgeGeometry{};
    Edge.Points = {FVector2D{20.0f, 40.0f}, FVector2D{100.0f, 40.0f}};

    for (const auto Zoom : {0.5f, 1.0f, 2.0f})
    {
        const auto Arrow = SCkDebug_GraphCanvas::Compute_ArrowGeometry(
            Edge, FVector2D{16.0f, 16.0f}, Zoom);
        TestEqual(TEXT("Arrow brush receives zoom exactly once"),
                  Arrow.Size,
                  FVector2D{16.0f, 16.0f} * Zoom);
        TestEqual(TEXT("Arrow remains centered on the shaft endpoint"),
                  Arrow.Position + Arrow.Size * 0.5f,
                  Edge.Points.Last());
        TestTrue(TEXT("Horizontal arrow points along the shaft"),
                 FMath::IsNearlyZero(Arrow.AngleInRadians));
    }

    Edge.Points[0] = FVector2D{90.0f, 30.0f};
    const auto Diagonal = SCkDebug_GraphCanvas::Compute_ArrowGeometry(
        Edge, FVector2D{16.0f, 16.0f}, 1.0f);
    TestTrue(TEXT("Diagonal arrow follows the final segment"),
             FMath::IsNearlyEqual(Diagonal.AngleInRadians, PI * 0.25f));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_ManualPositionFlagIsPreserved,
                                 "Ck.DebuggerCommon.GraphCanvas.ManualPositionFlagIsPreserved",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_ManualPositionFlagIsPreserved::RunTest(const FString&)
{
    const auto Canvas = SNew(SCkDebug_GraphCanvas);
    auto Scene = FCkDebug_GraphCanvasScene{};
    auto Node = FCkDebug_GraphCanvasNode{};
    Node.Id = 17;
    Node.Position = FVector2D{120.0f, 80.0f};
    Node.Size = FVector2D{100.0f, 60.0f};
    Node.bHasManualPosition = true;
    Scene.Nodes.Add(Node);

    Canvas->Set_Scene(MoveTemp(Scene));

    TestEqual(TEXT("Scene preserves the node"), Canvas->Get_Scene().Nodes.Num(), 1);
    TestTrue(TEXT("Scene preserves manual-position state"),
             Canvas->Get_Scene().Nodes[0].bHasManualPosition);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugGraphCanvas_InFlightDragSurvivesSceneRefresh,
    "Ck.DebuggerCommon.GraphCanvas.InFlightDragSurvivesSceneRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_InFlightDragSurvivesSceneRefresh::RunTest(const FString&)
{
    auto CommittedPositions = TMap<uint64, FVector2D>{};
    const auto Canvas = SNew(SCkDebug_GraphCanvas)
                            .AllowNodeDragging(true)
                            .OnNodeMoved_Lambda(
                                [&CommittedPositions](const uint64 InId, const FVector2D& InPosition)
                                {
                                    CommittedPositions.Add(InId, InPosition);
                                });
    auto Scene = FCkDebug_GraphCanvasScene{};
    auto DraggedNode = FCkDebug_GraphCanvasNode{};
    DraggedNode.Id = 1;
    DraggedNode.Position = FVector2D{100.0f, 100.0f};
    DraggedNode.Size = FVector2D{100.0f, 60.0f};
    Scene.Nodes.Add(DraggedNode);
    auto OtherNode = FCkDebug_GraphCanvasNode{};
    OtherNode.Id = 2;
    OtherNode.Position = FVector2D{300.0f, 100.0f};
    OtherNode.Size = FVector2D{100.0f, 60.0f};
    Scene.Nodes.Add(OtherNode);
    Canvas->Set_Scene(Scene);

    const auto Geometry = FGeometry::MakeRoot(FVector2D{800.0f, 600.0f}, FSlateLayoutTransform{});
    auto PressedButtons = TSet<FKey>{};
    PressedButtons.Add(EKeys::LeftMouseButton);
    const auto MakePointerEvent = [&PressedButtons](const FVector2D& InPosition,
                                                     const FVector2D& InPreviousPosition)
    {
        return FPointerEvent(0,
                             InPosition,
                             InPreviousPosition,
                             PressedButtons,
                             EKeys::LeftMouseButton,
                             0.0f,
                             FModifierKeysState{});
    };

    const auto PointerDown = MakePointerEvent(FVector2D{110.0f, 110.0f}, FVector2D{110.0f, 110.0f});
    Canvas->OnPreviewMouseButtonDown(Geometry, PointerDown);
    Canvas->OnMouseButtonDown(Geometry, PointerDown);
    Canvas->OnMouseMove(Geometry,
                        MakePointerEvent(FVector2D{180.0f, 110.0f}, FVector2D{110.0f, 110.0f}));

    auto RefreshedScene = Scene;
    RefreshedScene.Nodes[1].Position = FVector2D{360.0f, 100.0f};
    Canvas->Set_Scene(MoveTemp(RefreshedScene));

    TestEqual(TEXT("Refresh retains the transient dragged position"),
              Canvas->Get_Scene().Nodes[0].Position,
              FVector2D{176.0f, 96.0f});
    TestEqual(TEXT("Refresh still accepts unrelated incoming node data"),
              Canvas->Get_Scene().Nodes[1].Position,
              FVector2D{360.0f, 100.0f});

    Canvas->OnMouseButtonUp(Geometry,
                            MakePointerEvent(FVector2D{180.0f, 110.0f}, FVector2D{180.0f, 110.0f}));
    TestTrue(TEXT("Drag commits after an intervening scene refresh"), CommittedPositions.Contains(1));
    if (const auto* CommittedPosition = CommittedPositions.Find(1))
    {
        TestEqual(TEXT("Committed drag position matches the preserved transient position"),
                  *CommittedPosition,
                  FVector2D{176.0f, 96.0f});
    }
    Canvas->Clear_InteractionDelegates();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugGraphCanvas_DragGroupMovesMembersAtomically,
    "Ck.DebuggerCommon.GraphCanvas.DragGroupMovesMembersAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugGraphCanvas_DragGroupMovesMembersAtomically::RunTest(const FString&)
{
    auto CommittedIds = TSet<uint64>{};
    const auto Canvas = SNew(SCkDebug_GraphCanvas)
                            .AllowNodeDragging(true)
                            .OnResolveDragGroup_Lambda([](const uint64 InId)
                            {
                                return InId == 1 ? TSet<uint64>{1, 2} : TSet<uint64>{};
                            })
                            .OnNodeMoved_Lambda(
                                [&CommittedIds](const uint64 InId, const FVector2D&)
                                {
                                    CommittedIds.Add(InId);
                                });
    auto Scene = FCkDebug_GraphCanvasScene{};
    for (const auto [Id, Position] : TArray<TPair<uint64, FVector2D>>{
             {1, FVector2D{100.0f, 100.0f}},
             {2, FVector2D{260.0f, 100.0f}},
             {3, FVector2D{420.0f, 100.0f}}})
    {
        auto Node = FCkDebug_GraphCanvasNode{};
        Node.Id = Id;
        Node.Position = Position;
        Node.Size = FVector2D{100.0f, 60.0f};
        Scene.Nodes.Add(MoveTemp(Node));
    }
    Canvas->Set_Scene(MoveTemp(Scene));

    const auto Geometry = FGeometry::MakeRoot(FVector2D{800.0f, 600.0f}, FSlateLayoutTransform{});
    auto PressedButtons = TSet<FKey>{EKeys::LeftMouseButton};
    const auto MakePointerEvent = [&PressedButtons](const FVector2D& InPosition,
                                                     const FVector2D& InPreviousPosition)
    {
        return FPointerEvent(0,
                             InPosition,
                             InPreviousPosition,
                             PressedButtons,
                             EKeys::LeftMouseButton,
                             0.0f,
                             FModifierKeysState{});
    };
    const auto PointerDown = MakePointerEvent(FVector2D{110.0f, 110.0f},
                                               FVector2D{110.0f, 110.0f});
    Canvas->OnPreviewMouseButtonDown(Geometry, PointerDown);
    Canvas->OnMouseButtonDown(Geometry, PointerDown);
    Canvas->OnMouseMove(Geometry,
                        MakePointerEvent(FVector2D{180.0f, 110.0f},
                                         FVector2D{110.0f, 110.0f}));

    TestEqual(TEXT("Group root follows the snapped drag"),
              Canvas->Get_Scene().Nodes[0].Position,
              FVector2D{176.0f, 96.0f});
    TestEqual(TEXT("Resolved group member receives the identical delta"),
              Canvas->Get_Scene().Nodes[1].Position,
              FVector2D{336.0f, 96.0f});
    TestEqual(TEXT("Unrelated node remains fixed"),
              Canvas->Get_Scene().Nodes[2].Position,
              FVector2D{420.0f, 100.0f});

    Canvas->OnMouseButtonUp(Geometry,
                            MakePointerEvent(FVector2D{180.0f, 110.0f},
                                             FVector2D{180.0f, 110.0f}));
    TestTrue(TEXT("Group root owns the persisted move"), CommittedIds.Contains(1));
    TestFalse(TEXT("Derived group member does not emit a duplicate commit"),
              CommittedIds.Contains(2));
    Canvas->Clear_InteractionDelegates();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugGraphCanvas_SelectedRootDragGroupTest,
                                 "Ck.DebuggerCommon.GraphCanvas.SelectedRootDragGroup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCkDebugGraphCanvas_SelectedRootDragGroupTest::RunTest(const FString&)
{
    const auto Canvas = SNew(SCkDebug_GraphCanvas)
                            .AllowNodeDragging(true)
                            .OnResolveDragGroup_Lambda([](const uint64 InId)
                            { return InId == 1 ? TSet<uint64>{1, 2} : TSet<uint64>{}; });
    auto Scene = FCkDebug_GraphCanvasScene{};
    const auto Nodes = TArray<TPair<uint64, FVector2D>>{
        TPair<uint64, FVector2D>{1, FVector2D{100.0f, 100.0f}},
        TPair<uint64, FVector2D>{2, FVector2D{260.0f, 100.0f}},
        TPair<uint64, FVector2D>{3, FVector2D{420.0f, 100.0f}}};
    for (const auto& [Id, Position] : Nodes)
    {
        auto Node = FCkDebug_GraphCanvasNode{};
        Node.Id = Id;
        Node.Position = Position;
        Node.Size = FVector2D{100, 60};
        Scene.Nodes.Add(MoveTemp(Node));
    }
    Canvas->Set_Scene(MoveTemp(Scene));
    Canvas->Set_SelectedNodeIds(TSet<uint64>{1}, false);
    const auto Geometry = FGeometry::MakeRoot(FVector2D{800.0f, 600.0f}, FSlateLayoutTransform{});
    auto Buttons = TSet<FKey>{EKeys::LeftMouseButton};
    const auto Event = [&Buttons](const FVector2D Position, const FVector2D Previous)
    {
        return FPointerEvent(0,
                             Position,
                             Previous,
                             Buttons,
                             EKeys::LeftMouseButton,
                             0.0f,
                             FModifierKeysState{false,
                                                false,
                                                true,
                                                false,
                                                false,
                                                false,
                                                false,
                                                false,
                                                false});
    };
    Canvas->OnPreviewMouseButtonDown(
        Geometry, Event(FVector2D{430.0f, 110.0f}, FVector2D{430.0f, 110.0f}));
    Canvas->OnMouseButtonDown(
        Geometry, Event(FVector2D{430.0f, 110.0f}, FVector2D{430.0f, 110.0f}));
    Canvas->OnMouseMove(
        Geometry, Event(FVector2D{500.0f, 110.0f}, FVector2D{430.0f, 110.0f}));
    TestEqual(TEXT("selected root closure moves even when its child was not hit"),
              Canvas->Get_Scene().Nodes[1].Position, FVector2D{336.0f, 96.0f});
    Canvas->OnMouseButtonUp(
        Geometry, Event(FVector2D{500.0f, 110.0f}, FVector2D{500.0f, 110.0f}));
    Canvas->Clear_InteractionDelegates();
    return true;
}

#endif
