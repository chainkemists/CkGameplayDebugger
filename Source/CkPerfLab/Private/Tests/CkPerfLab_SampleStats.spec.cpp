#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_sample_stats_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    // Outliers are what this module is built to survive, so the fixtures are shaped around them
    // rather than around tidy numbers.
    constexpr auto kMadK = 3.5f;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_SampleStats_Empty_IsUnavailableNotZero,
    "Ck.PerfLab.Stats.Empty_IsUnavailableNotZero",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_SampleStats_Empty_IsUnavailableNotZero::RunTest(const FString& Parameters)
{
    const auto Stats = ck::perf_lab::Reduce_Samples({}, ck_perf_lab_sample_stats_spec::kMadK);

    // The whole point: nothing measured must not look like a measurement of zero.
    TestFalse(TEXT("An empty sample set is not available"), Stats.Get_IsAvailable());
    TestEqual(TEXT("An empty sample set reports no samples"), Stats.Get_SampleCount(), 0);
    TestFalse(TEXT("An empty sample set explains itself"), Stats.Get_UnavailableReason().IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_SampleStats_SingleSample_IsItsOwnEveryStatistic,
    "Ck.PerfLab.Stats.SingleSample_IsItsOwnEveryStatistic",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_SampleStats_SingleSample_IsItsOwnEveryStatistic::RunTest(const FString& Parameters)
{
    const auto Stats = ck::perf_lab::Reduce_Samples({12.5f}, ck_perf_lab_sample_stats_spec::kMadK);

    TestTrue (TEXT("A single sample is available"),   Stats.Get_IsAvailable());
    TestEqual(TEXT("Average is the sample"),          Stats.Get_AvgMs(),           12.5f);
    TestEqual(TEXT("Worst is the sample"),            Stats.Get_WorstMs(),         12.5f);
    TestEqual(TEXT("P99 is the sample"),              Stats.Get_P99Ms(),           12.5f);
    TestEqual(TEXT("One percent low is the sample"),  Stats.Get_OnePercentLowMs(), 12.5f);
    TestEqual(TEXT("One sample counted"),             Stats.Get_SampleCount(),     1);
    TestEqual(TEXT("A lone sample is not an outlier"),Stats.Get_OutlierCount(),    0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_SampleStats_IdenticalSamples_HaveNoOutliers,
    "Ck.PerfLab.Stats.IdenticalSamples_HaveNoOutliers",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_SampleStats_IdenticalSamples_HaveNoOutliers::RunTest(const FString& Parameters)
{
    auto Samples = TArray<float>{};
    Samples.Init(10.0f, 50);

    const auto Stats = ck::perf_lab::Reduce_Samples(Samples, ck_perf_lab_sample_stats_spec::kMadK);

    // A zero MAD would make the outlier threshold collapse onto the median and classify every
    // sample as an outlier; this pins the guard against that.
    TestEqual(TEXT("Identical samples produce no outliers"), Stats.Get_OutlierCount(), 0);
    TestEqual(TEXT("Average is the common value"),           Stats.Get_AvgMs(),        10.0f);
    TestEqual(TEXT("Worst is the common value"),             Stats.Get_WorstMs(),      10.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_SampleStats_Outlier_ExcludedFromAverageRetainedInWorst,
    "Ck.PerfLab.Stats.Outlier_ExcludedFromAverageRetainedInWorst",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_SampleStats_Outlier_ExcludedFromAverageRetainedInWorst::RunTest(const FString& Parameters)
{
    // Forty samples at 10ms and one enormous spike. The spike must not move the average, and must
    // absolutely still be visible as the worst case — that asymmetry is the contract.
    auto Samples = TArray<float>{};
    Samples.Init(10.0f, 40);
    Samples.Add(500.0f);

    const auto Stats = ck::perf_lab::Reduce_Samples(Samples, ck_perf_lab_sample_stats_spec::kMadK);

    TestEqual(TEXT("The spike is counted as an outlier"),      Stats.Get_OutlierCount(), 1);
    TestEqual(TEXT("The spike is excluded from the average"),  Stats.Get_AvgMs(),        10.0f);
    TestEqual(TEXT("The spike survives in the worst case"),    Stats.Get_WorstMs(),      500.0f);
    TestEqual(TEXT("Every sample is still counted"),           Stats.Get_SampleCount(),  41);

    // The 1% low retains outliers too, so with 41 samples it is the single worst frame.
    TestEqual(TEXT("The one percent low retains the spike"),   Stats.Get_OnePercentLowMs(), 500.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_SampleStats_AllOutliers_StillProduceAnAverage,
    "Ck.PerfLab.Stats.AllOutliers_StillProduceAnAverage",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_SampleStats_AllOutliers_StillProduceAnAverage::RunTest(const FString& Parameters)
{
    // Two samples far apart: whatever the classifier decides, the average must remain a real
    // number rather than collapsing because the inlier set emptied.
    const auto Stats = ck::perf_lab::Reduce_Samples({1.0f, 1000.0f}, 0.0f);

    TestTrue(TEXT("A degenerate sample set is still available"), Stats.Get_IsAvailable());
    TestTrue(TEXT("The average is a real number"),               Stats.Get_AvgMs() > 0.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Confidence_CleanRun_IsHighWithNoReasons,
    "Ck.PerfLab.Confidence.CleanRun_IsHighWithNoReasons",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_Confidence_CleanRun_IsHighWithNoReasons::RunTest(const FString& Parameters)
{
    auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
    Inputs._SampleCount       = 120;
    Inputs._TargetSampleCount = 120;

    const auto Result = ck::perf_lab::Compute_Confidence(Inputs);

    TestTrue (TEXT("A clean run rates High"), Result.Get_Rating() == ECk_PerfLab_Confidence::High);
    TestEqual(TEXT("A High rating carries no reasons"), Result.Get_Reasons().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Confidence_EachProblem_AddsItsOwnReason,
    "Ck.PerfLab.Confidence.EachProblem_AddsItsOwnReason",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_Confidence_EachProblem_AddsItsOwnReason::RunTest(const FString& Parameters)
{
    {
        auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
        Inputs._SampleCount       = 120;
        Inputs._TargetSampleCount = 120;
        Inputs._WarmupConverged   = false;

        const auto Result = ck::perf_lab::Compute_Confidence(Inputs);

        TestTrue(TEXT("A settle timeout downgrades to Medium"), Result.Get_Rating() == ECk_PerfLab_Confidence::Medium);
        TestTrue(TEXT("A settle timeout names itself"),
            Result.Get_Reasons().Contains(ECk_PerfLab_ConfidenceReason::SettleTimedOut));
    }

    {
        auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
        Inputs._SampleCount       = 120;
        Inputs._TargetSampleCount = 120;
        Inputs._StreamingQuiet    = false;
        Inputs._WarmupConverged   = false;

        const auto Result = ck::perf_lab::Compute_Confidence(Inputs);

        TestTrue(TEXT("Two problems downgrade to Low"), Result.Get_Rating() == ECk_PerfLab_Confidence::Low);
        TestEqual(TEXT("Both problems are named"), Result.Get_Reasons().Num(), 2);
    }

    {
        auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
        Inputs._SampleCount           = 120;
        Inputs._TargetSampleCount     = 120;
        Inputs._StreamingQuiet        = false;
        Inputs._WarmupConverged       = false;
        Inputs._ShaderCompileActivity = true;

        const auto Result = ck::perf_lab::Compute_Confidence(Inputs);

        TestTrue(TEXT("Three problems make the result unusable"),
            Result.Get_Rating() == ECk_PerfLab_Confidence::Unusable);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Confidence_NoSamples_IsUnusableRegardless,
    "Ck.PerfLab.Confidence.NoSamples_IsUnusableRegardless",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_Confidence_NoSamples_IsUnusableRegardless::RunTest(const FString& Parameters)
{
    // Nothing sampled cannot be High no matter how quiet the environment was.
    const auto Result = ck::perf_lab::Compute_Confidence(ck::perf_lab::FCk_ConfidenceInputs{});

    TestTrue(TEXT("A position with no samples is Unusable"), Result.Get_Rating() == ECk_PerfLab_Confidence::Unusable);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Confidence_Reasons_AreOrderStable,
    "Ck.PerfLab.Confidence.Reasons_AreOrderStable",
    ck_perf_lab_sample_stats_spec::kFlags)

bool FCkPerfLab_Confidence_Reasons_AreOrderStable::RunTest(const FString& Parameters)
{
    // Sessions get diffed and compared; an unstable reason order would surface as a phantom change.
    auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
    Inputs._SampleCount       = 120;
    Inputs._TargetSampleCount = 120;
    Inputs._StreamingQuiet    = false;
    Inputs._WarmupConverged   = false;

    const auto First  = ck::perf_lab::Compute_Confidence(Inputs);
    const auto Second = ck::perf_lab::Compute_Confidence(Inputs);

    TestTrue(TEXT("The same inputs produce the same reasons in the same order"),
        ck::algo::Compare(First.Get_Reasons(), Second.Get_Reasons()));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
