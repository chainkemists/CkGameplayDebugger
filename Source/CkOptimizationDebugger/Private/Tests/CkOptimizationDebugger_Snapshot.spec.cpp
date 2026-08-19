#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotCodec.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotLens.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotReport.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_snapshot_spec
{
    auto
        Make_Prim(
            const FString& InName,
            int32 InTriangles,
            int32 InSections,
            int32 InInstances = 1)
        -> FCkOptimizationDebugger_SnapshotPrim
    {
        auto Prim = FCkOptimizationDebugger_SnapshotPrim{};

        Prim.DisplayName = InName;
        Prim.MeshDisplayName = InName;
        Prim.InstanceCount = InInstances;

        auto Lod = FCkOptimizationDebugger_SnapshotLod{};
        Lod.Triangles = InTriangles;
        Lod.Sections = InSections;

        Prim.Lods.Add(Lod);

        return Prim;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Snapshot(
            int32 InPrimCount)
        -> FCkOptimizationDebugger_Snapshot
    {
        auto Snapshot = FCkOptimizationDebugger_Snapshot{};

        Snapshot.Width = 4;
        Snapshot.Height = 4;

        for (auto Index = 0; Index < InPrimCount; ++Index)
        {
            Snapshot.Prims.Add(Make_Prim(ck::Format_UE(TEXT("SM_Spec_{}"), Index), 100 * (Index + 1), Index + 1));
        }

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_RleRoundTrip,
    "Ck.OptimizationDebugger.Snapshot.RleRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_RleRoundTrip::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;

    // The codec is the whole reason a snapshot can be clicked into after the world moved on. If it round-trips
    // anything but exactly, every later pixel names the wrong mesh — quietly, because a wrong id is still an id.
    const auto RoundTrips = [this](const FString& InCase, const TArray<uint32>& InIds) -> void
    {
        const auto Decoded = Decode_IdMapRle(Encode_IdMapRle(InIds));

        if (NOT TestEqual(ck::Format_UE(TEXT("{}: the decoded length matches"), InCase), Decoded.Num(), InIds.Num()))
        { return; }

        for (auto Index = 0; Index < InIds.Num(); ++Index)
        {
            if (Decoded[Index] == InIds[Index])
            { continue; }

            AddError(ck::Format_UE(TEXT("{}: pixel {} decoded as {} rather than {}"),
                InCase, Index, Decoded[Index], InIds[Index]));

            return;
        }

        TestTrue(ck::Format_UE(TEXT("{}: every pixel round-trips"), InCase), true);
    };

    RoundTrips(TEXT("Empty"), TArray<uint32>{});

    // The best case for a run-length codec and the one a real capture is mostly made of: large flat regions.
    auto AllSame = TArray<uint32>{};
    AllSame.Init(7, 1'000'000);
    RoundTrips(TEXT("OneMillionOfOne"), AllSame);

    // And the worst case, where every run is length one — the encoding grows, and it still has to be exact.
    auto Alternating = TArray<uint32>{};

    for (auto Index = 0; Index < 1024; ++Index)
    { Alternating.Add(Index % 2 == 0 ? 3u : 9u); }

    RoundTrips(TEXT("Alternating"), Alternating);

    // The sentinel is a VALUE, not an absence: sky is most of a typical capture, and a codec that dropped it would
    // shift every later pixel by the length of the run it skipped.
    RoundTrips(TEXT("WithSentinel"), TArray<uint32>{k_NoPrim, k_NoPrim, 0u, 1u, k_NoPrim, 1u});

    // A truncated buffer decodes to NOTHING rather than to a partial map. Half an ID map misnames every pixel past
    // the damage, and the caller cannot tell that happened.
    auto Truncated = Encode_IdMapRle(TArray<uint32>{4u, 4u, 5u});
    Truncated.RemoveAt(Truncated.Num() - 1);

    TestEqual(TEXT("A truncated buffer decodes to nothing, never to a partial map"),
        Decode_IdMapRle(Truncated).Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_IdAtBounds,
    "Ck.OptimizationDebugger.Snapshot.IdAtBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_IdAtBounds::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;

    constexpr auto Width = 3;
    constexpr auto Height = 2;

    const auto Ids = TArray<uint32>{
        0u, 1u, 2u,
        3u, k_NoPrim, 5u};

    const auto IdAt = [&](int32 InX, int32 InY) -> TOptional<int32>
    {
        return Get_IdAt(Ids, Width, Height, FIntPoint{InX, InY});
    };

    TestEqual(TEXT("The first pixel reads its own id"), IdAt(0, 0).Get(INDEX_NONE), 0);
    TestEqual(TEXT("An interior pixel reads its own id"), IdAt(2, 0).Get(INDEX_NONE), 2);
    TestEqual(TEXT("The last pixel reads its own id"), IdAt(Width - 1, Height - 1).Get(INDEX_NONE), 5);

    // The sentinel and being outside the image are the same answer to the caller — "you clicked nothing" — and it
    // matters that neither one comes back as an index.
    TestFalse(TEXT("The sentinel reads as no prim"), IdAt(1, 1).IsSet());
    TestFalse(TEXT("A negative x is outside"), IdAt(-1, 0).IsSet());
    TestFalse(TEXT("A negative y is outside"), IdAt(0, -1).IsSet());
    TestFalse(TEXT("One past the right edge is outside"), IdAt(Width, 0).IsSet());
    TestFalse(TEXT("One past the bottom edge is outside"), IdAt(0, Height).IsSet());

    // A map shorter than the dimensions claim is a damaged one; reading it must not run off the end.
    TestFalse(TEXT("A map shorter than its dimensions reads as no prim rather than out of bounds"),
        Get_IdAt(TArray<uint32>{0u}, Width, Height, FIntPoint{2, 1}).IsSet());

    TestFalse(TEXT("A zero-sized image has no pixels"), Get_IdAt(Ids, 0, 0, FIntPoint{0, 0}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_ViewerPointMapping,
    "Ck.OptimizationDebugger.Snapshot.ViewerPointMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_ViewerPointMapping::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;

    // The image is drawn aspect-fit, so part of the widget is empty margin. A click there must select nothing —
    // otherwise clicking beside the picture picks whatever mesh happens to touch the nearest edge.

    // A 200x100 image in a 200x200 box: bands top and bottom, 50px each.
    const auto WideImage = FIntPoint{200, 100};
    const auto SquareBox = FVector2D{200.0, 200.0};

    TestFalse(TEXT("A point in the top letterbox band maps to nothing"),
        Map_ViewerPointToPixel(SquareBox, WideImage, FVector2D{100.0, 10.0}).IsSet());
    TestFalse(TEXT("A point in the bottom letterbox band maps to nothing"),
        Map_ViewerPointToPixel(SquareBox, WideImage, FVector2D{100.0, 190.0}).IsSet());

    const auto Center = Map_ViewerPointToPixel(SquareBox, WideImage, FVector2D{100.0, 100.0});

    if (TestTrue(TEXT("The centre of the box is inside the image"), Center.IsSet()))
    {
        TestEqual(TEXT("...and maps to the centre pixel"), Center.GetValue(), FIntPoint{100, 50});
    }

    // A 100x200 image in the same box: bands left and right instead.
    const auto TallImage = FIntPoint{100, 200};

    TestFalse(TEXT("A point in the left pillarbox band maps to nothing"),
        Map_ViewerPointToPixel(SquareBox, TallImage, FVector2D{10.0, 100.0}).IsSet());
    TestFalse(TEXT("A point in the right pillarbox band maps to nothing"),
        Map_ViewerPointToPixel(SquareBox, TallImage, FVector2D{190.0, 100.0}).IsSet());

    // Exact fit: the corners are the extreme pixels, which is where an off-by-one would show up as a click on the
    // far edge indexing one past the map.
    const auto ExactBox = FVector2D{200.0, 100.0};

    const auto TopLeft = Map_ViewerPointToPixel(ExactBox, WideImage, FVector2D{0.0, 0.0});

    if (TestTrue(TEXT("The top-left corner is inside"), TopLeft.IsSet()))
    { TestEqual(TEXT("...and is the first pixel"), TopLeft.GetValue(), FIntPoint{0, 0}); }

    const auto BottomRight = Map_ViewerPointToPixel(ExactBox, WideImage, FVector2D{199.999, 99.999});

    if (TestTrue(TEXT("The bottom-right corner is inside"), BottomRight.IsSet()))
    { TestEqual(TEXT("...and is the last pixel"), BottomRight.GetValue(), FIntPoint{199, 99}); }

    TestFalse(TEXT("A point past the right edge maps to nothing"),
        Map_ViewerPointToPixel(ExactBox, WideImage, FVector2D{200.0, 50.0}).IsSet());

    // A widget that has not been laid out yet has zero size, and a snapshot with no image has zero dimensions.
    TestFalse(TEXT("A zero-sized widget maps nothing"),
        Map_ViewerPointToPixel(FVector2D{0.0, 0.0}, WideImage, FVector2D{0.0, 0.0}).IsSet());
    TestFalse(TEXT("A zero-sized image maps nothing"),
        Map_ViewerPointToPixel(SquareBox, FIntPoint{0, 0}, FVector2D{100.0, 100.0}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_ClickSemantics,
    "Ck.OptimizationDebugger.Snapshot.ClickSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_ClickSemantics::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Snapshot = Make_Snapshot(4);

    // Plain click replaces.
    Apply_SnapshotClick(Snapshot, 1, ECkOptimizationDebugger_SnapshotClickModifier::None);
    TestEqual(TEXT("A plain click selects exactly the clicked prim"), Snapshot.SelectedPrims.Num(), 1);
    TestTrue(TEXT("...which is the one clicked"), Snapshot.SelectedPrims.Contains(1));

    Apply_SnapshotClick(Snapshot, 2, ECkOptimizationDebugger_SnapshotClickModifier::None);
    TestEqual(TEXT("A second plain click replaces rather than adds"), Snapshot.SelectedPrims.Num(), 1);
    TestTrue(TEXT("...with the new one"), Snapshot.SelectedPrims.Contains(2));

    // Shift adds.
    Apply_SnapshotClick(Snapshot, 3, ECkOptimizationDebugger_SnapshotClickModifier::Shift);
    TestEqual(TEXT("Shift adds to the selection"), Snapshot.SelectedPrims.Num(), 2);
    TestTrue(TEXT("...keeping what was there"), Snapshot.SelectedPrims.Contains(2));

    // Shift on something already selected is not a toggle — that is Ctrl's job, and a reader dragging a shift
    // selection across a prim twice must not deselect it.
    Apply_SnapshotClick(Snapshot, 3, ECkOptimizationDebugger_SnapshotClickModifier::Shift);
    TestEqual(TEXT("Shift on an already-selected prim leaves it selected"), Snapshot.SelectedPrims.Num(), 2);

    // Ctrl removes.
    Apply_SnapshotClick(Snapshot, 2, ECkOptimizationDebugger_SnapshotClickModifier::Ctrl);
    TestEqual(TEXT("Ctrl removes from the selection"), Snapshot.SelectedPrims.Num(), 1);
    TestTrue(TEXT("...leaving the rest"), Snapshot.SelectedPrims.Contains(3));

    Apply_SnapshotClick(Snapshot, 0, ECkOptimizationDebugger_SnapshotClickModifier::Ctrl);
    TestEqual(TEXT("Ctrl on an unselected prim changes nothing"), Snapshot.SelectedPrims.Num(), 1);

    // A modified click that hit nothing must not throw away the selection it was extending.
    Apply_SnapshotClick(Snapshot, {}, ECkOptimizationDebugger_SnapshotClickModifier::Shift);
    TestEqual(TEXT("Shift on an empty pixel leaves the selection alone"), Snapshot.SelectedPrims.Num(), 1);

    Apply_SnapshotClick(Snapshot, {}, ECkOptimizationDebugger_SnapshotClickModifier::Ctrl);
    TestEqual(TEXT("Ctrl on an empty pixel leaves the selection alone"), Snapshot.SelectedPrims.Num(), 1);

    // A plain click on nothing is how the reader clears.
    Apply_SnapshotClick(Snapshot, {}, ECkOptimizationDebugger_SnapshotClickModifier::None);
    TestEqual(TEXT("A plain click on an empty pixel clears the selection"), Snapshot.SelectedPrims.Num(), 0);

    // An index no prim table backs is dropped rather than stored: a decoded map and a prim table can disagree, and
    // this is the one place every click passes through.
    Apply_SnapshotClick(Snapshot, 99, ECkOptimizationDebugger_SnapshotClickModifier::None);
    TestEqual(TEXT("Clicking an index past the prim table selects nothing"), Snapshot.SelectedPrims.Num(), 0);

    Snapshot.SelectedPrims.Add(42);
    Apply_SnapshotClick(Snapshot, 0, ECkOptimizationDebugger_SnapshotClickModifier::Shift);
    TestFalse(TEXT("A stale index already in the set is dropped on the next click"),
        Snapshot.SelectedPrims.Contains(42));
    TestTrue(TEXT("...while the click itself still lands"), Snapshot.SelectedPrims.Contains(0));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_SelectionSet,
    "Ck.OptimizationDebugger.Snapshot.SelectionSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_SelectionSet::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Snapshot = Make_Snapshot(3);

    Apply_SnapshotSelection(Snapshot, TArray<int32>{2, 0});

    TestEqual(TEXT("A set selection replaces what was there"), Snapshot.SelectedPrims.Num(), 2);
    TestTrue(TEXT("...with exactly its members"),
        Snapshot.SelectedPrims.Contains(0) && Snapshot.SelectedPrims.Contains(2));

    // The mesh list can hand over a row that outlived the snapshot it was built from; the same distrust every click
    // applies has to apply here, or a stale index becomes a selection nothing can explain.
    Apply_SnapshotSelection(Snapshot, TArray<int32>{1, 99, -4});

    TestEqual(TEXT("Indices the table cannot back are dropped"), Snapshot.SelectedPrims.Num(), 1);
    TestTrue(TEXT("...leaving the valid one"), Snapshot.SelectedPrims.Contains(1));

    Apply_SnapshotSelection(Snapshot, TArray<int32>{});

    TestEqual(TEXT("An empty selection clears"), Snapshot.SelectedPrims.Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_StorageAndCycling,
    "Ck.OptimizationDebugger.Snapshot.StorageAndCycling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_StorageAndCycling::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Model = FCkOptimizationDebugger_Model{};

    const auto AddLabelled = [&Model](const FString& InLabel, int32 InMaxStored) -> void
    {
        auto Snapshot = Make_Snapshot(2);
        Snapshot.Label = InLabel;

        Model.Add_Snapshot(MoveTemp(Snapshot), InMaxStored);
    };

    constexpr auto MaxStored = 3;

    AddLabelled(TEXT("A"), MaxStored);
    AddLabelled(TEXT("B"), MaxStored);
    AddLabelled(TEXT("C"), MaxStored);

    TestEqual(TEXT("Three stored under a cap of three"), Model.Get_Snapshots().Num(), 3);
    TestEqual(TEXT("The newest is active"), Model.Get_ActiveSnapshotIndex(), 2);

    // Past the cap the OLDEST goes: the reader asked for "the last N I took", and evicting the newest would throw
    // away the one they are looking at.
    AddLabelled(TEXT("D"), MaxStored);

    TestEqual(TEXT("The cap holds"), Model.Get_Snapshots().Num(), 3);
    TestEqual(TEXT("The oldest was evicted"), Model.Get_Snapshots()[0].Label, FString{TEXT("B")});
    TestEqual(TEXT("The newest is still active"), Model.Get_ActiveSnapshotIndex(), 2);

    // A cap of zero would evict the capture the reader just took, which reads as a broken button.
    AddLabelled(TEXT("E"), 0);
    TestEqual(TEXT("A cap below one still keeps the capture just taken"), Model.Get_Snapshots().Num(), 1);
    TestEqual(TEXT("...and it is the one just taken"), Model.Get_Snapshots()[0].Label, FString{TEXT("E")});

    // Cycling wraps both ways.
    auto Cycling = FCkOptimizationDebugger_Model{};

    for (const auto* Label : {TEXT("A"), TEXT("B"), TEXT("C")})
    {
        auto Snapshot = Make_Snapshot(1);
        Snapshot.Label = FString{Label};
        Cycling.Add_Snapshot(MoveTemp(Snapshot), MaxStored);
    }

    TestEqual(TEXT("Cycling starts at the newest"), Cycling.Get_ActiveSnapshotIndex(), 2);

    Cycling.Cycle_ActiveSnapshot(1);
    TestEqual(TEXT("Cycling forward off the end wraps to the first"), Cycling.Get_ActiveSnapshotIndex(), 0);

    Cycling.Cycle_ActiveSnapshot(-1);
    TestEqual(TEXT("Cycling back off the front wraps to the last"), Cycling.Get_ActiveSnapshotIndex(), 2);

    Cycling.Cycle_ActiveSnapshot(-1);
    TestEqual(TEXT("Cycling back moves one"), Cycling.Get_ActiveSnapshotIndex(), 1);

    // Selection is per snapshot, so cycling away and back has to bring it with it — the reason it lives on the
    // snapshot rather than on the page.
    auto* Middle = Cycling.TryGet_MutableActiveSnapshot();

    if (TestNotNull(TEXT("The active snapshot is reachable"), Middle))
    {
        Middle->SelectedPrims.Add(0);

        Cycling.Cycle_ActiveSnapshot(1);
        Cycling.Cycle_ActiveSnapshot(-1);

        const auto* Returned = Cycling.TryGet_ActiveSnapshot();

        if (TestNotNull(TEXT("...and still reachable after cycling away and back"), Returned))
        {
            TestTrue(TEXT("The selection made on it survived the round trip"), Returned->SelectedPrims.Contains(0));
        }
    }

    // Removing clamps rather than clearing, so the reader is left looking at the one that took its place.
    Cycling.Set_ActiveSnapshotIndex(2);
    Cycling.Remove_ActiveSnapshot();

    TestEqual(TEXT("Removing the last leaves two"), Cycling.Get_Snapshots().Num(), 2);
    TestEqual(TEXT("...with the active index clamped into range"), Cycling.Get_ActiveSnapshotIndex(), 1);

    Cycling.Remove_ActiveSnapshot();
    Cycling.Remove_ActiveSnapshot();

    TestEqual(TEXT("Removing them all empties the list"), Cycling.Get_Snapshots().Num(), 0);
    TestEqual(TEXT("...and leaves no active index"), Cycling.Get_ActiveSnapshotIndex(), INDEX_NONE);
    TestNull(TEXT("...and nothing to read"), Cycling.TryGet_ActiveSnapshot());

    Cycling.Remove_ActiveSnapshot();
    Cycling.Cycle_ActiveSnapshot(1);
    TestEqual(TEXT("Removing and cycling an empty list is a no-op"), Cycling.Get_ActiveSnapshotIndex(), INDEX_NONE);

    // A PIE boundary invalidates answers about the world. A snapshot is not an answer about the world NOW — it is a
    // picture of a moment that has already passed, which is exactly what makes it worth keeping across one.
    auto Surviving = FCkOptimizationDebugger_Model{};
    auto Kept = Make_Snapshot(1);
    Kept.Label = FString{TEXT("Kept")};
    Surviving.Add_Snapshot(MoveTemp(Kept), MaxStored);

    Surviving.Reset();

    TestEqual(TEXT("Reset keeps the snapshots"), Surviving.Get_Snapshots().Num(), 1);
    TestEqual(TEXT("...and which one was active"), Surviving.Get_ActiveSnapshotIndex(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_SelectionTotals,
    "Ck.OptimizationDebugger.Snapshot.SelectionTotals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_SelectionTotals::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Snapshot = FCkOptimizationDebugger_Snapshot{};
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_A"), 100, 2, 1));
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_B"), 250, 3, 4));
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_C"), 999, 9, 9));

    const auto Empty = Get_SelectionTotals(Snapshot);
    TestEqual(TEXT("Nothing selected totals nothing"), Empty.PrimCount, 0);
    TestEqual(TEXT("...including triangles"), static_cast<int32>(Empty.Lod0Triangles), 0);

    Snapshot.SelectedPrims.Add(0);
    Snapshot.SelectedPrims.Add(1);

    const auto Totals = Get_SelectionTotals(Snapshot);

    // The unselected prim is the point: a total that counted it would answer a question the reader did not ask.
    TestEqual(TEXT("Only the selected prims are counted"), Totals.PrimCount, 2);
    TestEqual(TEXT("Triangles sum over LOD0"), static_cast<int32>(Totals.Lod0Triangles), 350);
    TestEqual(TEXT("Sections sum over LOD0"), Totals.Lod0Sections, 5);
    TestEqual(TEXT("Instances sum too"), Totals.InstanceCount, 5);

    // A stale index in the set must not read off the end of the table.
    Snapshot.SelectedPrims.Add(77);
    const auto WithStale = Get_SelectionTotals(Snapshot);
    TestEqual(TEXT("A stale index contributes nothing"), WithStale.PrimCount, 2);

    // A prim with no LODs is legal — a capture can fail to read one — and must not be a divide-by-nothing.
    auto NoLods = FCkOptimizationDebugger_Snapshot{};
    NoLods.Prims.Add(FCkOptimizationDebugger_SnapshotPrim{});
    NoLods.SelectedPrims.Add(0);

    const auto LodlessTotals = Get_SelectionTotals(NoLods);
    TestEqual(TEXT("A prim with no LODs still counts as a prim"), LodlessTotals.PrimCount, 1);
    TestEqual(TEXT("...contributing no triangles"), static_cast<int32>(LodlessTotals.Lod0Triangles), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_DrawCallText,
    "Ck.OptimizationDebugger.Snapshot.DrawCallText",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_DrawCallText::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_spec;

    const auto Text = Get_EstimatedDrawCallText(Make_Prim(TEXT("SM_A"), 100, 4));

    TestTrue(TEXT("The estimate names the number"), Text.Contains(TEXT("4")));

    // Both halves of the honesty are load-bearing: the approximation mark says it is not measured, and naming LOD0
    // sections says WHAT was counted. A bare number would read as a profiler reading.
    TestTrue(TEXT("...marks itself as an approximation"), Text.Contains(TEXT("≈")));
    TestTrue(TEXT("...and says what it counted"), Text.Contains(TEXT("LOD0 sections")));

    const auto Lodless = Get_EstimatedDrawCallText(FCkOptimizationDebugger_SnapshotPrim{});
    TestTrue(TEXT("A prim with no LODs estimates zero rather than nothing"), Lodless.Contains(TEXT("0")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_StencilBatching,
    "Ck.OptimizationDebugger.Snapshot.StencilBatching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_StencilBatching::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;

    // Stencil 0 is what every primitive OUTSIDE the current pass is set to, so it cannot also mean "the first
    // primitive" — which is the whole reason a pass carries 255 rather than 256.
    const auto First = Get_StencilSlot(0);
    TestEqual(TEXT("The first prim is in the first pass"), First.PassIndex, 0);
    TestEqual(TEXT("...with stencil one, never zero"), static_cast<int32>(First.StencilValue), 1);

    const auto LastOfFirstPass = Get_StencilSlot(k_StencilBatchSize - 1);
    TestEqual(TEXT("The 255th prim is still in the first pass"), LastOfFirstPass.PassIndex, 0);
    TestEqual(TEXT("...carrying the highest stencil value"),
        static_cast<int32>(LastOfFirstPass.StencilValue), 255);

    const auto FirstOfSecondPass = Get_StencilSlot(k_StencilBatchSize);
    TestEqual(TEXT("The 256th prim starts the second pass"), FirstOfSecondPass.PassIndex, 1);
    TestEqual(TEXT("...back at stencil one"), static_cast<int32>(FirstOfSecondPass.StencilValue), 1);

    // Every slot must round-trip through the resolve rule, or a pixel names the wrong mesh.
    for (const auto PrimIndex : {0, 1, 100, 254, 255, 256, 509, 510, 4095})
    {
        const auto Slot = Get_StencilSlot(PrimIndex);

        auto PerPass = TArray<uint8>{};
        PerPass.Init(0, Get_StencilPassCount(4096));
        PerPass[Slot.PassIndex] = Slot.StencilValue;

        auto Conflicts = 0;
        const auto Resolved = Resolve_PrimFromPassValues(PerPass, 4096, Conflicts);

        TestEqual(ck::Format_UE(TEXT("Prim {} round-trips through its slot"), PrimIndex),
            Resolved.Get(INDEX_NONE), PrimIndex);
    }

    TestEqual(TEXT("No prims need no passes"), Get_StencilPassCount(0), 0);
    TestEqual(TEXT("One prim needs one pass"), Get_StencilPassCount(1), 1);
    TestEqual(TEXT("A full batch is still one pass"), Get_StencilPassCount(255), 1);
    TestEqual(TEXT("One past a full batch needs two"), Get_StencilPassCount(256), 2);
    TestEqual(TEXT("The prim cap needs seventeen"), Get_StencilPassCount(4096), 17);

    // A negative count is not a capture with negative work in it.
    TestEqual(TEXT("A negative count needs no passes"), Get_StencilPassCount(-5), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_PassResolution,
    "Ck.OptimizationDebugger.Snapshot.PassResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_PassResolution::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;

    constexpr auto PrimCount = 300;

    auto Conflicts = 0;

    // The ordinary pixel: one pass saw something, the rest saw the batch's zero.
    const auto Single = Resolve_PrimFromPassValues(TArray<uint8>{0, 7}, PrimCount, Conflicts);
    TestEqual(TEXT("A single non-zero pass resolves to its prim"),
        Single.Get(INDEX_NONE), k_StencilBatchSize + 6);
    TestEqual(TEXT("...with nothing to disagree about"), Conflicts, 0);

    // Sky, an excluded primitive type, or a translucent material that writes no custom depth.
    Conflicts = 0;
    TestFalse(TEXT("All-zero passes resolve to no prim"),
        Resolve_PrimFromPassValues(TArray<uint8>{0, 0, 0}, PrimCount, Conflicts).IsSet());
    TestEqual(TEXT("...and that is not a conflict"), Conflicts, 0);

    // Two passes claiming one pixel cannot happen inside a single game-thread scope, so if it does the capture is
    // wrong in a way worth counting rather than worth crashing over.
    Conflicts = 0;
    const auto Conflicted = Resolve_PrimFromPassValues(TArray<uint8>{3, 9}, PrimCount, Conflicts);
    TestEqual(TEXT("Two claims resolve to the first"), Conflicted.Get(INDEX_NONE), 2);
    TestEqual(TEXT("...and the disagreement is counted"), Conflicts, 1);

    // The last pass is nearly always partial: 300 prims means pass 1 holds 45 of them, so a reading of 200 there is
    // a misread and naming a prim for it would name one the snapshot does not contain.
    Conflicts = 0;
    TestFalse(TEXT("A value past the end of a partial batch resolves to nothing"),
        Resolve_PrimFromPassValues(TArray<uint8>{0, 200}, PrimCount, Conflicts).IsSet());

    // Which also means it must not be counted as a conflict — nothing disagreed, one reading was simply not real.
    TestEqual(TEXT("...and is not counted as a conflict"), Conflicts, 0);

    Conflicts = 0;
    TestFalse(TEXT("No passes at all resolve to nothing"),
        Resolve_PrimFromPassValues(TArray<uint8>{}, PrimCount, Conflicts).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_Aggregates,
    "Ck.OptimizationDebugger.Snapshot.Aggregates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_Aggregates::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Prims = TArray<FCkOptimizationDebugger_SnapshotPrim>{};

    const auto Empty = Get_SnapshotAggregates(Prims);
    TestEqual(TEXT("No prims total nothing"), static_cast<int32>(Empty.TotalLod0Triangles), 0);
    TestEqual(TEXT("...and count as nothing"), Empty.StaticCount + Empty.InstancedCount + Empty.SkeletalCount, 0);

    Prims.Add(Make_Prim(TEXT("SM_A"), 100, 2, 1));

    auto Instanced = Make_Prim(TEXT("ISM_B"), 250, 3, 40);
    Instanced.Kind = ECkOptimizationDebugger_SnapshotPrimKind::InstancedStaticMesh;
    Instanced.IsNanite = true;
    Prims.Add(Instanced);

    auto Skeletal = Make_Prim(TEXT("SK_C"), 999, 5, 1);
    Skeletal.Kind = ECkOptimizationDebugger_SnapshotPrimKind::SkeletalMesh;
    Prims.Add(Skeletal);

    // A capture can fail to read a mesh's LODs; the prim still counts as a prim and contributes no triangles.
    auto Lodless = FCkOptimizationDebugger_SnapshotPrim{};
    Lodless.InstanceCount = 3;
    Prims.Add(Lodless);

    const auto Aggregates = Get_SnapshotAggregates(Prims);

    TestEqual(TEXT("Triangles sum over LOD0"), static_cast<int32>(Aggregates.TotalLod0Triangles), 1349);
    TestEqual(TEXT("Sections sum over LOD0"), Aggregates.TotalLod0Sections, 10);
    TestEqual(TEXT("Instances sum over every prim"), Aggregates.TotalInstances, 45);
    TestEqual(TEXT("Static prims counted"), Aggregates.StaticCount, 2);
    TestEqual(TEXT("Instanced prims counted"), Aggregates.InstancedCount, 1);
    TestEqual(TEXT("Skeletal prims counted"), Aggregates.SkeletalCount, 1);
    TestEqual(TEXT("Nanite prims counted"), Aggregates.NaniteCount, 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

// Every field a capture writes, populated with distinct values, so a codec or report that drops one shows up as a
// mismatch rather than as a lucky pass over defaults.
namespace ck_optimization_debugger_snapshot_spec
{
    auto
        Make_FullSnapshot()
        -> FCkOptimizationDebugger_Snapshot
    {
        using namespace ck_optimization_debugger_snapshot;

        auto Snapshot = FCkOptimizationDebugger_Snapshot{};

        Snapshot.Id = FGuid{0x11111111, 0x22222222, 0x33333333, 0x44444444};
        Snapshot.Label = FString{TEXT("Snapshot 3 — 14:52:10")};
        Snapshot.CapturedAt = FDateTime{2026, 8, 18, 14, 52, 10};
        Snapshot.WorldName = FString{TEXT("L_Spec & <World>")};
        Snapshot.Width = 4;
        Snapshot.Height = 2;
        Snapshot.ColorPng = TArray64<uint8>{{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x01, 0x02}};
        Snapshot.HasIdMap = true;
        Snapshot.IdMapRle = Encode_IdMapRle(TArray<uint32>{0u, 0u, 1u, k_NoPrim, 1u, 1u, 0u, k_NoPrim});
        Snapshot.UnidentifiedPixelCount = 2;
        Snapshot.CaptureNotes = FString{TEXT("3 primitive(s) excluded")};
        Snapshot.UniqueMaterialCount = 2;
        Snapshot.UniqueTextureCount = 5;
        Snapshot.TextureResidentBytes = 123456789;

        Snapshot.CameraLocation = FVector{100.0, -250.5, 90.25};
        Snapshot.CameraRotation = FRotator{-12.5, 47.0, 0.0};
        Snapshot.CameraFov = 78.5f;
        Snapshot.ScalabilityPreset = FString{TEXT("View 3 · AA 2 · Shadow 3")};
        Snapshot.ScreenPercentage = 66.0f;
        Snapshot.BuildVersion = FString{TEXT("++Spec+Branch-CL-12345")};

        auto Aux = FCkOptimizationDebugger_SnapshotAuxImage{};
        Aux.Name = FString{TEXT("Depth")};
        Aux.Png = TArray64<uint8>{{0x89, 0x50, 0x4E, 0x47, 0x11, 0x22}};
        Snapshot.AuxImages.Add(Aux);

        auto PrimA = Make_Prim(TEXT("Shelf_12 / SM_ShelfBody"), 4210, 3, 1);
        PrimA.MeshDisplayName = FString{TEXT("SM_ShelfBody")};
        PrimA.MeshAssetPath = FSoftObjectPath{TEXT("/Game/Spec/SM_ShelfBody.SM_ShelfBody")};
        PrimA.MeshResourceSizeBytes = 987654;
        PrimA.Lods[0].Vertices = 2900;
        PrimA.Lods[0].ScreenSize = 1.0f;

        auto Slot = FCkOptimizationDebugger_SnapshotMaterialSlot{};
        Slot.SlotName = FString{TEXT("Body")};
        Slot.MaterialName = FString{TEXT("M_Shelf <inst>")};
        Slot.MaterialPath = FSoftObjectPath{TEXT("/Game/Spec/M_Shelf.M_Shelf")};
        Slot.BlendMode = FString{TEXT("Opaque")};
        Slot.ShadingModel = FString{TEXT("Lit")};
        Slot.IsTwoSided = true;
        Slot.UsedTextureCount = 2;
        Slot.UsedTextureNames = TArray<FString>{TEXT("T_Shelf_D"), TEXT("T_Shelf_N")};
        PrimA.MaterialSlots.Add(Slot);

        auto PrimB = Make_Prim(TEXT("Crowd / SK_Person"), 999, 5, 7);
        PrimB.Kind = ECkOptimizationDebugger_SnapshotPrimKind::SkeletalMesh;
        PrimB.IsNanite = true;
        PrimB.DistanceFromCamera = 345.5f;

        Snapshot.Prims.Add(PrimA);
        Snapshot.Prims.Add(PrimB);

        Snapshot.SelectedPrims.Add(1);
        Snapshot.SelectedPrims.Add(0);

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_FileCodecRoundTrip,
    "Ck.OptimizationDebugger.Snapshot.FileCodecRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_FileCodecRoundTrip::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_codec;
    using namespace ck_optimization_debugger_snapshot_spec;

    const auto Original = Make_FullSnapshot();
    const auto Bytes = Encode_SnapshotFile(Original);

    TestTrue(TEXT("A snapshot encodes to something"), Bytes.Num() > 0);

    const auto Decoded = Decode_SnapshotFile(Bytes);

    if (NOT TestTrue(TEXT("A whole file decodes"), Decoded.IsSet()))
    { return false; }

    // Field-by-field rather than a byte compare of re-encoded output: a re-encode could cancel out a bug that
    // loses data symmetrically, and the fields are the contract.
    TestEqual(TEXT("Id survives"), Decoded->Id, Original.Id);
    TestEqual(TEXT("Label survives"), Decoded->Label, Original.Label);
    TestEqual(TEXT("CapturedAt survives"), Decoded->CapturedAt, Original.CapturedAt);
    TestEqual(TEXT("World survives"), Decoded->WorldName, Original.WorldName);
    TestEqual(TEXT("Width survives"), Decoded->Width, Original.Width);
    TestEqual(TEXT("Height survives"), Decoded->Height, Original.Height);
    TestEqual(TEXT("Colour byte count survives"), static_cast<int32>(Decoded->ColorPng.Num()), static_cast<int32>(Original.ColorPng.Num()));
    TestTrue(TEXT("...identically"), Decoded->ColorPng == Original.ColorPng);
    TestEqual(TEXT("HasIdMap survives"), Decoded->HasIdMap, Original.HasIdMap);
    TestTrue(TEXT("The RLE map survives"), Decoded->IdMapRle == Original.IdMapRle);
    TestEqual(TEXT("Unidentified count survives"), Decoded->UnidentifiedPixelCount, Original.UnidentifiedPixelCount);
    TestEqual(TEXT("Notes survive"), Decoded->CaptureNotes, Original.CaptureNotes);
    TestEqual(TEXT("Unique materials survive"), Decoded->UniqueMaterialCount, Original.UniqueMaterialCount);
    TestEqual(TEXT("Unique textures survive"), Decoded->UniqueTextureCount, Original.UniqueTextureCount);
    TestEqual(TEXT("Texture bytes survive"), Decoded->TextureResidentBytes, Original.TextureResidentBytes);

    if (TestEqual(TEXT("The prim table survives whole"), Decoded->Prims.Num(), Original.Prims.Num()))
    {
        const auto& PrimA = Decoded->Prims[0];

        TestEqual(TEXT("A prim's name survives"), PrimA.DisplayName, Original.Prims[0].DisplayName);
        TestEqual(TEXT("...its mesh path"), PrimA.MeshAssetPath.ToString(), Original.Prims[0].MeshAssetPath.ToString());
        TestEqual(TEXT("...its resource size"), PrimA.MeshResourceSizeBytes, Original.Prims[0].MeshResourceSizeBytes);
        TestEqual(TEXT("...its LOD vertices"), PrimA.Lods[0].Vertices, Original.Prims[0].Lods[0].Vertices);
        TestEqual(TEXT("...its LOD screen size"), PrimA.Lods[0].ScreenSize, Original.Prims[0].Lods[0].ScreenSize);

        if (TestEqual(TEXT("...its material slots"), PrimA.MaterialSlots.Num(), 1))
        {
            TestEqual(TEXT("...a slot's texture names"),
                FString::Join(PrimA.MaterialSlots[0].UsedTextureNames, TEXT(",")),
                FString::Join(Original.Prims[0].MaterialSlots[0].UsedTextureNames, TEXT(",")));
            TestEqual(TEXT("...a slot's two-sidedness"),
                PrimA.MaterialSlots[0].IsTwoSided, Original.Prims[0].MaterialSlots[0].IsTwoSided);
        }

        TestEqual(TEXT("The second prim's kind survives"),
            static_cast<int32>(Decoded->Prims[1].Kind), static_cast<int32>(Original.Prims[1].Kind));
        TestEqual(TEXT("...and its Nanite flag"), Decoded->Prims[1].IsNanite, Original.Prims[1].IsNanite);
    }

    TestEqual(TEXT("The selection survives"), Decoded->SelectedPrims.Num(), 2);
    TestTrue(TEXT("...with its members"),
        Decoded->SelectedPrims.Contains(0) && Decoded->SelectedPrims.Contains(1));

    // ---- v2: the point of view, the capture context and the auxiliary images ----
    TestEqual(TEXT("The camera location survives"), Decoded->CameraLocation, Original.CameraLocation);
    TestEqual(TEXT("...its rotation"), Decoded->CameraRotation, Original.CameraRotation);
    TestEqual(TEXT("...its FOV"), Decoded->CameraFov, Original.CameraFov);
    TestTrue(TEXT("...so the decoded snapshot can be recaptured from"), Get_HasPov(Decoded.GetValue()));

    TestEqual(TEXT("The scalability preset survives"), Decoded->ScalabilityPreset, Original.ScalabilityPreset);
    TestEqual(TEXT("...the screen percentage"), Decoded->ScreenPercentage, Original.ScreenPercentage);
    TestEqual(TEXT("...the build version"), Decoded->BuildVersion, Original.BuildVersion);

    if (TestEqual(TEXT("The auxiliary images survive"), Decoded->AuxImages.Num(), Original.AuxImages.Num()))
    {
        TestEqual(TEXT("...by name"), Decoded->AuxImages[0].Name, Original.AuxImages[0].Name);
        TestTrue(TEXT("...and byte for byte"), Decoded->AuxImages[0].Png == Original.AuxImages[0].Png);
    }

    // A snapshot that never knew its POV must not claim one, or the recapture button would offer to replay a view
    // that is a zero-FOV camera at the origin.
    TestFalse(TEXT("A POV-less snapshot reports no POV"), Get_HasPov(FCkOptimizationDebugger_Snapshot{}));

    // ---- The refusals ----
    auto Truncated = Bytes;
    Truncated.SetNum(Bytes.Num() - 7);
    TestFalse(TEXT("A truncated file decodes to nothing, never a partial snapshot"),
        Decode_SnapshotFile(Truncated).IsSet());

    auto WrongMagic = Bytes;
    WrongMagic[0] = 0x00;
    TestFalse(TEXT("A file with the wrong magic decodes to nothing"),
        Decode_SnapshotFile(WrongMagic).IsSet());

    auto FutureVersion = Bytes;
    FutureVersion[4] = 0xFF;
    TestFalse(TEXT("A file from a future version decodes to nothing, never a guess"),
        Decode_SnapshotFile(FutureVersion).IsSet());

    TestFalse(TEXT("An empty buffer decodes to nothing"), Decode_SnapshotFile(TArray<uint8>{}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_ReportDeterminism,
    "Ck.OptimizationDebugger.Snapshot.ReportDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_ReportDeterminism::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot_report;
    using namespace ck_optimization_debugger_snapshot_spec;

    const auto Snapshot = Make_FullSnapshot();
    const auto GeneratedAt = FDateTime{2026, 8, 18, 15, 0, 0};

    const auto First = Build_SnapshotReportHtml(Snapshot, GeneratedAt);
    const auto Second = Build_SnapshotReportHtml(Snapshot, GeneratedAt);

    // The determinism the module doctrine demanded the export arrive with: same input, byte-identical output. A
    // report that differed between two generations would turn every diff of two reports into noise.
    TestTrue(TEXT("Two builds of the same snapshot are byte-identical"), First.Equals(Second, ESearchCase::CaseSensitive));

    TestTrue(TEXT("The report carries the LOD0 triangle total"), First.Contains(TEXT("5209")));
    TestTrue(TEXT("...the texture memory figure"), First.Contains(TEXT("117.7 MB")));
    TestTrue(TEXT("...the embedded capture image"), First.Contains(TEXT("data:image/png;base64,")));
    TestTrue(TEXT("...the mesh identification image"), First.Contains(TEXT("Mesh identification")));

    // A shared report has to say where the picture was taken and what it was rendered at, or the reader cannot tell
    // whether two reports from two machines are describing the same thing.
    TestTrue(TEXT("...the point of view"), First.Contains(TEXT("FOV 78.5")));
    TestTrue(TEXT("...the capture context"), First.Contains(TEXT("++Spec+Branch-CL-12345")));

    // Names come from artist assets and can carry markup; unescaped, one asset name breaks the whole file.
    TestTrue(TEXT("Names are HTML-escaped"), First.Contains(TEXT("L_Spec &amp; &lt;World&gt;")));
    TestFalse(TEXT("...and never raw"), First.Contains(TEXT("M_Shelf <inst>")));

    // Worst-first: the 4210-triangle shelf outranks the 999-triangle skeletal.
    const auto ShelfAt = First.Find(TEXT("SM_ShelfBody"));
    const auto PersonAt = First.Find(TEXT("SK_Person"));
    TestTrue(TEXT("The mesh table is sorted worst-first"), ShelfAt != INDEX_NONE && PersonAt != INDEX_NONE && ShelfAt < PersonAt);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_ScreenCoverage,
    "Ck.OptimizationDebugger.Snapshot.ScreenCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_ScreenCoverage::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_lens;
    using namespace ck_optimization_debugger_snapshot_spec;

    // Two prims, one sentinel pixel, and one id no table can back — which is what a stored map read against the
    // wrong snapshot looks like.
    const auto Ids = TArray<uint32>{0u, 0u, 1u, k_NoPrim, 1u, 1u, 0u, 7u};

    const auto Coverage = Get_ScreenCoverage(Ids, 2);

    if (TestEqual(TEXT("Coverage is indexed by prim"), Coverage.Num(), 2))
    {
        TestEqual(TEXT("The first prim owns its pixels"), Coverage[0], 3);
        TestEqual(TEXT("...and so does the second"), Coverage[1], 3);
    }

    TestEqual(TEXT("A table with no prims has no coverage"), Get_ScreenCoverage(Ids, 0).Num(), 0);
    TestEqual(TEXT("An empty map covers nothing"), Get_ScreenCoverage(TArray<uint32>{}, 2).Num(), 2);

    // Density is per pixel the mesh actually occupies, and a mesh nothing can see has none to report: an infinity or
    // a sentinel here would sort to the top of every list that ranks by it.
    const auto Prim = Make_Prim(TEXT("SM_Dense"), 1000, 1);

    TestEqual(TEXT("Density divides by covered pixels"), Get_TrianglesPerCoveredPixel(Prim, 100), 10.0f);
    TestEqual(TEXT("An unseen mesh has no density"), Get_TrianglesPerCoveredPixel(Prim, 0), 0.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_LensRules,
    "Ck.OptimizationDebugger.Snapshot.LensRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_LensRules::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_lens;
    using namespace ck_optimization_debugger_snapshot_spec;

    // Two meshes and a sky pixel. Prim 0 is the expensive one in every dimension the lenses measure, prim 1 carries
    // the flags it does not.
    auto Snapshot = FCkOptimizationDebugger_Snapshot{};
    Snapshot.Width = 2;
    Snapshot.Height = 2;
    Snapshot.HasIdMap = true;

    auto Heavy = Make_Prim(TEXT("SM_Heavy"), 4000, 2);
    Heavy.IsNanite = true;
    Heavy.MeshResourceSizeBytes = 4 * 1024 * 1024;

    auto HeavySlot = FCkOptimizationDebugger_SnapshotMaterialSlot{};
    HeavySlot.BlendMode = FString{TEXT("Opaque")};
    HeavySlot.UsedTextureCount = 20;
    Heavy.MaterialSlots.Add(HeavySlot);

    auto Light = Make_Prim(TEXT("SM_Light"), 100, 1);
    Light.MeshResourceSizeBytes = 4 * 1024;

    auto LightSlot = FCkOptimizationDebugger_SnapshotMaterialSlot{};
    LightSlot.BlendMode = FString{TEXT("Translucent")};
    LightSlot.UsedTextureCount = 2;
    Light.MaterialSlots.Add(LightSlot);

    Snapshot.Prims.Add(Heavy);
    Snapshot.Prims.Add(Light);

    const auto Ids = TArray<uint32>{0u, 0u, 1u, k_NoPrim};

    auto Thresholds = FCkOptimizationDebugger_Thresholds{};
    Thresholds.MaxTriangleCountLOD0 = 500;
    Thresholds.MaxMaterialSlots = 8;
    Thresholds.MaxTextureSamplers = 16;

    // ---- The capture is not a lens ----
    TestEqual(TEXT("The capture view paints nothing"),
        Build_LensPixels(Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::None, Thresholds).Num(), 0);

    // An ID map that does not match the picture cannot be painted onto it; a lens drawn at the wrong stride would be
    // a convincing image of nothing.
    TestEqual(TEXT("A mismatched ID map paints nothing"),
        Build_LensPixels(Snapshot, TArray<uint32>{0u, 1u}, ECkOptimizationDebugger_SnapshotLens::NaniteMask,
            Thresholds).Num(), 0);

    // ---- A mask tints only its members ----
    const auto NanitePixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::NaniteMask, Thresholds);

    if (TestEqual(TEXT("A lens covers every pixel"), NanitePixels.Num(), 4))
    {
        TestTrue(TEXT("One mesh reads as one colour"), NanitePixels[0] == NanitePixels[1]);
        TestFalse(TEXT("The Nanite mesh is not painted as the other one"), NanitePixels[0] == NanitePixels[2]);
        TestTrue(TEXT("Sky is black on a lens"), NanitePixels[3] == FColor::Black);
    }

    // ---- Unidentified inverts it: the sentinel is the subject ----
    const auto UnidentifiedPixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::Unidentified, Thresholds);

    if (TestEqual(TEXT("The unidentified lens covers every pixel"), UnidentifiedPixels.Num(), 4))
    {
        TestTrue(TEXT("Both meshes dim together"), UnidentifiedPixels[0] == UnidentifiedPixels[2]);
        TestFalse(TEXT("...and the sky is the highlight"), UnidentifiedPixels[3] == UnidentifiedPixels[0]);
        TestFalse(TEXT("...which is not the sentinel black"), UnidentifiedPixels[3] == FColor::Black);
    }

    // ---- Two-sided or non-opaque is a mask over the material slots ----
    const auto FlagPixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::MaterialFlags, Thresholds);

    if (TestEqual(TEXT("The material-flag lens covers every pixel"), FlagPixels.Num(), 4))
    {
        // The translucent mesh is the member here, and it is NOT the mesh the Nanite mask picked. A lens that
        // painted the same mesh on every mask would be reading one flag for all of them.
        TestFalse(TEXT("The translucent mesh is flagged, the opaque one is not"), FlagPixels[0] == FlagPixels[2]);
        TestTrue(TEXT("...and the two masks disagree about which mesh"),
            (NanitePixels[0] == FlagPixels[2]) && (NanitePixels[2] == FlagPixels[0]));
    }

    // ---- A scalar lens normalizes across the view ----
    const auto DensityPixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::TriangleDensity, Thresholds);

    if (TestEqual(TEXT("The density lens covers every pixel"), DensityPixels.Num(), 4))
    {
        TestFalse(TEXT("Two densities are two colours"), DensityPixels[0] == DensityPixels[2]);
        TestTrue(TEXT("...and sky still has none"), DensityPixels[3] == FColor::Black);
    }

    // ---- The budget lens colours exactly what a check would flag ----
    const auto HeavySeverity = TryGet_BudgetSeverity(Snapshot.Prims[0], Thresholds);
    const auto LightSeverity = TryGet_BudgetSeverity(Snapshot.Prims[1], Thresholds);

    TestTrue(TEXT("The over-budget mesh is graded"), HeavySeverity.IsSet());
    TestFalse(TEXT("...and the mesh within every budget is not"), LightSeverity.IsSet());

    if (HeavySeverity.IsSet())
    {
        // 4000 triangles against a 500 budget is eight times over, which is past the doubling the analysis engine
        // escalates at: the same grading the findings page would apply to the same mesh.
        TestEqual(TEXT("...as Critical, because it is past twice the budget"),
            static_cast<int32>(HeavySeverity.GetValue()),
            static_cast<int32>(ECkOptimizationDebugger_Severity::Critical));
    }

    const auto BudgetPixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::Budget, Thresholds);

    if (TestEqual(TEXT("The budget lens covers every pixel"), BudgetPixels.Num(), 4))
    { TestFalse(TEXT("The over-budget mesh is not painted as the compliant one"), BudgetPixels[0] == BudgetPixels[2]); }

    // The largest sampler count of any ONE material is what breaches a per-material budget; a sum would flag six
    // modest materials and miss the one that actually exceeds the platform limit.
    TestEqual(TEXT("Samplers are the largest of any one slot"), Get_MaxSamplerCount(Snapshot.Prims[0]), 20);

    // ---- Determinism, and a legend for every lens ----
    const auto SecondDensityPixels = Build_LensPixels(
        Snapshot, Ids, ECkOptimizationDebugger_SnapshotLens::TriangleDensity, Thresholds);

    TestTrue(TEXT("The same inputs paint the same pixels"), DensityPixels == SecondDensityPixels);

    for (const auto& Lens : Get_AllLenses())
    {
        const auto Legend = Get_LensLegendText(Lens);

        if (Lens == ECkOptimizationDebugger_SnapshotLens::None)
        {
            TestTrue(TEXT("The capture has no legend, because it is not a measurement"), Legend.IsEmpty());
            continue;
        }

        // A heatmap without a legend is a picture, not a measurement.
        TestFalse(TEXT("Every lens says what its colours mean"), Legend.IsEmpty());
        TestFalse(TEXT("...and every lens is named"), Get_LensLabel(Lens).IsEmpty());
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_MeshListSort,
    "Ck.OptimizationDebugger.Snapshot.MeshListSort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_MeshListSort::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_lens;
    using namespace ck_optimization_debugger_snapshot_spec;

    auto Snapshot = FCkOptimizationDebugger_Snapshot{};

    // Two meshes with the SAME triangle count, so the tie-break is what decides their order, plus one heavier mesh.
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_TiedA"), 500, 1));
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_Heavy"), 9000, 4));
    Snapshot.Prims.Add(Make_Prim(TEXT("SM_TiedB"), 500, 1));

    auto Thresholds = FCkOptimizationDebugger_Thresholds{};
    Thresholds.MaxTriangleCountLOD0 = 1000;
    Thresholds.MaxMaterialSlots = 8;
    Thresholds.MaxTextureSamplers = 16;

    const auto Coverage = TArray<int32>{100, 0, 250};
    const auto Rows = Build_SnapshotMeshRows(Snapshot, Coverage, Thresholds);

    if (TestEqual(TEXT("One row per prim, in prim order"), Rows.Num(), 3))
    {
        TestEqual(TEXT("A row knows its prim"), Rows[0].PrimIndex, 0);
        TestEqual(TEXT("...and its coverage"), Rows[0].CoveredPixels, 100);
        TestEqual(TEXT("...and the density that follows from it"), Rows[0].TrianglesPerPixel, 5.0f);

        // A prim nothing can see is covered by zero pixels, which is a measurement; it is NOT the em-dash case.
        TestEqual(TEXT("An unseen mesh reads as zero coverage"), Rows[1].CoveredPixels, 0);

        TestTrue(TEXT("The over-budget mesh carries its severity"), Rows[1].BudgetSeverity.IsSet());
        TestFalse(TEXT("...and a compliant one carries none"), Rows[0].BudgetSeverity.IsSet());
    }

    // No coverage at all is the no-identification case: unset, so the table prints an em dash rather than a zero
    // that would read as "not visible".
    const auto RowsWithoutCoverage = Build_SnapshotMeshRows(Snapshot, TArray<int32>{}, Thresholds);

    if (TestEqual(TEXT("Rows survive a snapshot with no identification"), RowsWithoutCoverage.Num(), 3))
    {
        TestEqual(TEXT("...with coverage unset"), RowsWithoutCoverage[0].CoveredPixels, INDEX_NONE);
        TestEqual(TEXT("...and no density"), RowsWithoutCoverage[0].TrianglesPerPixel, 0.0f);
    }

    // ---- Sorting ----
    const auto Descending = Get_SortedSnapshotMeshRows(
        Rows, ECkOptimizationDebugger_SnapshotMeshColumn::Triangles, false);

    if (TestEqual(TEXT("Sorting keeps every row"), Descending.Num(), 3))
    {
        TestEqual(TEXT("Worst first when descending"), Descending[0].PrimIndex, 1);
        TestEqual(TEXT("Ties keep prim order"), Descending[1].PrimIndex, 0);
        TestEqual(TEXT("...both of them"), Descending[2].PrimIndex, 2);
    }

    const auto Ascending = Get_SortedSnapshotMeshRows(
        Rows, ECkOptimizationDebugger_SnapshotMeshColumn::Triangles, true);

    if (TestEqual(TEXT("Reversing keeps every row"), Ascending.Num(), 3))
    {
        TestEqual(TEXT("Smallest first when ascending"), Ascending[0].PrimIndex, 0);

        // The tie-break NEVER reverses with the arrow: rows equal on the sorted column jumping around when the
        // reader flips the header is how a table stops being trusted.
        TestEqual(TEXT("...and the tie-break still runs in prim order"), Ascending[1].PrimIndex, 2);
        TestEqual(TEXT("...with the heaviest last"), Ascending[2].PrimIndex, 1);
    }

    // ---- Columns ----
    for (const auto& Column : Get_AllMeshColumns())
    {
        const auto RoundTripped = TryGet_MeshColumnFromId(Get_MeshColumnId(Column));

        // The header row addresses columns by FName; an id that does not map back is a column that silently cannot
        // be sorted.
        if (TestTrue(TEXT("Every column id maps back to its column"), RoundTripped.IsSet()))
        { TestEqual(TEXT("...to the same one"), static_cast<int32>(RoundTripped.GetValue()), static_cast<int32>(Column)); }

        TestFalse(TEXT("...and every column is labelled"), Get_MeshColumnLabel(Column).IsEmpty());
    }

    TestFalse(TEXT("An unknown column id maps to nothing"), TryGet_MeshColumnFromId(FName{TEXT("NotAColumn")}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_spec
{
    auto
        Make_ComparePrim(
            const FString& InMeshName,
            const FString& InPath,
            int32 InTriangles,
            int32 InInstances)
        -> FCkOptimizationDebugger_SnapshotPrim
    {
        auto Prim = Make_Prim(InMeshName, InTriangles, 1, InInstances);
        Prim.MeshDisplayName = InMeshName;
        Prim.MeshAssetPath = FSoftObjectPath{InPath};
        Prim.MeshResourceSizeBytes = 1000;

        return Prim;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Snapshot_CompareDelta,
    "Ck.OptimizationDebugger.Snapshot.CompareDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Snapshot_CompareDelta::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_debugger_snapshot_lens;
    using namespace ck_optimization_debugger_snapshot_spec;

    // The baseline: one shelf and one crate, both identified in the picture.
    auto Baseline = FCkOptimizationDebugger_Snapshot{};
    Baseline.Prims.Add(Make_ComparePrim(TEXT("SM_Shelf"), TEXT("/Game/Spec/SM_Shelf.SM_Shelf"), 100, 1));
    Baseline.Prims.Add(Make_ComparePrim(TEXT("SM_Crate"), TEXT("/Game/Spec/SM_Crate.SM_Crate"), 500, 1));
    Baseline.HasIdMap = true;
    Baseline.IdMapRle = Encode_IdMapRle(TArray<uint32>{0u, 0u, 1u, k_NoPrim});

    // The current capture: a second shelf was placed, a barrel appeared, the crate is gone. Prim indices renumbered,
    // which is exactly what a comparison keyed on index would get wrong.
    auto Current = FCkOptimizationDebugger_Snapshot{};
    Current.Prims.Add(Make_ComparePrim(TEXT("SM_Barrel"), TEXT("/Game/Spec/SM_Barrel.SM_Barrel"), 50, 1));
    Current.Prims.Add(Make_ComparePrim(TEXT("SM_Shelf"), TEXT("/Game/Spec/SM_Shelf.SM_Shelf"), 100, 1));
    Current.Prims.Add(Make_ComparePrim(TEXT("SM_Shelf"), TEXT("/Game/Spec/SM_Shelf.SM_Shelf"), 100, 3));
    Current.HasIdMap = true;
    Current.IdMapRle = Encode_IdMapRle(TArray<uint32>{1u, 1u, 2u, 0u});

    const auto Delta = Build_SnapshotDelta(Baseline, Current);

    if (NOT TestEqual(TEXT("One row per changed mesh asset"), Delta.Num(), 3))
    { return false; }

    // Worst regression first: the shelf gained a placement, the barrel is new and smaller, the crate is a saving.
    TestEqual(TEXT("The biggest regression leads"), Delta[0].DisplayName, FString{TEXT("SM_Shelf")});
    TestEqual(TEXT("...as a change, not an addition"),
        static_cast<int32>(Delta[0].Kind),
        static_cast<int32>(ECkOptimizationDebugger_SnapshotDeltaKind::Changed));
    TestEqual(TEXT("...whose placements grew"), Delta[0].PlacementDelta, 1);
    TestEqual(TEXT("...and whose triangles grew with them"), static_cast<int32>(Delta[0].Lod0TriangleDelta), 100);
    TestEqual(TEXT("...and whose instances grew by three"), Delta[0].InstanceDelta, 3);

    // One asset, two placements: the memory delta is the ASSET's, so re-using a mesh does not read as new memory.
    TestEqual(TEXT("Re-using an asset costs no new mesh memory"), static_cast<int32>(Delta[0].MeshMemoryDelta), 0);

    TestEqual(TEXT("The new mesh is next"), Delta[1].DisplayName, FString{TEXT("SM_Barrel")});
    TestEqual(TEXT("...as an addition"),
        static_cast<int32>(Delta[1].Kind),
        static_cast<int32>(ECkOptimizationDebugger_SnapshotDeltaKind::Added));
    TestEqual(TEXT("...carrying its whole cost"), static_cast<int32>(Delta[1].Lod0TriangleDelta), 50);

    TestEqual(TEXT("The saving sorts last"), Delta[2].DisplayName, FString{TEXT("SM_Crate")});
    TestEqual(TEXT("...as a removal"),
        static_cast<int32>(Delta[2].Kind),
        static_cast<int32>(ECkOptimizationDebugger_SnapshotDeltaKind::Removed));
    TestEqual(TEXT("...as a negative delta"), static_cast<int32>(Delta[2].Lod0TriangleDelta), -500);

    // Coverage IS comparable here: both sides counted pixels. The shelf owned two of them and now owns three.
    if (TestTrue(TEXT("Coverage is compared when both sides have an ID map"), Delta[0].CoverageDelta.IsSet()))
    { TestEqual(TEXT("...as a pixel difference"), Delta[0].CoverageDelta.GetValue(), 1); }

    // ---- A snapshot that never counted pixels cannot be compared on them ----
    auto WithoutIdMap = Current;
    WithoutIdMap.HasIdMap = false;
    WithoutIdMap.IdMapRle.Reset();

    const auto BlindDelta = Build_SnapshotDelta(Baseline, WithoutIdMap);

    if (TestEqual(TEXT("The comparison still runs without identification"), BlindDelta.Num(), 3))
    {
        // Unset, never zero: zero would read as "this mesh takes up the same room", which is a claim neither
        // capture supports.
        TestFalse(TEXT("...but coverage is unset rather than zero"), BlindDelta[0].CoverageDelta.IsSet());
    }

    // ---- Identical captures are not a table of zeroes ----
    TestEqual(TEXT("A capture compared with itself reports nothing"),
        Build_SnapshotDelta(Current, Current).Num(), 0);

    // ---- Determinism ----
    const auto SecondDelta = Build_SnapshotDelta(Baseline, Current);

    if (TestEqual(TEXT("Two runs produce the same number of rows"), SecondDelta.Num(), Delta.Num()))
    {
        auto SameOrder = true;

        for (auto Index = 0; Index < Delta.Num(); ++Index)
        { SameOrder = SameOrder && SecondDelta[Index].Key == Delta[Index].Key; }

        // TMap iteration is a hashing detail, so without the key tie-break two reads of one comparison could
        // disagree about the order and every re-read would look like a fresh diff.
        TestTrue(TEXT("...in the same order"), SameOrder);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
