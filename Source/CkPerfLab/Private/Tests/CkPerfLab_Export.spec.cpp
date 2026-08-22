#include "CkPerfLab/Export/CkPerfLab_Export.h"

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"
#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include <Algo/Reverse.h>
#include <Misc/AutomationTest.h>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_export_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    constexpr auto k_BudgetMs = 16.67f;

    auto
        Make_Metrics(
            float InFrameMs,
            bool InGpuAvailable)
        -> FCk_PerfLab_MetricSet
    {
        auto Samples = TArray<float>{};
        Samples.Init(InFrameMs, 20);

        // A deliberate spike, so p99 and worst differ from the average and the export has real
        // spread to format rather than five copies of one number.
        Samples.Add(InFrameMs * 2.0f);

        const auto Stats = ck::perf_lab::Reduce_Samples(Samples, 3.5f);

        auto Metrics = FCk_PerfLab_MetricSet{};

        Metrics.Set_Frame(Stats);
        Metrics.Set_GameThread(Stats);
        Metrics.Set_RenderThread(Stats);
        Metrics.Set_RhiThread(Stats);
        Metrics.Set_Gpu(InGpuAvailable
            ? Stats
            : ck::perf_lab::Make_UnavailableStats(
                  ECk_Stats_MetricAvailability::Unavailable_NullRhi, TEXT("headless")));

        return Metrics;
    }

    auto
        Make_Position(
            const FString& InId,
            const FVector& InLocation,
            float InFrameMs,
            bool InGpuAvailable)
        -> FCk_PerfLab_PositionResult
    {
        const auto Metrics = Make_Metrics(InFrameMs, InGpuAvailable);

        return FCk_PerfLab_PositionResult{}
            .Set_PositionId(InId)
            .Set_Location(InLocation)
            .Set_EyeHeightCm(160.0f)
            .Set_Aggregate(Metrics)
            .Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(ECk_PerfLab_Confidence::High))
            .Set_Directions(
            {
                FCk_PerfLab_DirectionResult{}.Set_YawDegrees(0.0f).Set_Metrics(Metrics),
                FCk_PerfLab_DirectionResult{}.Set_YawDegrees(180.0f).Set_Metrics(Metrics),
            })
            .Set_ActorCensus(
            {
                FCk_PerfLab_ActorCensusRow{}
                    .Set_ObjectPath(TEXT("/Game/Maps/Test.Test:PersistentLevel.Mesh_0"))
                    .Set_ClassName(TEXT("StaticMeshActor"))
                    .Set_DistanceCm(420.0f)
                    .Set_TriangleCount(250000),
            });
    }

    /** A session carrying the characters an export has to survive: quotes, commas, angle brackets. */
    auto
        Make_Session()
        -> FCk_PerfLab_Session
    {
        return FCk_PerfLab_Session{}
            .Set_SessionId(TEXT("session-a"))
            .Set_State(ECk_PerfLab_RunState::Done)
            .Set_Request(FCk_PerfLab_Request{}
                .Set_SessionId(TEXT("session-a"))
                .Set_MapPath(TEXT("/Game/Maps/Test.Test"))
                .Set_BudgetMs(k_BudgetMs)
                .Set_Seed(1234))
            .Set_Environment(FCk_PerfLab_Environment{}
                .Set_MachineName(TEXT("DESK, \"TOP\" <1>"))
                .Set_CpuBrand(TEXT("Test CPU"))
                .Set_GpuBrand(TEXT("Test GPU"))
                .Set_RhiName(TEXT("D3D12"))
                .Set_EngineVersion(TEXT("5.6.0"))
                .Set_BuildConfiguration(TEXT("Development"))
                .Set_InstanceMode(TEXT("Game"))
                .Set_ViewportSizeActual(FIntPoint{1280, 720}))
            .Set_Positions(
            {
                Make_Position(TEXT("p_slow"), FVector{1000.0, 0.0, 0.0}, 50.0f, true),
                Make_Position(TEXT("p_fast"), FVector{0.0, 0.0, 0.0},    5.0f,  true),
            });
    }

    auto
        Make_Analysis(
            const FCk_PerfLab_Session& InSession)
        -> FCk_PerfLab_Analysis
    {
        return ck::perf_lab::Analyse_Session(InSession, k_BudgetMs);
    }

    /** A fixed instant, so nothing in a determinism assertion depends on when the test ran. */
    auto
        Get_FixedTimestamp()
        -> FDateTime
    {
        return FDateTime{2026, 8, 22, 13, 45, 0};
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Export_IsByteIdenticalAcrossBuilds,
    "Ck.PerfLab.Export.Determinism",
    ck_perf_lab_export_spec::kFlags)

