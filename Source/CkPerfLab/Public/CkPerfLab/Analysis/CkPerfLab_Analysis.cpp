#include "CkPerfLab_Analysis.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_analysis
{
    // The 1% low is compared against a looser budget than the average: a level that never dips is a
    // level nobody ships, and holding worst-case frames to the same bar as typical ones would make
    // every honest result look broken.
    constexpr auto k_OnePercentLowTolerance = 1.5f;

    struct FCk_WeightedComponent
    {
        ECk_PerfLab_ScoreComponent _Component;
        float                      _Weight;
    };

    // The published weights. They live here as data rather than in the arithmetic so the table in
    // the UI and the exports is the same table the score is computed from.
    auto
        Get_ComponentWeights()
        -> TArray<FCk_WeightedComponent>
    {
        return
        {
            {ECk_PerfLab_ScoreComponent::AverageFrameAttainment,  0.25f},
            {ECk_PerfLab_ScoreComponent::WorstCaseAttainment,     0.15f},
            {ECk_PerfLab_ScoreComponent::OnePercentLowAttainment, 0.15f},
            {ECk_PerfLab_ScoreComponent::GameThreadHeadroom,      0.10f},
            {ECk_PerfLab_ScoreComponent::RenderThreadHeadroom,    0.10f},
            {ECk_PerfLab_ScoreComponent::GpuHeadroom,             0.10f},
            {ECk_PerfLab_ScoreComponent::SpatialConsistency,      0.10f},
            {ECk_PerfLab_ScoreComponent::MeasurementConfidence,   0.05f},
        };
    }

    /** Budget attainment as a 0-100 figure: at or under budget is 100, and twice budget is 50. */
    auto
        Get_Attainment(
            float InMeasuredMs,
            float InBudgetMs)
        -> double
    {
        if (InMeasuredMs <= 0.0f || InBudgetMs <= 0.0f)
        {
            return 100.0;
        }

        return FMath::Clamp(100.0 * static_cast<double>(InBudgetMs) / static_cast<double>(InMeasuredMs), 0.0, 100.0);
    }

    /** Positions whose numbers are worth scoring at all. */
    auto
        Get_UsablePositions(
            const FCk_PerfLab_Session& InSession)
        -> TArray<FCk_PerfLab_PositionResult>
    {
        return ck::algo::Filter(InSession.Get_Positions(), [](const FCk_PerfLab_PositionResult& InPosition)
        {
            return InPosition.Get_Confidence().Get_Rating() != ECk_PerfLab_Confidence::Unusable;
        });
    }

    /**
     * The per-position scores for one metric, skipping positions where it was never measured.
     *
     * Filter-and-project in one pass rather than Filter then Transform, because filtering positions
     * would copy each one, and a position owns its census array.
     */
    auto
        Get_MetricValues(
            const TArray<FCk_PerfLab_PositionResult>& InPositions,
            ECk_PerfLab_Metric InMetric,
            TFunctionRef<double(const FCk_PerfLab_MetricStats&)> InProjection)
        -> TArray<double>
    {
        return ck::algo::TransformIf<TArray<double>>(InPositions,
            [&](const FCk_PerfLab_PositionResult& InPosition)
            { return InPosition.Get_Aggregate().Get_ByMetric(InMetric).Get_IsAvailable(); },
            [&](const FCk_PerfLab_PositionResult& InPosition)
            { return InProjection(InPosition.Get_Aggregate().Get_ByMetric(InMetric)); });
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    const TCHAR* const k_ContributorDisclaimer =
        TEXT("near the measured cost — worth investigating, not proven cause");

    auto
        Compute_Score(
            const FCk_PerfLab_Session& InSession,
            float InBudgetMs)
        -> FCk_PerfLab_Score
    {
        using namespace ck_perf_lab_analysis;

        const auto Usable = Get_UsablePositions(InSession);

        auto Results = TArray<FCk_PerfLab_ScoreComponentResult>{};

        // Each component either produces a value or explains why it could not. A component that
        // could not be measured is EXCLUDED and its weight redistributed, never scored as zero —
        // scoring an unmeasured axis as zero would punish a level for the machine's RHI.
        for (const auto& Weighted : Get_ComponentWeights())
        {
            auto Values = TArray<double>{};

            switch (Weighted._Component)
            {
                case ECk_PerfLab_ScoreComponent::AverageFrameAttainment:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::Frame,
                        [&](const FCk_PerfLab_MetricStats& InStats) { return Get_Attainment(InStats.Get_AvgMs(), InBudgetMs); });
                    break;

                case ECk_PerfLab_ScoreComponent::WorstCaseAttainment:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::Frame,
                        [&](const FCk_PerfLab_MetricStats& InStats) { return Get_Attainment(InStats.Get_WorstMs(), InBudgetMs); });
                    break;

                case ECk_PerfLab_ScoreComponent::OnePercentLowAttainment:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::Frame,
                        [&](const FCk_PerfLab_MetricStats& InStats)
                        { return Get_Attainment(InStats.Get_OnePercentLowMs(), InBudgetMs * k_OnePercentLowTolerance); });
                    break;

                case ECk_PerfLab_ScoreComponent::GameThreadHeadroom:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::GameThread,
                        [&](const FCk_PerfLab_MetricStats& InStats) { return Get_Attainment(InStats.Get_AvgMs(), InBudgetMs); });
                    break;

                case ECk_PerfLab_ScoreComponent::RenderThreadHeadroom:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::RenderThread,
                        [&](const FCk_PerfLab_MetricStats& InStats) { return Get_Attainment(InStats.Get_AvgMs(), InBudgetMs); });
                    break;

                case ECk_PerfLab_ScoreComponent::GpuHeadroom:
                    Values = Get_MetricValues(Usable, ECk_PerfLab_Metric::Gpu,
                        [&](const FCk_PerfLab_MetricStats& InStats) { return Get_Attainment(InStats.Get_AvgMs(), InBudgetMs); });
                    break;

                case ECk_PerfLab_ScoreComponent::SpatialConsistency:
                {
                    // How evenly the level performs. A place that is uniformly mediocre is a
                    // different problem from one that is mostly fine with a cliff in it, and the
                    // score should be able to tell them apart.
                    const auto Averages = Get_MetricValues(Usable, ECk_PerfLab_Metric::Frame,
                        [](const FCk_PerfLab_MetricStats& InStats) { return static_cast<double>(InStats.Get_AvgMs()); });

                    if (const auto Spread = ck::algo::CoefficientOfVariation(Averages); Spread.IsSet())
                    {
                        Values.Add(FMath::Clamp(100.0 * (1.0 - *Spread), 0.0, 100.0));
                    }
                    break;
                }

                case ECk_PerfLab_ScoreComponent::MeasurementConfidence:
                {
                    // Counted over ALL positions, not just usable ones: positions dropped for being
                    // unusable are exactly what this component exists to report.
                    if (NOT InSession.Get_Positions().IsEmpty())
                    {
                        const auto Trusted = ck::algo::CountIf(InSession.Get_Positions(),
                            [](const FCk_PerfLab_PositionResult& InPosition)
                            {
                                return InPosition.Get_Confidence().Get_Rating() == ECk_PerfLab_Confidence::High
                                    || InPosition.Get_Confidence().Get_Rating() == ECk_PerfLab_Confidence::Medium;
                            });

                        Values.Add(100.0 * static_cast<double>(Trusted) / static_cast<double>(InSession.Get_Positions().Num()));
                    }
                    break;
                }
            }

            // Harmonic rather than arithmetic on purpose: one terrible area should drag the result
            // down hard, because that is how a player experiences a level. Twenty good rooms do not
            // compensate for the one that stutters, and an arithmetic mean would say they do.
            const auto Aggregated = ck::algo::HarmonicMean(Values);

            Results.Add(FCk_PerfLab_ScoreComponentResult{}
                .Set_Component(Weighted._Component)
                .Set_Weight(Weighted._Weight)
                .Set_Value(Aggregated.IsSet() ? FMath::RoundToInt32(*Aggregated) : 0)
                .Set_Included(Aggregated.IsSet())
                .Set_ExcludedReason(Aggregated.IsSet() ? FString{} : FString{TEXT("not measured at any usable position")}));
        }

        const auto TotalIncludedWeight = ck::algo::SumBy(Results, [](const FCk_PerfLab_ScoreComponentResult& InResult)
        {
            return InResult.Get_Included() ? InResult.Get_Weight() : 0.0f;
        });

        auto Score = FCk_PerfLab_Score{}.Set_Formula(TEXT(
            "weighted harmonic mean of per-position component scores; "
            "per-position component = clamp(100 * budgetMs / actualMs, 0, 100); "
            "components that could not be measured are excluded and the remaining weights renormalised"));

        if (TotalIncludedWeight <= 0.0)
        {
            // Nothing measurable at all. Zero would read as "this level scored badly"; the component
            // table beside it is what says nothing could be scored.
            return Score.Set_Components(Results).Set_Value(0);
        }

        // Renormalise over what survived, so a missing GPU shifts weight onto the axes that were
        // measured rather than dragging the total down.
        ck::algo::ForEach(Results, [&](FCk_PerfLab_ScoreComponentResult& InResult)
        {
            InResult.Set_Weight(InResult.Get_Included()
                ? static_cast<float>(InResult.Get_Weight() / TotalIncludedWeight)
                : 0.0f);
        });

        const auto Total = ck::algo::SumBy(Results, [](const FCk_PerfLab_ScoreComponentResult& InResult)
        {
            return InResult.Get_Included() ? InResult.Get_Weight() * static_cast<float>(InResult.Get_Value()) : 0.0f;
        });

        return Score.Set_Components(Results).Set_Value(FMath::Clamp(FMath::RoundToInt32(Total), 0, 100));
    }
}

// --------------------------------------------------------------------------------------------------------------------
