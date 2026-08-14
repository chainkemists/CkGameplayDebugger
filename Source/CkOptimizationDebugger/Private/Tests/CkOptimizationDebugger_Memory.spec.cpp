#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_memory_spec
{
    /** A texture row with every sortable field under the caller's control, so a sort that read the wrong field lands
     *  on an order no other field could have produced. */
    auto
        MakeTextureRow(
            const FString& InName,
            const FString& InPathLeaf,
            const FString& InFormat,
            int32 InSide,
            int64 InBytes,
            int64 InGpuBytes,
            int32 InResidentMips)
        -> FCkOptimizationDebugger_MemoryRow
    {
        auto Row = FCkOptimizationDebugger_MemoryRow{};

        Row.Table = ECkOptimizationDebugger_MemoryTable::Textures;
        Row.AssetPath = ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InPathLeaf, InPathLeaf);
        Row.DisplayName = InName;
        Row.ClassName = FString{TEXT("Texture2D")};
        Row.FormatName = InFormat;
        Row.Width = InSide;
        Row.Height = InSide;
        Row.MipCount = 4;
        Row.LodGroupName = FString{TEXT("World")};
        Row.ResourceSizeBytes = InBytes;

        // The flag follows the TABLE, never the byte count — the same rule `Apply_ResourceSize` applies, asked
        // through the same projection so the fixture cannot drift from production. A texture with zero resident mips
        // reports zero dedicated bytes and is still a type that reports them.
        Row.GpuSizeBytes = InGpuBytes;
        Row.HasSeparableGpuSize = ck_optimization_debugger_model::Has_SeparableGpuSize(Row.Table);

        Row.IsStreamable = InResidentMips >= 0;
        Row.HasStreamingMetrics = InResidentMips >= 0;
        Row.ResidentMipCount = FMath::Max(InResidentMips, 0);
        Row.RequestedMipCount = FMath::Max(InResidentMips, 0);

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeMeshRow(
            const FString& InName,
            int32 InLodCount,
            int64 InTriangles,
            int64 InBytes)
        -> FCkOptimizationDebugger_MemoryRow
    {
        auto Row = FCkOptimizationDebugger_MemoryRow{};

        Row.Table = ECkOptimizationDebugger_MemoryTable::StaticMeshes;
        Row.AssetPath = ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InName, InName);
        Row.DisplayName = InName;
        Row.ClassName = FString{TEXT("StaticMesh")};
        Row.LodCount = InLodCount;
        Row.Lod0TriangleCount = InTriangles;
        Row.ResourceSizeBytes = InBytes;

        // Asked, not left to the default. A mesh is not separable today and `false` is what the default happens to
        // be — but a fixture that agrees with production by coincidence stops agreeing the moment the rule moves,
        // and silently. All three builders ask the same projection the scan does.
        Row.HasSeparableGpuSize = ck_optimization_debugger_model::Has_SeparableGpuSize(Row.Table);

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeRenderTargetRow(
            const FString& InName,
            int64 InBytes)
        -> FCkOptimizationDebugger_MemoryRow
    {
        auto Row = FCkOptimizationDebugger_MemoryRow{};

        Row.Table = ECkOptimizationDebugger_MemoryTable::RenderTargets;
        Row.AssetPath = ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InName, InName);
        Row.DisplayName = InName;
        Row.ClassName = FString{TEXT("TextureRenderTarget2D")};
        Row.FormatName = FString{TEXT("PF_FloatRGBA")};
        Row.Width = 512;
        Row.Height = 512;
        Row.ResourceSizeBytes = InBytes;

        // Asked rather than defaulted, for the reason `MakeMeshRow` states.
        Row.HasSeparableGpuSize = ck_optimization_debugger_model::Has_SeparableGpuSize(Row.Table);

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Join_Names(
            const TArray<FCkOptimizationDebugger_MemoryRow>& InRows)
        -> FString
    {
        auto Names = TArray<FString>{};

        for (const auto& Row : InRows)
        { Names.Add(Row.AssetPath); }

        return FString::Join(Names, TEXT(","));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Three textures where two of them are IDENTICAL on every sortable column, so only the tie-break can order
     *  them — plus one mesh, which no texture-table query may ever return.
     *
     *  **`Alpha` and `Copy` deliberately share the DISPLAY NAME "Alpha"** and differ only in their path. That is
     *  what gives the Name column a real tie to break, and it means the name is NOT a unique key here: a filter
     *  query of "alpha" matches two rows, correctly. Anything that needs to narrow to exactly one row must query a
     *  path leaf (`"Copy"`, `"Beta"`) — the fields that ARE unique per row. */
    auto
        MakeFixture()
        -> TArray<FCkOptimizationDebugger_MemoryRow>
    {
        return TArray<FCkOptimizationDebugger_MemoryRow>{
            MakeTextureRow(TEXT("Alpha"), TEXT("Alpha"), TEXT("PF_DXT1"), 64, 100, 100, 4),
            MakeTextureRow(TEXT("Beta"),  TEXT("Beta"),  TEXT("PF_B8G8R8A8"), 128, 300, 300, 8),
            MakeTextureRow(TEXT("Alpha"), TEXT("Copy"),  TEXT("PF_DXT1"), 64, 100, 0, -1),
            MakeMeshRow(TEXT("Rock"), 3, 12000, 900)};
    }

    // ----------------------------------------------------------------------------------------------------------------

    const auto k_AlphaPath = FString{TEXT("/Game/Spec/Alpha.Alpha")};
    const auto k_BetaPath  = FString{TEXT("/Game/Spec/Beta.Beta")};
    const auto k_CopyPath  = FString{TEXT("/Game/Spec/Copy.Copy")};
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Memory_SeparableGpuSize,
    "Ck.OptimizationDebugger.Memory.SeparableGpuSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Memory_SeparableGpuSize::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;
    using namespace ck_optimization_debugger_memory_spec;

    // The CONTRACT: whether a row reports a separable video-memory figure is a property of its TYPE, not of the
    // number that came back. `UTexture2D::GetResourceSizeEx` routes mip bytes through the dedicated-video bucket;
    // render targets and static meshes fold everything into the untagged one.
    TestTrue(TEXT("Textures report a separable GPU figure"),
        Has_SeparableGpuSize(ECkOptimizationDebugger_MemoryTable::Textures));
    TestFalse(TEXT("Render targets do not"),
        Has_SeparableGpuSize(ECkOptimizationDebugger_MemoryTable::RenderTargets));
    TestFalse(TEXT("Static meshes do not"),
        Has_SeparableGpuSize(ECkOptimizationDebugger_MemoryTable::StaticMeshes));

    // The case the value-based rule got wrong: a fully streamed-out texture has zero resident mips and therefore
    // zero dedicated bytes. That is a measurement, not the absence of one — the column must print `0 B`, which is
    // what the flag being true means, rather than the em dash that says "this type does not report it".
    const auto StreamedOut = MakeTextureRow(TEXT("T_Evicted"), TEXT("Evicted"),
        TEXT("DXT1"), 2048, 1024, /*InGpuBytes*/ 0, /*InResidentMips*/ 0);

    TestTrue(TEXT("A texture with nothing resident still reports a separable GPU figure"),
        StreamedOut.HasSeparableGpuSize);
    TestEqual(TEXT("...and that figure is a measured zero"), StreamedOut.GpuSizeBytes, int64{0});

    const auto Mesh = MakeMeshRow(TEXT("SM_Spec"), 3, 1000, 4096);

    TestFalse(TEXT("A static mesh reports no separable GPU figure whatever its size"), Mesh.HasSeparableGpuSize);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Memory_SortedProjection,
    "Ck.OptimizationDebugger.Memory.SortedProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Memory_SortedProjection::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_memory_spec;
    using namespace ck_optimization_debugger_model;

    const auto Rows = MakeFixture();

    // ---- Table and column identity ----
    TestEqual(TEXT("Every memory table is enumerated"), Get_AllMemoryTables().Num(), k_MemoryTableCount);
    TestEqual(TEXT("Every memory column is enumerated"), Get_AllMemoryColumns().Num(), k_MemoryColumnCount);

    for (const auto Table : Get_AllMemoryTables())
    {
        const auto RoundTrip = TryGet_MemoryTableFromId(Get_MemoryTableId(Table));

        TestTrue(TEXT("A table id resolves back to its table"), RoundTrip.IsSet());
        TestTrue(TEXT("...to the SAME table"), RoundTrip.IsSet() && RoundTrip.GetValue() == Table);
    }

    for (const auto Column : Get_AllMemoryColumns())
    {
        // This is the contract the header row sits on: Slate hands a column NAME back through the sort callback and
        // nothing else, so a name that did not resolve would silently sort by whatever was already selected.
        const auto RoundTrip = TryGet_MemoryColumnFromId(Get_MemoryColumnId(Column));

        TestTrue(TEXT("A column id resolves back to its column"), RoundTrip.IsSet());
        TestTrue(TEXT("...to the SAME column"), RoundTrip.IsSet() && RoundTrip.GetValue() == Column);
    }

    TestFalse(TEXT("An unknown column id resolves to nothing"),
        TryGet_MemoryColumnFromId(FName{TEXT("NotAColumn")}).IsSet());

    // ---- The table filter ----
    const auto Textures = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, false, FString{});

    TestEqual(TEXT("A texture query returns only textures"), Textures.Num(), 3);

    const auto Meshes = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::StaticMeshes,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, false, FString{});

    TestEqual(TEXT("A mesh query returns only meshes"), Meshes.Num(), 1);

    // ---- Size, both directions ----
    TestEqual(TEXT("Biggest first"),
        Join_Names(Textures),
        ck::Format_UE(TEXT("{},{},{}"), k_BetaPath, k_AlphaPath, k_CopyPath));

    const auto BySizeAscending = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, true, FString{});

    // The two 100-byte rows keep Alpha-before-Copy in BOTH directions: the arrow reverses the column, never the
    // path tie-break, so flipping the sort must not swap two rows the reader cannot tell apart.
    TestEqual(TEXT("Smallest first, with the tie-break unreversed"),
        Join_Names(BySizeAscending),
        ck::Format_UE(TEXT("{},{},{}"), k_AlphaPath, k_CopyPath, k_BetaPath));

    // ---- Name ----
    const auto ByName = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::Name, true, FString{});

    TestEqual(TEXT("By name, ties broken by path"),
        Join_Names(ByName),
        ck::Format_UE(TEXT("{},{},{}"), k_AlphaPath, k_CopyPath, k_BetaPath));

    // ---- Dimensions sorts on the KEY, not the printed string ----
    const auto ByDimensions = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::Dimensions, false, FString{});

    TestEqual(TEXT("Biggest surface first"),
        Join_Names(ByDimensions),
        ck::Format_UE(TEXT("{},{},{}"), k_BetaPath, k_AlphaPath, k_CopyPath));

    TestEqual(TEXT("A texture's dimension key is its pixel count"),
        Get_MemoryDimensionSortKey(Textures[0]), static_cast<int64>(128 * 128));

    TestEqual(TEXT("A mesh's dimension key is its LOD0 triangle count"),
        Get_MemoryDimensionSortKey(Meshes[0]), static_cast<int64>(12000));

    // ---- Streaming: unmeasured rows sort below every measured one ----
    const auto ByStreaming = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::Streaming, false, FString{});

    TestEqual(TEXT("Most resident first, unmeasured last"),
        Join_Names(ByStreaming),
        ck::Format_UE(TEXT("{},{},{}"), k_BetaPath, k_AlphaPath, k_CopyPath));

    TestEqual(TEXT("An unmeasured row sorts below zero resident mips"),
        Get_MemoryStreamingSortKey(Textures[2]), -1);

    TestEqual(TEXT("An unmeasurable row says so rather than printing a count"),
        Get_MemoryStreamingText(Textures[2]), FString{TEXT("not streamable")});

    // ---- Determinism ----
    for (const auto Column : Get_AllMemoryColumns())
    {
        const auto First = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
            Column, false, FString{});
        const auto Second = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
            Column, false, FString{});

        TestEqual(TEXT("The same query twice produces the same order"), Join_Names(First), Join_Names(Second));

        // A re-scan hands the rows back in whatever order the object iterator walked them; the projection has to be
        // blind to that or the list would appear to churn between two identical measurements.
        auto Shuffled = Rows;
        Algo::Reverse(Shuffled);

        const auto FromShuffled = Get_SortedMemoryRows(Shuffled, ECkOptimizationDebugger_MemoryTable::Textures,
            Column, false, FString{});

        TestEqual(TEXT("...regardless of the input order"), Join_Names(First), Join_Names(FromShuffled));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Memory_FilterMatch,
    "Ck.OptimizationDebugger.Memory.FilterMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Memory_FilterMatch::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_memory_spec;
    using namespace ck_optimization_debugger_model;

    const auto Rows = MakeFixture();
    const auto Alpha = Rows[0];
    const auto SharedNameRow = Rows[2];
    const auto Mesh = Rows[3];

    // The fixture invariant every assertion below leans on, asserted rather than assumed: two rows share a display
    // name and differ only in path. A future edit that made the names unique would silently turn the
    // matches-two-rows assertions into matches-one and they would still pass for the wrong reason.
    TestEqual(TEXT("Two fixture rows deliberately share a display name"),
        SharedNameRow.DisplayName, Alpha.DisplayName);
    TestNotEqual(TEXT("...and differ only in path"), SharedNameRow.AssetPath, Alpha.AssetPath);

    TestTrue(TEXT("An empty filter passes everything"), Matches_MemoryFilter(Alpha, FString{}));

    TestTrue(TEXT("A name matches, case-insensitively"), Matches_MemoryFilter(Alpha, TEXT("alPHa")));
    TestTrue(TEXT("A path fragment matches"), Matches_MemoryFilter(Alpha, TEXT("/Game/Spec")));
    TestTrue(TEXT("A format matches"), Matches_MemoryFilter(Alpha, TEXT("dxt")));
    TestTrue(TEXT("A class name matches"), Matches_MemoryFilter(Mesh, TEXT("staticmesh")));

    TestFalse(TEXT("Something no field carries matches nothing"), Matches_MemoryFilter(Alpha, TEXT("zzz")));

    // The dimensions and the sizes are deliberately NOT in the haystack: "64" would match a texture that is 64 wide,
    // one that is 640 wide and one whose path contains the number, which is three different questions.
    TestFalse(TEXT("A dimension is not searchable text"), Matches_MemoryFilter(Alpha, TEXT("64")));

    // ---- Through the projection ----
    // A NAME query matches every row carrying that name. Two rows here do, so two come back — the haystack is
    // name + path + class + format, and a filter that returned one of two equally-named rows would be hiding a row
    // the reader asked for.
    const auto ByName = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, false, TEXT("alpha"));

    TestEqual(TEXT("A shared name matches every row that carries it"), ByName.Num(), 2);
    TestEqual(TEXT("...both of them, path-ordered"),
        Join_Names(ByName), ck::Format_UE(TEXT("{},{}"), k_AlphaPath, k_CopyPath));

    // A PATH-unique query narrows to exactly one row. `Copy` appears in no other row's name, path, class or format —
    // and it is not in this row's own display name either, so a match here can only have come from the path.
    const auto Filtered = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, false, TEXT("Copy"));

    TestEqual(TEXT("The filter narrows the table"), Filtered.Num(), 1);
    TestEqual(TEXT("...to the row whose PATH matched, not its name"), Join_Names(Filtered), k_CopyPath);

    const auto FilteredOut = Get_SortedMemoryRows(Rows, ECkOptimizationDebugger_MemoryTable::Textures,
        ECkOptimizationDebugger_MemoryColumn::ResourceSize, false, TEXT("Rock"));

    // The mesh named Rock matches the query and is still absent: the table filter runs first, and a search must not
    // pull a row in from a table the reader is not looking at.
    TestEqual(TEXT("A match in another table stays in that table"), FilteredOut.Num(), 0);

    // ---- Through the model, which is where the window reads it ----
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_MemoryRows(Rows, ECkOptimizationDebugger_StreamingAvailability::Available);

    TestEqual(TEXT("Unfiltered, every texture is visible"),
        Model.Get_VisibleMemoryRowCount(ECkOptimizationDebugger_MemoryTable::Textures), 3);

    Model.Set_MemoryFilterString(TEXT("Copy"));

    TestEqual(TEXT("The memory filter narrows the count the tab binds to"),
        Model.Get_VisibleMemoryRowCount(ECkOptimizationDebugger_MemoryTable::Textures), 1);

    // The findings filter is a DIFFERENT field: a query typed on the findings page must not narrow these tables.
    Model.Get_Filter().FilterString = FString{TEXT("nothing-here")};

    TestEqual(TEXT("The findings filter does not touch the memory tables"),
        Model.Get_VisibleMemoryRowCount(ECkOptimizationDebugger_MemoryTable::Textures), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Memory_Totals,
    "Ck.OptimizationDebugger.Memory.Totals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Memory_Totals::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_memory_spec;
    using namespace ck_optimization_debugger_model;

    // ---- An empty census still describes all three tables ----
    const auto Empty = Get_MemoryTotals(TArray<FCkOptimizationDebugger_MemoryRow>{});

    TestEqual(TEXT("Every table is on the record, empty or not"), Empty.Tables.Num(), k_MemoryTableCount);
    TestEqual(TEXT("Nothing measured is nothing counted"), Empty.RowCount, 0);
    TestEqual(TEXT("...and no bytes"), Empty.ResourceSizeBytes, static_cast<int64>(0));

    for (auto Index = 0; Index < Empty.Tables.Num(); ++Index)
    {
        // Indexed BY enum value — the header reads `Tables[static_cast<int32>(Table)]` and would otherwise print one
        // table's figures under another's name.
        TestTrue(TEXT("Table entries are in enum order"),
            static_cast<int32>(Empty.Tables[Index].Table) == Index);
    }

    // ---- A real census ----
    auto Rows = MakeFixture();
    Rows.Add(MakeRenderTargetRow(TEXT("Capture"), 700));

    const auto Totals = Get_MemoryTotals(Rows);

    const auto& TextureTotals = Totals.Tables[static_cast<int32>(ECkOptimizationDebugger_MemoryTable::Textures)];
    const auto& TargetTotals  = Totals.Tables[static_cast<int32>(ECkOptimizationDebugger_MemoryTable::RenderTargets)];
    const auto& MeshTotals    = Totals.Tables[static_cast<int32>(ECkOptimizationDebugger_MemoryTable::StaticMeshes)];

    TestEqual(TEXT("Three textures"), TextureTotals.RowCount, 3);
    TestEqual(TEXT("...weighing 100 + 300 + 100"), TextureTotals.ResourceSizeBytes, static_cast<int64>(500));

    TestEqual(TEXT("One render target"), TargetTotals.RowCount, 1);
    TestEqual(TEXT("...weighing 700"), TargetTotals.ResourceSizeBytes, static_cast<int64>(700));

    TestEqual(TEXT("One mesh"), MeshTotals.RowCount, 1);
    TestEqual(TEXT("...weighing 900"), MeshTotals.ResourceSizeBytes, static_cast<int64>(900));

    TestEqual(TEXT("The grand count is every row"), Totals.RowCount, 5);
    TestEqual(TEXT("The grand total is the sum of the tables"),
        Totals.ResourceSizeBytes, static_cast<int64>(2100));

    // Only rows of a TYPE that reports a separable video-memory figure contribute to the GPU column. That is a
    // property of the table, not of the bytes that came back, so all THREE textures count — including "Copy",
    // whose fixture reports 0 GPU bytes because nothing of it is resident.
    //
    // Three, not two, is the whole point. Under the old value-based rule "Copy" was excluded for having zero bytes,
    // which conflated "this type reports no separable figure" with "this instance currently has none resident" —
    // and made the GPU column print the em dash that means "could not be measured" over a measurement that had
    // been taken and was genuinely zero.
    TestEqual(TEXT("All three textures report a separable GPU figure — it follows the table, not the byte count"),
        Totals.SeparableGpuRowCount, 3);

    // 100 + 300 + 0. The third row folding in a real zero is correct and is exactly what changed: a measured zero
    // belongs in a total, where an unmeasurable one would not.
    TestEqual(TEXT("...summing 100 + 300 + a genuine 0"), Totals.GpuSizeBytes, static_cast<int64>(400));
    TestEqual(TEXT("...all of them in the texture table"), TextureTotals.SeparableGpuRowCount, 3);
    TestEqual(TEXT("...and the texture table's GPU bytes are the same 400"),
        TextureTotals.GpuSizeBytes, static_cast<int64>(400));

    // The two families whose `GetResourceSizeEx` folds everything into the untagged bucket contribute NEITHER a row
    // nor a byte — which is what makes their GPU cell an em dash rather than a `0 B` neither of them claimed.
    TestEqual(TEXT("The render target reports no separable GPU figure"),
        TargetTotals.SeparableGpuRowCount, 0);
    TestEqual(TEXT("...and folds no bytes into the GPU total"),
        TargetTotals.GpuSizeBytes, static_cast<int64>(0));
    TestEqual(TEXT("The static mesh reports none either"), MeshTotals.SeparableGpuRowCount, 0);
    TestEqual(TEXT("...and folds no bytes in either"), MeshTotals.GpuSizeBytes, static_cast<int64>(0));

    // ---- The model's own lifecycle around it ----
    auto Model = FCkOptimizationDebugger_Model{};

    TestFalse(TEXT("A fresh model has taken no measurement"), Model.Get_HasMemoryScan());
    TestTrue(TEXT("...and says the streaming manager was not reached, rather than claiming it was"),
        Model.Get_StreamingAvailability() == ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable);

    Model.Set_MemoryFilterString(TEXT("alpha"));
    Model.Set_MemoryRows(Rows, ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled);

    TestTrue(TEXT("A scan is recorded as having happened"), Model.Get_HasMemoryScan());
    TestTrue(TEXT("...along with why its streaming column is empty"),
        Model.Get_StreamingAvailability() == ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled);

    // The header is the CENSUS, never the view. The filter above admits two of the five rows — both named "Alpha",
    // since the fixture shares that name deliberately — and the total must not drop with it, or the page would read
    // as a session that got lighter because somebody typed. The visible count is asserted alongside so this pins a
    // filter that demonstrably EXCLUDES something rather than one that happens to admit everything.
    TestEqual(TEXT("The filter does narrow the view"),
        Model.Get_VisibleMemoryRowCount(ECkOptimizationDebugger_MemoryTable::Textures), 2);
    TestEqual(TEXT("...but the totals ignore it"), Model.Get_MemoryTotals().RowCount, 5);

    TestTrue(TEXT("An available streamer prints no note — an unconditional footnote is one nobody reads"),
        Get_StreamingAvailabilityNote(ECkOptimizationDebugger_StreamingAvailability::Available).IsEmpty());
    TestFalse(TEXT("A disabled streamer explains itself"),
        Get_StreamingAvailabilityNote(ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled).IsEmpty());
    TestFalse(TEXT("An absent manager explains itself differently"),
        Get_StreamingAvailabilityNote(ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable).IsEmpty());
    TestNotEqual(TEXT("...and the two reasons are not the same sentence"),
        Get_StreamingAvailabilityNote(ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled),
        Get_StreamingAvailabilityNote(ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable));

    Model.Reset();

    TestFalse(TEXT("A session boundary drops the census"), Model.Get_HasMemoryScan());
    TestEqual(TEXT("...and the rows with it"), Model.Get_MemoryTotals().RowCount, 0);
    TestEqual(TEXT("The memory filter SURVIVES — it is the user's question, not the answer"),
        Model.Get_MemoryFilterString(), FString{TEXT("alpha")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
