#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_dashboard_spec
{
    /** A census with every stat set to a distinct multiple of its seed, so a delta that read the wrong field lands on
     *  a number no other field could have produced. */
    auto
        MakeSummary(
            int32 InSeed)
        -> FCkOptimizationDebugger_ScanSummary
    {
        auto Summary = FCkOptimizationDebugger_ScanSummary{};

        Summary.ActorCount            = InSeed * 1;
        Summary.StaticMeshUsageCount  = InSeed * 2;
        Summary.UniqueStaticMeshCount = InSeed * 3;
        Summary.UniqueMaterialCount   = InSeed * 4;
        Summary.UniqueTextureCount    = InSeed * 5;
        Summary.Lod0TriangleTotal     = static_cast<int64>(InSeed) * 6;
        Summary.LightCount            = InSeed * 7;

        Summary.FindingCounts.CriticalCount = InSeed * 8;
        Summary.FindingCounts.MajorCount    = InSeed * 9;
        Summary.FindingCounts.MinorCount    = InSeed * 10;

        Summary.Disk.TotalBytes = static_cast<int64>(InSeed) * 11;
        Summary.Disk.IsAvailable = true;

        return Summary;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeFinding(
            const FString& InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            const FString& InAssetName)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};
        Finding.CheckId = FName{*InCheckId};
        Finding.Severity = InSeverity;
        Finding.Category = ECkOptimizationDebugger_Category::Mesh;
        Finding.Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Finding.Target.Path = FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InAssetName, InAssetName)};
        Finding.Target.DisplayName = InAssetName;
        Finding.Title = InCheckId;
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(Finding.CheckId, Finding.Target);
        return Finding;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Dashboard_SummaryDelta,
    "Ck.OptimizationDebugger.Dashboard.SummaryDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Dashboard_SummaryDelta::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_dashboard_spec;
    using namespace ck_optimization_debugger_model;

    // ---- The pure projection ----
    const auto Previous = MakeSummary(10);
    const auto Current = MakeSummary(11);

    const auto Grew = Get_SummaryDelta(Previous, Current);

    TestTrue(TEXT("A computed delta always has a previous to compare against"), Grew.HasPrevious);

    TestEqual(TEXT("Actors"), Grew.ActorCountDelta, static_cast<int64>(1));
    TestEqual(TEXT("Mesh usages"), Grew.StaticMeshUsageCountDelta, static_cast<int64>(2));
    TestEqual(TEXT("Unique meshes"), Grew.UniqueStaticMeshCountDelta, static_cast<int64>(3));
    TestEqual(TEXT("Materials"), Grew.UniqueMaterialCountDelta, static_cast<int64>(4));
    TestEqual(TEXT("Textures"), Grew.UniqueTextureCountDelta, static_cast<int64>(5));
    TestEqual(TEXT("LOD0 triangles"), Grew.Lod0TriangleTotalDelta, static_cast<int64>(6));
    TestEqual(TEXT("Lights"), Grew.LightCountDelta, static_cast<int64>(7));
    TestEqual(TEXT("Critical findings"), Grew.CriticalCountDelta, static_cast<int64>(8));
    TestEqual(TEXT("Major findings"), Grew.MajorCountDelta, static_cast<int64>(9));
    TestEqual(TEXT("Minor findings"), Grew.MinorCountDelta, static_cast<int64>(10));
    TestEqual(TEXT("Total findings is the sum of the three"), Grew.FindingTotalDelta, static_cast<int64>(27));
    TestEqual(TEXT("Disk bytes"), Grew.DiskTotalBytesDelta, static_cast<int64>(11));

    // Signed, both ways — the sign IS the direction, so reversing the arguments must reverse every field.
    const auto Shrank = Get_SummaryDelta(Current, Previous);

    TestEqual(TEXT("Reversed actors"), Shrank.ActorCountDelta, static_cast<int64>(-1));
    TestEqual(TEXT("Reversed triangles"), Shrank.Lod0TriangleTotalDelta, static_cast<int64>(-6));
    TestEqual(TEXT("Reversed findings"), Shrank.FindingTotalDelta, static_cast<int64>(-27));

    const auto Same = Get_SummaryDelta(Previous, Previous);
    TestEqual(TEXT("A scan against itself moves nothing"), Same.FindingTotalDelta, static_cast<int64>(0));

    // ---- The model's rotation ----
    auto Model = FCkOptimizationDebugger_Model{};

    TestFalse(TEXT("A fresh model has no census"), Model.Get_HasSummary());
    TestFalse(TEXT("...and therefore nothing to compare against"), Model.Get_SummaryDelta().HasPrevious);

    Model.Set_Summary(Previous);

    TestTrue(TEXT("One scan produces a census"), Model.Get_HasSummary());
    TestFalse(TEXT("One scan is still not a comparison"), Model.Get_HasPreviousSummary());
    TestFalse(TEXT("...so the delta stays unset rather than reading as zero change"),
        Model.Get_SummaryDelta().HasPrevious);

    Model.Set_Summary(Current);

    TestTrue(TEXT("The second scan rotates the first into place"), Model.Get_HasPreviousSummary());
    TestEqual(TEXT("The previous census is the one that was replaced"),
        Model.Get_PreviousSummary().ActorCount, Previous.ActorCount);
    TestEqual(TEXT("The current census is the one just stored"),
        Model.Get_Summary().ActorCount, Current.ActorCount);
    TestEqual(TEXT("The model's delta is the pure projection's"),
        Model.Get_SummaryDelta().ActorCountDelta, static_cast<int64>(1));

    // A scan that cleared the window must not leave a delta pointing at a world that no longer exists.
    Model.Reset();

    TestFalse(TEXT("Reset drops the census"), Model.Get_HasSummary());
    TestFalse(TEXT("Reset drops the comparison too"), Model.Get_SummaryDelta().HasPrevious);

    // ---- Delta tone ----
    TestEqual(TEXT("Fewer findings is an improvement"),
        static_cast<int32>(Get_DeltaTone(-3, true)), static_cast<int32>(ECk_Tone::Ok));
    TestEqual(TEXT("More findings is a regression"),
        static_cast<int32>(Get_DeltaTone(3, true)), static_cast<int32>(ECk_Tone::Err));
    TestEqual(TEXT("No change is neither"),
        static_cast<int32>(Get_DeltaTone(0, true)), static_cast<int32>(ECk_Tone::Neutral));

    // A count with no better direction is never painted — a level that grew is not a defect.
    TestEqual(TEXT("A directionless stat going up is not coloured"),
        static_cast<int32>(Get_DeltaTone(3, false)), static_cast<int32>(ECk_Tone::Neutral));
    TestEqual(TEXT("A directionless stat going down is not coloured either"),
        static_cast<int32>(Get_DeltaTone(-3, false)), static_cast<int32>(ECk_Tone::Neutral));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Dashboard_LevelExclusion,
    "Ck.OptimizationDebugger.Dashboard.LevelExclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Dashboard_LevelExclusion::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;

    const auto Persistent = FName{TEXT("Gym_Persistent")};
    const auto SubLevel   = FName{TEXT("Gym_Combat")};

    // ---- The seam the scan asks ----
    auto Excluded = TSet<FName>{};

    TestTrue(TEXT("An empty exclusion set scans everything"), ShouldScanLevel(Persistent, Excluded));
    TestTrue(TEXT("...including the sub-level"), ShouldScanLevel(SubLevel, Excluded));

    Excluded.Add(SubLevel);

    TestFalse(TEXT("An excluded level is not scanned"), ShouldScanLevel(SubLevel, Excluded));
    TestTrue(TEXT("...and its neighbour is unaffected"), ShouldScanLevel(Persistent, Excluded));

    // A level with no name cannot be named in the exclusion set, so refusing to scan it would hide it with no row
    // left to switch back on.
    TestTrue(TEXT("A nameless level is always in scope"), ShouldScanLevel(NAME_None, Excluded));

    // ---- The state the toggles drive ----
    auto Model = FCkOptimizationDebugger_Model{};

    TestFalse(TEXT("A fresh model excludes nothing"), Model.Get_LevelExcluded(SubLevel));

    Model.Set_LevelExcluded(SubLevel, true);

    TestTrue(TEXT("A switched-off level is excluded"), Model.Get_LevelExcluded(SubLevel));
    TestFalse(TEXT("...and only that one"), Model.Get_LevelExcluded(Persistent));
    TestEqual(TEXT("The set the scan is handed holds exactly it"), Model.Get_ExcludedLevelNames().Num(), 1);
    TestFalse(TEXT("The scan agrees with the model"),
        ShouldScanLevel(SubLevel, Model.Get_ExcludedLevelNames()));

    // Idempotent: a re-render that re-applied the same state must not grow the set.
    Model.Set_LevelExcluded(SubLevel, true);
    TestEqual(TEXT("Excluding twice is excluding once"), Model.Get_ExcludedLevelNames().Num(), 1);

    Model.Set_LevelExcluded(SubLevel, false);
    TestFalse(TEXT("Switching it back on puts it in scope"), Model.Get_LevelExcluded(SubLevel));
    TestEqual(TEXT("...and empties the set"), Model.Get_ExcludedLevelNames().Num(), 0);

    // The exclusion set is the user's narrowing of the QUESTION, exactly like the filter state — a PIE boundary
    // clears the answers, never the question.
    Model.Set_LevelExcluded(SubLevel, true);
    Model.Reset();

    TestTrue(TEXT("Reset keeps the scope the user narrowed to"), Model.Get_LevelExcluded(SubLevel));

    Model.Set_ExcludedLevelNames(TSet<FName>{TArray<FName>{Persistent}});

    TestTrue(TEXT("A restored set replaces the old one"), Model.Get_LevelExcluded(Persistent));
    TestFalse(TEXT("...wholesale, not by merge"), Model.Get_LevelExcluded(SubLevel));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Dashboard_SizeFormatting,
    "Ck.OptimizationDebugger.Dashboard.SizeFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Dashboard_SizeFormatting::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;

    // ---- Bytes. Binary units, because that is how the reader's file browser counts. ----
    TestEqual(TEXT("Zero"), Format_ByteSize(0), FString{TEXT("0 B")});
    TestEqual(TEXT("Bytes print whole"), Format_ByteSize(512), FString{TEXT("512 B")});
    TestEqual(TEXT("A kilobyte is 1024 bytes"), Format_ByteSize(1024), FString{TEXT("1.0 KB")});
    TestEqual(TEXT("Just under a megabyte stays in kilobytes"), Format_ByteSize(1048575), FString{TEXT("1024.0 KB")});
    TestEqual(TEXT("A megabyte"), Format_ByteSize(1048576), FString{TEXT("1.0 MB")});
    TestEqual(TEXT("A gigabyte"), Format_ByteSize(1073741824), FString{TEXT("1.0 GB")});
    TestEqual(TEXT("A terabyte"), Format_ByteSize(int64{1099511627776}), FString{TEXT("1.0 TB")});

    // The sign is kept rather than clamped, so a byte DELTA prints through the same function.
    TestEqual(TEXT("A negative size keeps its sign"), Format_ByteSize(-2048), FString{TEXT("-2.0 KB")});

    // ---- Counts. Decimal, because a triangle count is not a memory size. ----
    TestEqual(TEXT("Small counts print as themselves"), Format_AbbreviatedCount(947), FString{TEXT("947")});
    TestEqual(TEXT("A thousand"), Format_AbbreviatedCount(1000), FString{TEXT("1.0K")});
    TestEqual(TEXT("Twelve and a bit thousand"), Format_AbbreviatedCount(12345), FString{TEXT("12.3K")});
    TestEqual(TEXT("A million and a bit"), Format_AbbreviatedCount(1234567), FString{TEXT("1.2M")});
    TestEqual(TEXT("Billions"), Format_AbbreviatedCount(int64{2500000000}), FString{TEXT("2.5B")});
    TestEqual(TEXT("Zero"), Format_AbbreviatedCount(0), FString{TEXT("0")});

    // ---- Deltas ----
    TestEqual(TEXT("An unchanged stat reads as an em dash, not +0"),
        Format_Delta(0), FString{TEXT("—")});
    TestEqual(TEXT("Growth is explicitly signed"), Format_Delta(12), FString{TEXT("+12")});
    TestEqual(TEXT("Shrinkage is explicitly signed"), Format_Delta(-3), FString{TEXT("-3")});
    TestEqual(TEXT("A large delta abbreviates like the value above it"),
        Format_Delta(1234567), FString{TEXT("+1.2M")});

    // ---- Fractions ----
    TestEqual(TEXT("Half of the total"), Get_DiskCategoryFraction(50, 100), 0.5f);
    TestEqual(TEXT("All of the total"), Get_DiskCategoryFraction(100, 100), 1.0f);
    TestEqual(TEXT("An empty project's meter is empty, not full"), Get_DiskCategoryFraction(10, 0), 0.0f);
    TestEqual(TEXT("A negative total cannot fill a meter either"), Get_DiskCategoryFraction(10, -5), 0.0f);
    TestEqual(TEXT("A bucket bigger than its total clamps"), Get_DiskCategoryFraction(200, 100), 1.0f);

    // ---- Disk category labels ----
    const auto Categories = Get_AllDiskCategories();

    TestEqual(TEXT("Every disk category is listed"), Categories.Num(), k_DiskCategoryCount);

    auto SeenLabels = TSet<FString>{};

    for (const auto Category : Categories)
    {
        const auto Label = Get_DiskCategoryLabel(Category);

        TestFalse(ck::Format_UE(TEXT("Category {} has a label"), static_cast<int32>(Category)), Label.IsEmpty());
        SeenLabels.Add(Label);
    }

    // Two families sharing a label would put two rows under one name and no way to tell which was which.
    TestEqual(TEXT("Every disk category label is distinct"), SeenLabels.Num(), Categories.Num());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Dashboard_SeveritySolo,
    "Ck.OptimizationDebugger.Dashboard.SeveritySolo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Dashboard_SeveritySolo::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_dashboard_spec;
    using namespace ck_optimization_debugger_model;

    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({
        MakeFinding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Critical, TEXT("SM_Boulder")),
        MakeFinding(TEXT("Mesh.MissingLods"), ECkOptimizationDebugger_Severity::Major, TEXT("SM_Crate")),
        MakeFinding(TEXT("Mesh.NaniteCandidate"), ECkOptimizationDebugger_Severity::Minor, TEXT("SM_Rock")),
    });

    TestEqual(TEXT("Nothing is hidden before a solo"), Model.Get_VisibleFindingCount(), 3);

    Model.Set_SeveritySolo(ECkOptimizationDebugger_Severity::Major);

    TestEqual(TEXT("A solo leaves exactly that severity visible"), Model.Get_VisibleFindingCount(), 1);
    TestTrue(TEXT("...the one that was solo'd"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Major));
    TestFalse(TEXT("...and not the more severe one"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Critical));
    TestFalse(TEXT("...nor the less severe one"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Minor));

    TestEqual(TEXT("The visible counts follow"),
        Model.Get_VisibleCountsBySeverity().MajorCount, 1);
    TestEqual(TEXT("...and report nothing for the hidden ones"),
        Model.Get_VisibleCountsBySeverity().CriticalCount, 0);

    // The DASHBOARD headline is the whole scan and must not move when a page filter does — a filter making the
    // headline drop would read as a level that got better.
    TestEqual(TEXT("The unfiltered count is unaffected by a solo"),
        Model.Get_CountsBySeverity().Get_Total(), 3);

    // An assignment, never a toggle: a second click on the same badge must leave the answer standing rather than
    // emptying the list.
    Model.Set_SeveritySolo(ECkOptimizationDebugger_Severity::Major);
    TestEqual(TEXT("Soloing the same severity twice is still that severity"), Model.Get_VisibleFindingCount(), 1);

    Model.Set_SeveritySolo(ECkOptimizationDebugger_Severity::Critical);
    TestEqual(TEXT("Soloing another severity replaces the first"), Model.Get_VisibleFindingCount(), 1);
    TestTrue(TEXT("...with the new one"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Critical));
    TestFalse(TEXT("...and drops the old one"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Major));

    // The severity toggles in the chrome are what widens it again, so a solo must be reversible through them.
    for (const auto Severity : Get_AllSeverities())
    { Model.Set_SeverityVisible(Severity, true); }

    TestEqual(TEXT("Re-enabling every severity restores the whole list"), Model.Get_VisibleFindingCount(), 3);
    TestEqual(TEXT("...and the mask is back to the no-filtering value"),
        static_cast<int32>(Model.Get_Filter().SeverityMask), static_cast<int32>(k_AllSeverityMask));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