bool FCkPerfLab_Export_IsByteIdenticalAcrossBuilds::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_export_spec;

    const auto Session   = Make_Session();
    const auto Analysis  = Make_Analysis(Session);
    const auto Timestamp = Get_FixedTimestamp();

    // Same input, same timestamp, same bytes. This is what makes two exports diffable — and without
    // it, "the report changed" stops being evidence that anything about the level changed.
    TestEqual(TEXT("HTML is byte-identical across builds"),
        ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, Timestamp),
        ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, Timestamp));

    TestEqual(TEXT("The CSV bundle is byte-identical across builds"),
        ck::perf_lab::exporter::Build_SessionCsvBundle(Session, Analysis),
        ck::perf_lab::exporter::Build_SessionCsvBundle(Session, Analysis));

    TestEqual(TEXT("JSON is byte-identical across builds"),
        ck::perf_lab::exporter::Build_SessionJson(Session, Analysis, Timestamp),
        ck::perf_lab::exporter::Build_SessionJson(Session, Analysis, Timestamp));

    // The order positions arrive in must not reach the file. A session whose positions were listed
    // the other way round is the same session, and its export has to say so.
    auto Reversed = Session;
    auto Positions = Reversed.Get_Positions();

    Algo::Reverse(Positions);
    Reversed.Set_Positions(Positions);

    TestEqual(TEXT("Incoming position order does not reach the HTML"),
        ck::perf_lab::exporter::Build_SessionHtml(Reversed, Make_Analysis(Reversed), Timestamp),
        ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, Timestamp));

    TestEqual(TEXT("Incoming position order does not reach the CSV"),
        ck::perf_lab::exporter::Build_SessionCsvBundle(Reversed, Make_Analysis(Reversed)),
        ck::perf_lab::exporter::Build_SessionCsvBundle(Session, Analysis));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Export_CarriesItsLimitationsAndDisclaimer,
    "Ck.PerfLab.Export.CarriesItsLimitationsAndDisclaimer",
    ck_perf_lab_export_spec::kFlags)

bool FCkPerfLab_Export_CarriesItsLimitationsAndDisclaimer::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_export_spec;

    const auto Session  = Make_Session();
    const auto Analysis = Make_Analysis(Session);

    const auto Html = ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, Get_FixedTimestamp());
    const auto Json = ck::perf_lab::exporter::Build_SessionJson(Session, Analysis, Get_FixedTimestamp());

    // The exported file is the copy most likely to be read by somebody who never ran the tool, and
    // therefore the copy least able to afford implying a contributor was measured to be the cause.
    TestTrue(TEXT("The HTML carries the limitation paragraph"),
        Html.Contains(TEXT("worth investigating, not proven to be the cause")));

    TestTrue(TEXT("The HTML says the camera was stationary"),
        Html.Contains(TEXT("stationary")));

    TestTrue(TEXT("The JSON carries the limitations text"),
        Json.Contains(TEXT("limitations")));

    // Contributors carry the disclaimer in the ROW, so it survives a reader filtering or re-sorting
    // the sheet away from any header that carried it.
    const auto Contributors = ck::perf_lab::exporter::Build_SessionCsv(
        Session, Analysis, ck::perf_lab::exporter::ECk_CsvTable::Contributors);

    TestTrue(TEXT("The contributors CSV declares a disclaimer column"),
        Contributors.Contains(TEXT("disclaimer")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Export_NeverPrintsAnUnmeasuredMetricAsZero,
    "Ck.PerfLab.Export.NeverPrintsAnUnmeasuredMetricAsZero",
    ck_perf_lab_export_spec::kFlags)

bool FCkPerfLab_Export_NeverPrintsAnUnmeasuredMetricAsZero::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_export_spec;

    auto Session = Make_Session();

    Session.Set_Positions({Make_Position(TEXT("p_headless"), FVector::ZeroVector, 20.0f, false)});

    const auto Analysis = Make_Analysis(Session);

    const auto Html = ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, Get_FixedTimestamp());
    const auto Csv  = ck::perf_lab::exporter::Build_SessionCsv(
        Session, Analysis, ck::perf_lab::exporter::ECk_CsvTable::Positions);

    // A headless run measures no GPU time. Printing 0.000 would read as the fastest GPU in the
    // project rather than as an absent reading, and it would do so in the file that outlives the
    // session everywhere else.
    TestTrue(TEXT("The HTML says a GPU metric was not measured"), Html.Contains(TEXT("not measured")));
    TestTrue(TEXT("The CSV says the same"),                       Csv.Contains(TEXT("not measured")));

    // The derived columns too, not just the averages. A review found worst / p99 / 1%-low bypassing
    // the availability gate and printing 0.000: sort that spreadsheet ascending on frameP99Ms and
    // every unmeasured position rises to the top as the fastest place in the level.
    const auto Absent = ck::perf_lab::Make_UnavailableStats(
        ECk_Stats_MetricAvailability::Unavailable_NotYetSampled, TEXT("never sampled"));

    // Every metric explicitly unavailable — a DEFAULT-constructed set reads as Available with zeros,
    // which is a separate known defect and would make this spec pass for the wrong reason.
    auto Dark = FCk_PerfLab_MetricSet{};

    Dark.Set_Frame(Absent);
    Dark.Set_GameThread(Absent);
    Dark.Set_RenderThread(Absent);
    Dark.Set_RhiThread(Absent);
    Dark.Set_Gpu(Absent);

    auto Unmeasured = Make_Session();

    Unmeasured.Set_Positions({FCk_PerfLab_PositionResult{}
        .Set_PositionId(TEXT("p_dark"))
        // Off the origin, so the location columns cannot themselves emit the zero being asserted on.
        .Set_Location(FVector{1234.5, 678.25, 90.125})
        .Set_Aggregate(Dark)
        .Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(ECk_PerfLab_Confidence::Low))});

    const auto DarkCsv = ck::perf_lab::exporter::Build_SessionCsv(
        Unmeasured, Make_Analysis(Unmeasured), ck::perf_lab::exporter::ECk_CsvTable::Positions);

    TestFalse(TEXT("An unmeasured position never prints a zero timing"),
        DarkCsv.Contains(TEXT("\"0.000\"")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Export_CsvQuotesEveryCell,
    "Ck.PerfLab.Export.CsvQuotesEveryCell",
    ck_perf_lab_export_spec::kFlags)

bool FCkPerfLab_Export_CsvQuotesEveryCell::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_export_spec;

    const auto Session  = Make_Session();
    const auto Analysis = Make_Analysis(Session);

    const auto SessionInfo = ck::perf_lab::exporter::Build_SessionCsv(
        Session, Analysis, ck::perf_lab::exporter::ECk_CsvTable::SessionInfo);

    // The machine name in the fixture carries a comma and an embedded quote on purpose. An
    // unescaped one silently shifts every column after it, which produces a file that opens fine
    // and is wrong in a way nobody notices.
    TestTrue(TEXT("An embedded quote is doubled"), SessionInfo.Contains(TEXT("\"\"TOP\"\"")));
    TestTrue(TEXT("Header cells are quoted"),      SessionInfo.Contains(TEXT("\"key\",\"value\"")));

    // Every table exists and names itself in the bundle, so a consumer can find the one it wants.
    const auto Bundle = ck::perf_lab::exporter::Build_SessionCsvBundle(Session, Analysis);

    for (const auto Table : ck::perf_lab::exporter::Get_AllCsvTables())
    {
        TestTrue(*FString::Printf(TEXT("The bundle carries the %s section"),
                *ck::perf_lab::exporter::Get_CsvTableName(Table)),
            Bundle.Contains(ck::Format_UE(TEXT("# {}"), ck::perf_lab::exporter::Get_CsvTableName(Table))));
    }

    TestEqual(TEXT("Six tables, no more and no fewer"),
        ck::perf_lab::exporter::Get_AllCsvTables().Num(), 6);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Export_JsonRemainsTheSessionSchemaPlusAnalysis,
    "Ck.PerfLab.Export.JsonRemainsTheSessionSchemaPlusAnalysis",
    ck_perf_lab_export_spec::kFlags)

