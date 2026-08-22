#include "CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_sample_stats
{
    // The worst 1% of a window, but never fewer than one sample: with a 120-sample dwell the exact
    // 1% is 1.2 samples, and rounding that to zero would silently delete the statistic.
    auto
        Get_OnePercentLow(
            const TArray<float>& InSortedAscending)
        -> float
    {
        if (InSortedAscending.IsEmpty())
        {
            return 0.0f;
        }

        const auto Count = FMath::Max(1, FMath::CeilToInt32(InSortedAscending.Num() * 0.01f));

        auto Sum = 0.0;

        for (auto Index = InSortedAscending.Num() - Count; Index < InSortedAscending.Num(); ++Index)
        {
            Sum += static_cast<double>(InSortedAscending[Index]);
        }

        return static_cast<float>(Sum / static_cast<double>(Count));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Reduce_Samples(
            const TArray<float>& InSamples,
            float InMadK)
        -> FCk_PerfLab_MetricStats
    {
        auto Stats = FCk_PerfLab_MetricStats{};

        if (InSamples.IsEmpty())
        {
            return Stats.Set_Availability(ECk_Stats_MetricAvailability::Unavailable_NotYetSampled)
                        .Set_UnavailableReason(TEXT("no samples were accepted"));
        }

        auto Sorted = InSamples;
        ck::algo::Sort(Sorted);

        const auto Median = ck::algo::Median(Sorted);
        const auto Mad    = ck::algo::MedianAbsoluteDeviation(Sorted);

        // MAD is the robust estimator, but it collapses to zero whenever more than half the samples
        // are identical — which is the normal shape of a stable frame-time window, not an edge case.
        // Taken literally that would switch outlier detection off exactly when the data is cleanest,
        // letting a lone 500ms spike sit inside the average unnoticed. The mean absolute deviation
        // does not collapse that way, so it is the fallback; only when BOTH are zero is the window
        // genuinely spreadless, and then there is nothing for any threshold to find.
        const auto RobustSpread = Mad.IsSet() ? *Mad : 0.0;
        const auto Spread       = RobustSpread > 0.0
            ? RobustSpread
            : ck::algo::MeanAbsoluteDeviation(Sorted).Get(0.0);

        const auto Threshold = Spread > 0.0
            ? *Median + (static_cast<double>(InMadK) * Spread)
            : TNumericLimits<double>::Max();

        auto Inliers = TArray<float>{};
        Inliers.Reserve(Sorted.Num());

        auto OutlierCount = 0;

        for (const auto& Sample : Sorted)
        {
            if (static_cast<double>(Sample) > Threshold)
            {
                ++OutlierCount;
                continue;
            }

            Inliers.Add(Sample);
        }

        // Every sample being an outlier would leave nothing to average; fall back to the full set so
        // the average stays a real number rather than becoming unavailable on a technicality.
        const auto& ForAverage = Inliers.IsEmpty() ? Sorted : Inliers;

        const auto Average = ck::algo::Mean(ForAverage);
        const auto P99     = ck::algo::Percentile(Sorted, 0.99);

        return Stats.Set_Availability(ECk_Stats_MetricAvailability::Available)
                    .Set_AvgMs(Average.IsSet() ? static_cast<float>(*Average) : 0.0f)
                    .Set_WorstMs(Sorted.Last())
                    .Set_P99Ms(P99.IsSet() ? static_cast<float>(*P99) : 0.0f)
                    .Set_OnePercentLowMs(ck_perf_lab_sample_stats::Get_OnePercentLow(Sorted))
                    .Set_SampleCount(Sorted.Num())
                    .Set_OutlierCount(OutlierCount);
    }

    auto
        Make_UnavailableStats(
            ECk_Stats_MetricAvailability InAvailability,
            const FString& InReason)
        -> FCk_PerfLab_MetricStats
    {
        return FCk_PerfLab_MetricStats{}
            .Set_Availability(InAvailability)
            .Set_UnavailableReason(InReason);
    }

    auto
        Compute_Confidence(
            const FCk_ConfidenceInputs& InInputs)
        -> FCk_PerfLab_ConfidenceResult
    {
        auto Reasons = TArray<ECk_PerfLab_ConfidenceReason>{};

        if (NOT InInputs._WarmupConverged)
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::SettleTimedOut);
        }

        if (NOT InInputs._StreamingQuiet)
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::StreamingActive);
        }

        if (InInputs._ShaderCompileActivity)
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::ShaderCompileActivity);
        }

        if (InInputs._TargetSampleCount > 0 &&
            InInputs._SampleCount < FMath::CeilToInt32(InInputs._TargetSampleCount * InInputs._MinAcceptableSampleFraction))
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::LowSampleCount);
        }

        if (InInputs._CoefficientOfVariation > InInputs._MaxAcceptableCv)
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::HighVariance);
        }

        if (InInputs._SampleCount > 0 &&
            static_cast<float>(InInputs._OutlierCount) / static_cast<float>(InInputs._SampleCount) > InInputs._MaxAcceptableOutlierFraction)
        {
            Reasons.Add(ECk_PerfLab_ConfidenceReason::OutlierHeavy);
        }

        // Sorted so the reason list is stable across runs; the session file is diffed and compared,
        // and an unstable ordering would show as a spurious change.
        ck::algo::Sort(Reasons);

        const auto Rating = [&]() -> ECk_PerfLab_Confidence
        {
            if (InInputs._SampleCount == 0)
            {
                return ECk_PerfLab_Confidence::Unusable;
            }

            switch (Reasons.Num())
            {
                case 0:  return ECk_PerfLab_Confidence::High;
                case 1:  return ECk_PerfLab_Confidence::Medium;
                case 2:  return ECk_PerfLab_Confidence::Low;
                default: return ECk_PerfLab_Confidence::Unusable;
            }
        }();

        return FCk_PerfLab_ConfidenceResult{}
            .Set_Rating(Rating)
            .Set_Reasons(Reasons);
    }
}

// --------------------------------------------------------------------------------------------------------------------
