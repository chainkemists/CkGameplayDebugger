#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

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

#endif
