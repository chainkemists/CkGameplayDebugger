#include "CkPerfLab/Heatmap/CkPerfLab_HeatmapSlot.h"

#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_heatmap_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_Position(
            const FString& InId,
            const FVector& InLocation,
            float InFrameMs,
            bool InMeasured)
        -> FCk_PerfLab_PositionResult
    {
        auto Samples = TArray<float>{};
        Samples.Init(InFrameMs, 20);

        auto Metrics = FCk_PerfLab_MetricSet{};
        Metrics.Set_Frame(InMeasured
            ? ck::perf_lab::Reduce_Samples(Samples, 3.5f)
            : ck::perf_lab::Make_UnavailableStats(
                ECk_Stats_MetricAvailability::Unavailable_NullRhi, TEXT("headless")));

        return FCk_PerfLab_PositionResult{}
            .Set_PositionId(InId)
            .Set_Location(InLocation)
            .Set_Aggregate(Metrics)
            .Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(ECk_PerfLab_Confidence::High));
    }

    auto
        Make_Session()
        -> FCk_PerfLab_Session
    {
        return FCk_PerfLab_Session{}
            .Set_Request(FCk_PerfLab_Request{}.Set_MapPath(TEXT("/Game/Maps/Test.Test")))
            .Set_Positions(
            {
                Make_Position(TEXT("p_fast"), FVector{0.0, 0.0, 0.0},       5.0f,  true),
                Make_Position(TEXT("p_slow"), FVector{1000.0, 0.0, 0.0},    50.0f, true),
                Make_Position(TEXT("p_dark"), FVector{2000.0, 0.0, 0.0},    0.0f,  false),
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Heatmap_NormalisesAgainstTheWorstInThisSession,
    "Ck.PerfLab.Heatmap.NormalisesAgainstTheWorstInThisSession",
    ck_perf_lab_heatmap_spec::kFlags)

bool FCkPerfLab_Heatmap_NormalisesAgainstTheWorstInThisSession::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_heatmap_spec;

    const auto Session  = Make_Session();
    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);
    const auto Snapshot = ck::perf_lab::heatmap::Build_Snapshot(Session, Analysis);

    // The ramp is relative to this level, not to an absolute scale — otherwise every level in a project paints the
    // same colour and the overlay stops answering "where in HERE do I look first".
    const auto Slow = ck::algo::FindIf(Snapshot._Markers,
        [](const ck::perf_lab::heatmap::FCk_Marker& InMarker) { return InMarker._PositionId == TEXT("p_slow"); });

    const auto Fast = ck::algo::FindIf(Snapshot._Markers,
        [](const ck::perf_lab::heatmap::FCk_Marker& InMarker) { return InMarker._PositionId == TEXT("p_fast"); });

    TestTrue(TEXT("The worst position is present"), Slow.IsSet());
    TestTrue(TEXT("The fast position is present"),  Fast.IsSet());

    if (Slow.IsSet() && Fast.IsSet())
    {
        TestEqual(TEXT("The worst position sits at the top of the ramp"), Slow->_Normalised, 1.0f, 1e-3f);
        TestTrue (TEXT("A fast position sits well below it"),             Fast->_Normalised < 0.25f);

        // Size reads the over-budget multiple, so a position inside budget is never inflated.
        TestTrue (TEXT("The worst position is over budget"),   Slow->_OverBudgetRatio > 1.0f);
        TestTrue (TEXT("The fast position is inside budget"),  Fast->_OverBudgetRatio < 1.0f);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Heatmap_UnmeasuredPositionsAreNotDrawn,
    "Ck.PerfLab.Heatmap.UnmeasuredPositionsAreNotDrawn",
    ck_perf_lab_heatmap_spec::kFlags)

bool FCkPerfLab_Heatmap_UnmeasuredPositionsAreNotDrawn::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_heatmap_spec;

    const auto Session  = Make_Session();
    const auto Snapshot = ck::perf_lab::heatmap::Build_Snapshot(Session, ck::perf_lab::Analyse_Session(Session, 16.67f));

    // A position whose frame time was never measured gets no marker at all. Drawing it in the cool colour would
    // claim a reading that does not exist, which is the same zero-as-data mistake in visual form.
    TestEqual(TEXT("Only measured positions produce markers"), Snapshot._Markers.Num(), 2);

    TestFalse(TEXT("The unmeasured position is absent"),
        ck::algo::AnyOf(Snapshot._Markers,
            [](const ck::perf_lab::heatmap::FCk_Marker& InMarker) { return InMarker._PositionId == TEXT("p_dark"); }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Heatmap_CarriesItsMapAndLegend,
    "Ck.PerfLab.Heatmap.CarriesItsMapAndLegend",
    ck_perf_lab_heatmap_spec::kFlags)

bool FCkPerfLab_Heatmap_CarriesItsMapAndLegend::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_heatmap_spec;

    const auto Session  = Make_Session();
    const auto Snapshot = ck::perf_lab::heatmap::Build_Snapshot(Session, ck::perf_lab::Analyse_Session(Session, 16.67f));

    // The map path is what lets the EdMode refuse to draw a session captured against a different level — markers
    // placed by another level's coordinates would look exactly as authoritative as real ones.
    TestEqual(TEXT("The snapshot names the map it was measured against"),
        Snapshot._MapPath, TEXT("/Game/Maps/Test.Test"));

    // Every encoding axis is described. A lens without a legend is a picture nobody can read.
    TestFalse(TEXT("The snapshot carries a legend"), Snapshot._Legend.IsEmpty());
    TestTrue (TEXT("The legend explains colour"),    Snapshot._Legend.Contains(TEXT("colour")));
    TestTrue (TEXT("The legend explains size"),      Snapshot._Legend.Contains(TEXT("size")));
    TestTrue (TEXT("The legend explains shape"),     Snapshot._Legend.Contains(TEXT("shape")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Heatmap_SlotRoundTripsAndClears,
    "Ck.PerfLab.Heatmap.SlotRoundTripsAndClears",
    ck_perf_lab_heatmap_spec::kFlags)

bool FCkPerfLab_Heatmap_SlotRoundTripsAndClears::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_heatmap_spec;

    const auto Session = Make_Session();

    ck::perf_lab::heatmap::Publish(
        ck::perf_lab::heatmap::Build_Snapshot(Session, ck::perf_lab::Analyse_Session(Session, 16.67f)));

    TestTrue (TEXT("A published snapshot reads back enabled"), ck::perf_lab::heatmap::Get_Published()._IsEnabled);
    TestEqual(TEXT("...with its markers"), ck::perf_lab::heatmap::Get_Published()._Markers.Num(), 2);

    // Clearing must actually stop the drawing rather than leaving stale markers over a level the reader has since
    // changed.
    ck::perf_lab::heatmap::Clear();

    TestFalse(TEXT("A cleared slot draws nothing"), ck::perf_lab::heatmap::Get_Published()._IsEnabled);
    TestEqual(TEXT("...and holds no markers"), ck::perf_lab::heatmap::Get_Published()._Markers.Num(), 0);

    ck::perf_lab::heatmap::Set_SelectedPositionId(TEXT("p_slow"));
    TestEqual(TEXT("A viewport click round-trips through the slot"),
        ck::perf_lab::heatmap::Get_SelectedPositionId(), TEXT("p_slow"));

    ck::perf_lab::heatmap::Set_SelectedPositionId(FString{});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