bool FCkPerfLab_Export_JsonRemainsTheSessionSchemaPlusAnalysis::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_export_spec;

    const auto Session  = Make_Session();
    const auto Analysis = Make_Analysis(Session);

    const auto Json = ck::perf_lab::exporter::Build_SessionJson(Session, Analysis, Get_FixedTimestamp());

    // The export is the SESSION file with an analysis block attached, never a second serialisation
    // of the same shape — so the codec that wrote it must still be able to read it back. Two writers
    // of one schema drift, and the drift surfaces as an export the reader silently mis-parses.
    auto RoundTripped = FCk_PerfLab_Session{};

    TestTrue(TEXT("The exported JSON still decodes through the session codec"),
        ck::perf_lab::TryRead_SessionFromJson(Json, RoundTripped));

    TestEqual(TEXT("...with the session id intact"),
        RoundTripped.Get_SessionId(), Session.Get_SessionId());

    TestEqual(TEXT("...and every position"),
        RoundTripped.Get_Positions().Num(), Session.Get_Positions().Num());

    // Shape and vocabulary per SCHEMA.md §3.1 and §3.2: the score is an object carrying its own
    // components and formula, and every enum is written camelCase from the closed set rather than
    // from a display name that a rename could change underneath a consumer.
    TestTrue(TEXT("The analysis block is present"),   Json.Contains(TEXT("\"analysis\"")));
    TestTrue(TEXT("...stamped with when it was run"), Json.Contains(TEXT("\"analysedUtc\"")));
    TestTrue(TEXT("...how many components counted"),  Json.Contains(TEXT("\"componentsUsed\"")));
    // Case-SENSITIVE on purpose: FString::Contains ignores case by default, so the obvious spelling
    // of this assertion would pass against the PascalCase spelling it exists to forbid.
    TestTrue(TEXT("...its components by camelCase key"),
        Json.Contains(TEXT("\"averageFrameAttainment\""), ESearchCase::CaseSensitive));

    TestFalse(TEXT("Component keys are never PascalCase"),
        Json.Contains(TEXT("\"AverageFrameAttainment\""), ESearchCase::CaseSensitive));

    TestTrue(TEXT("...carrying the score"),         Json.Contains(TEXT("\"score\"")));
    TestTrue(TEXT("...and its formula"),            Json.Contains(TEXT("\"formula\"")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
