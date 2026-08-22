#include "CkPerfLab_Analysis.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Format/CkFormat.h"

// --------------------------------------------------------------------------------------------------------------------
// The rule catalog.
//
// The gate every rule passes through is the product's whole claim: a rule may fire ONLY when a
// measured metric at a position exceeded budget AND its static signal is present in that position's
// census. A census full of expensive-looking content at a position that measured fine publishes
// nothing, because nothing there cost anybody anything.
//
// Every rule names a signal the census can actually carry. A rule whose signal is not measurable is
// worse than no rule: it can never fire, and its absence reads as a clean result.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_rules
{
    constexpr auto k_MaxContributors = 5;

    // Over-budget ratios separating the three severities. A finding at 1.05x budget is real, but it
    // is not the same news as one at 3x, and reporting them identically would flatten the list.
    constexpr auto k_CriticalRatio = 2.0f;
    constexpr auto k_MajorRatio    = 1.25f;

    auto
        Get_Severity(
            float InRatio)
        -> ECk_PerfLab_Severity
    {
        if (InRatio >= k_CriticalRatio)
        {
            return ECk_PerfLab_Severity::Critical;
        }

        if (InRatio >= k_MajorRatio)
        {
            return ECk_PerfLab_Severity::Major;
        }

        return ECk_PerfLab_Severity::Minor;
    }

    struct FCk_Thresholds
    {
        int64 _TriangleCount        = 1500000;
        int32 _MaterialSlotCount    = 60;
        int32 _ShadowCasterCount    = 6;
        int32 _LightCount           = 8;
        int32 _TickingActorCount    = 40;
        int32 _EffectCount          = 6;
        int32 _PrimitiveCount       = 120;
        int32 _DuplicateClassCount  = 20;
    };

    using FCk_Census = TArray<FCk_PerfLab_ActorCensusRow>;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_TriangleTotal(const FCk_Census& InCensus) -> double
    {
        return ck::algo::SumBy(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow) { return InRow.Get_TriangleCount(); });
    }

    auto
        Get_MaterialSlotTotal(const FCk_Census& InCensus) -> double
    {
        return ck::algo::SumBy(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow) { return InRow.Get_MaterialSlotCount(); });
    }

    auto
        Get_LightTotal(const FCk_Census& InCensus) -> double
    {
        return ck::algo::SumBy(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow) { return InRow.Get_LightCount(); });
    }

    auto
        Get_ShadowCasterCount(const FCk_Census& InCensus) -> double
    {
        return ck::algo::CountIf(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow)
        { return InRow.Get_CastsDynamicShadow(); });
    }

    auto
        Get_TickingCount(const FCk_Census& InCensus) -> double
    {
        return ck::algo::CountIf(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow) { return InRow.Get_TickEnabled(); });
    }

    auto
        Get_EffectTotal(const FCk_Census& InCensus) -> double
    {
        return ck::algo::SumBy(InCensus, [](const FCk_PerfLab_ActorCensusRow& InRow)
        { return InRow.Get_NiagaraComponentCount(); });
    }

    /** The largest run of one class. A stand-in for "this should probably have been instanced". */
    auto
        Get_LargestClassCluster(const FCk_Census& InCensus) -> double
    {
        auto CountsByClass = ck::algo::CountBy(InCensus,
            [](const FCk_PerfLab_ActorCensusRow& InRow) { return InRow.Get_ClassName(); });

        const auto Largest = ck::algo::MaxElement(CountsByClass,
            [](const TPair<FString, int32>& InPair) { return InPair.Value; });

        // Unset rather than zero for an empty census: nothing to count is not a count of nothing.
        return Largest.IsSet() ? static_cast<double>(Largest->Value) : 0.0;
    }

    // ----------------------------------------------------------------------------------------------------------------

    struct FCk_Rule
    {
        FString                    _CheckId;
        ECk_PerfLab_Metric         _Metric;
        FString                    _Signal;
        TFunction<double(const FCk_Census&)> _Measure;
        double                     _Threshold;
        FString                    _Recommendation;
        ECk_PerfLab_Band           _GainBand;
        ECk_PerfLab_Band           _EffortBand;
    };

    auto
        Get_Rules(
            const FCk_Thresholds& InThresholds)
        -> TArray<FCk_Rule>
    {
        return
        {
            {
                TEXT("Perf.Gpu.TriangleDensity"), ECk_PerfLab_Metric::Gpu, TEXT("nearbyTriangleCount"),
                &Get_TriangleTotal, static_cast<double>(InThresholds._TriangleCount),
                TEXT("Enable Nanite on the densest meshes here, or author LODs for them. Triangle load is the "
                     "dominant GPU cost at this position."),
                ECk_PerfLab_Band::High, ECk_PerfLab_Band::Low
            },
            {
                TEXT("Perf.Gpu.MaterialSlotDensity"), ECk_PerfLab_Metric::Gpu, TEXT("nearbyMaterialSlotCount"),
                &Get_MaterialSlotTotal, static_cast<double>(InThresholds._MaterialSlotCount),
                TEXT("Merge material slots on the meshes here. Each slot is a separate draw call, and this "
                     "position pays for all of them every frame."),
                ECk_PerfLab_Band::Medium, ECk_PerfLab_Band::Medium
            },
            {
                TEXT("Perf.Gpu.DynamicShadowCasters"), ECk_PerfLab_Metric::Gpu, TEXT("nearbyShadowCasterCount"),
                &Get_ShadowCasterCount, static_cast<double>(InThresholds._ShadowCasterCount),
                TEXT("Review dynamic shadow casting on the movable meshes here. Each one re-renders into the "
                     "shadow map every frame."),
                ECk_PerfLab_Band::High, ECk_PerfLab_Band::Low
            },
            {
                TEXT("Perf.Gpu.LightOverlap"), ECk_PerfLab_Metric::Gpu, TEXT("nearbyLightCount"),
                &Get_LightTotal, static_cast<double>(InThresholds._LightCount),
                TEXT("Reduce overlapping light influence here, or shrink attenuation radii. Overlapping lights "
                     "multiply shading cost rather than adding to it."),
                ECk_PerfLab_Band::High, ECk_PerfLab_Band::Medium
            },
            {
                TEXT("Perf.GameThread.TickDensity"), ECk_PerfLab_Metric::GameThread, TEXT("nearbyTickingActorCount"),
                &Get_TickingCount, static_cast<double>(InThresholds._TickingActorCount),
                TEXT("Disable tick on the actors here that do not need it, or move them to a slower tick "
                     "interval. Most placed actors never need per-frame work."),
                ECk_PerfLab_Band::Medium, ECk_PerfLab_Band::Low
            },
            {
                TEXT("Perf.GameThread.EffectDensity"), ECk_PerfLab_Metric::GameThread, TEXT("nearbyEffectComponentCount"),
                &Get_EffectTotal, static_cast<double>(InThresholds._EffectCount),
                TEXT("Review the particle and Niagara systems here for emitter counts and tick rates, and give "
                     "them significance-based culling."),
                ECk_PerfLab_Band::Medium, ECk_PerfLab_Band::Medium
            },
            {
                TEXT("Perf.RenderThread.PrimitiveCount"), ECk_PerfLab_Metric::RenderThread, TEXT("nearbyActorCount"),
                [](const FCk_Census& InCensus) { return static_cast<double>(InCensus.Num()); },
                static_cast<double>(InThresholds._PrimitiveCount),
                TEXT("Reduce the number of separate primitives here. Render-thread cost scales with how many "
                     "things must be culled and submitted, not only with how big they are."),
                ECk_PerfLab_Band::Medium, ECk_PerfLab_Band::Medium
            },
            {
                TEXT("Perf.RenderThread.UninstancedDuplicates"), ECk_PerfLab_Metric::RenderThread, TEXT("largestIdenticalClassCluster"),
                &Get_LargestClassCluster, static_cast<double>(InThresholds._DuplicateClassCount),
                TEXT("Convert the repeated actors here to instanced static meshes. Identical primitives placed "
                     "separately are submitted separately."),
                ECk_PerfLab_Band::High, ECk_PerfLab_Band::Medium
            },
        };
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Analyse_Session(
            const FCk_PerfLab_Session& InSession,
            float InBudgetMs)
        -> FCk_PerfLab_Analysis
    {
        using namespace ck_perf_lab_rules;

        const auto Thresholds = FCk_Thresholds{};
        const auto Rules      = Get_Rules(Thresholds);

        auto Findings     = TArray<FCk_PerfLab_Finding>{};
        auto AnyOverBudget = false;

        for (const auto& Position : InSession.Get_Positions())
        {
            const auto& Census = Position.Get_ActorCensus();

            auto FiredAtThisPosition = false;

            // Measurement-quality findings are about the reading, not the level, so they are gated on
            // confidence rather than on budget and never count as attribution.
            if (Position.Get_Confidence().Get_Reasons().Contains(ECk_PerfLab_ConfidenceReason::StreamingActive))
            {
                Findings.Add(FCk_PerfLab_Finding{}
                    .Set_CheckId(TEXT("Perf.Confidence.StreamingUnsettled"))
                    .Set_StableKey(ck::Format_UE(TEXT("Perf.Confidence.StreamingUnsettled|{}"), Position.Get_PositionId()))
                    .Set_Severity(ECk_PerfLab_Severity::Minor)
                    .Set_PositionId(Position.Get_PositionId())
                    .Set_Evidence(FCk_PerfLab_Evidence{}
                        .Set_Metric(ECk_PerfLab_Metric::Frame)
                        .Set_Signal(TEXT("streamingActiveDuringSampling"))
                        .Set_SignalValue(1.0f))
                    .Set_Recommendations({FCk_PerfLab_Recommendation{}
                        .Set_Order(1)
                        .Set_GainBand(ECk_PerfLab_Band::Low)
                        .Set_EffortBand(ECk_PerfLab_Band::Low)
                        .Set_Text(TEXT("Re-measure this position: streaming was still resolving while it was "
                                       "sampled, so its numbers describe the transition rather than the level."))}));
            }

            // Whether this position is over budget AT ALL, decided once across every measured metric.
            // Deriving it as a side effect of the rule loop meant a position whose frame time was over
            // budget while each individual thread sat inside it reported "measured clean" AND
            // published an over-budget finding in the same breath — which is the ordinary shape of a
            // pipelined frame, not an exotic case.
            for (const auto Metric : {ECk_PerfLab_Metric::Frame, ECk_PerfLab_Metric::GameThread,
                                      ECk_PerfLab_Metric::RenderThread, ECk_PerfLab_Metric::RhiThread,
                                      ECk_PerfLab_Metric::Gpu})
            {
                const auto& MetricStats = Position.Get_Aggregate().Get_ByMetric(Metric);

                if (MetricStats.Get_IsAvailable() && InBudgetMs > 0.0f && MetricStats.Get_AvgMs() > InBudgetMs)
                {
                    AnyOverBudget = true;
                    break;
                }
            }

            for (const auto& Rule : Rules)
            {
                const auto& Stats = Position.Get_Aggregate().Get_ByMetric(Rule._Metric);

                // Half one of the gate: an unmeasured metric can never justify a finding.
                if (NOT Stats.Get_IsAvailable() || InBudgetMs <= 0.0f)
                {
                    continue;
                }

                const auto Ratio = Stats.Get_AvgMs() / InBudgetMs;

                if (Ratio <= 1.0f)
                {
                    continue;
                }

                // Half two: the static signal has to actually be there. This is what stops the tool
                // reporting a busy-looking room that measured perfectly well.
                const auto SignalValue = Rule._Measure(Census);

                if (SignalValue < Rule._Threshold)
                {
                    continue;
                }

                FiredAtThisPosition = true;

                auto Contributors = TArray<FCk_PerfLab_Contributor>{};

                // The census is already nearest-first, so the top rows are the closest things to the
                // measured cost. Proximity is all this claims to be.
                for (auto Index = 0; Index < FMath::Min(k_MaxContributors, Census.Num()); ++Index)
                {
                    Contributors.Add(FCk_PerfLab_Contributor{}
                        .Set_Rank(Index + 1)
                        .Set_ObjectPath(Census[Index].Get_ObjectPath())
                        .Set_ClassName(Census[Index].Get_ClassName())
                        .Set_DistanceCm(Census[Index].Get_DistanceCm())
                        .Set_Note(k_ContributorDisclaimer));
                }

                Findings.Add(FCk_PerfLab_Finding{}
                    .Set_CheckId(Rule._CheckId)
                    .Set_StableKey(ck::Format_UE(TEXT("{}|{}"), Rule._CheckId, Position.Get_PositionId()))
                    .Set_Severity(Get_Severity(Ratio))
                    .Set_PositionId(Position.Get_PositionId())
                    .Set_Evidence(FCk_PerfLab_Evidence{}
                        .Set_Metric(Rule._Metric)
                        .Set_MeasuredMs(Stats.Get_AvgMs())
                        .Set_BudgetMs(InBudgetMs)
                        .Set_OverBudgetRatio(Ratio)
                        .Set_Signal(Rule._Signal)
                        .Set_SignalValue(static_cast<float>(SignalValue)))
                    .Set_Contributors(Contributors)
                    .Set_Recommendations({FCk_PerfLab_Recommendation{}
                        .Set_Order(1)
                        .Set_GainBand(Rule._GainBand)
                        .Set_EffortBand(Rule._EffortBand)
                        .Set_Text(Rule._Recommendation)}));
            }

            // Over budget with nothing to blame is still worth saying. Staying silent would let the
            // absence of a specific rule read as an absence of a problem.
            const auto& Frame = Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame);

            if (NOT FiredAtThisPosition && Frame.Get_IsAvailable() && InBudgetMs > 0.0f &&
                Frame.Get_AvgMs() > InBudgetMs)
            {
                const auto Ratio = Frame.Get_AvgMs() / InBudgetMs;

                Findings.Add(FCk_PerfLab_Finding{}
                    .Set_CheckId(TEXT("Perf.General.OverBudgetUnattributed"))
                    .Set_StableKey(ck::Format_UE(TEXT("Perf.General.OverBudgetUnattributed|{}"), Position.Get_PositionId()))
                    .Set_Severity(Get_Severity(Ratio))
                    .Set_PositionId(Position.Get_PositionId())
                    .Set_Evidence(FCk_PerfLab_Evidence{}
                        .Set_Metric(ECk_PerfLab_Metric::Frame)
                        .Set_MeasuredMs(Frame.Get_AvgMs())
                        .Set_BudgetMs(InBudgetMs)
                        .Set_OverBudgetRatio(Ratio)
                        .Set_Signal(TEXT("none"))
                        .Set_SignalValue(0.0f))
                    .Set_Recommendations({FCk_PerfLab_Recommendation{}
                        .Set_Order(1)
                        .Set_GainBand(ECk_PerfLab_Band::Medium)
                        .Set_EffortBand(ECk_PerfLab_Band::High)
                        .Set_Text(TEXT("This position is over budget but nothing nearby matched a known cost "
                                       "pattern. Profile it directly with Unreal Insights."))}));
            }
        }

        // Nothing surveyed anywhere is a MEASUREMENT problem, not a level problem, and the two are easy to confuse:
        // an empty census makes every static signal fall below its threshold, so every over-budget position falls
        // through to the unattributed fallback and the report reads as "your level is slow and I cannot say why"
        // when the truth is "I never saw your level". The commonest cause is a world whose content had not streamed
        // in when the survey ran — World Partition, or level streaming with nothing loaded.
        const auto SurveyedAnything = ck::algo::AnyOf(InSession.Get_Positions(),
            [](const FCk_PerfLab_PositionResult& InPosition)
            { return NOT InPosition.Get_ActorCensus().IsEmpty(); });

        if (NOT InSession.Get_Positions().IsEmpty() && NOT SurveyedAnything)
        {
            Findings.Add(FCk_PerfLab_Finding{}
                .Set_CheckId(TEXT("Perf.Survey.NothingSurveyed"))
                .Set_StableKey(TEXT("Perf.Survey.NothingSurveyed"))
                .Set_Severity(ECk_PerfLab_Severity::Critical)
                .Set_Evidence(FCk_PerfLab_Evidence{}
                    .Set_Metric(ECk_PerfLab_Metric::Frame)
                    .Set_BudgetMs(InBudgetMs)
                    .Set_Signal(TEXT("actorsSurveyedNearAnyPosition"))
                    .Set_SignalValue(0.0f))
                .Set_Recommendations({FCk_PerfLab_Recommendation{}
                    .Set_Order(1)
                    .Set_GainBand(ECk_PerfLab_Band::High)
                    .Set_EffortBand(ECk_PerfLab_Band::Low)
                    .Set_Text(TEXT("No actors were found near ANY measured position, so no finding here can name a "
                                   "cause — the timings are real but nothing could be attributed. This usually means "
                                   "the level's content had not streamed in when the survey ran. Check whether the "
                                   "map uses World Partition or level streaming, and treat every other finding in "
                                   "this session as unattributed by measurement, not by absence of a problem."))}));
        }

        // Findings about the MEASUREMENT come before findings about the level, ahead of severity. A caveat saying
        // the survey saw nothing, or that streaming never settled, qualifies every number underneath it — read
        // second, it is read too late to stop somebody acting on attribution it has just invalidated.
        const auto Is_AboutTheMeasurement = [](const FCk_PerfLab_Finding& InFinding)
        {
            return InFinding.Get_CheckId().StartsWith(TEXT("Perf.Survey.")) ||
                   InFinding.Get_CheckId().StartsWith(TEXT("Perf.Confidence."));
        };

        // Then severity — the enum is declared worst-first, so ascending by value is most-severe first — then check
        // id and position, so the order is reproducible across runs.
        ck::algo::Sort(Findings, [&](const FCk_PerfLab_Finding& InA, const FCk_PerfLab_Finding& InB)
        {
            if (Is_AboutTheMeasurement(InA) != Is_AboutTheMeasurement(InB))
            {
                return Is_AboutTheMeasurement(InA);
            }

            if (InA.Get_Severity() != InB.Get_Severity())
            {
                return InA.Get_Severity() < InB.Get_Severity();
            }

            if (InA.Get_CheckId() != InB.Get_CheckId())
            {
                return InA.Get_CheckId() < InB.Get_CheckId();
            }

            return InA.Get_PositionId() < InB.Get_PositionId();
        });

        return FCk_PerfLab_Analysis{}
            .Set_BudgetMs(InBudgetMs)
            .Set_Score(Compute_Score(InSession, InBudgetMs))
            .Set_MeasuredClean(NOT InSession.Get_Positions().IsEmpty() && NOT AnyOverBudget)
            .Set_Findings(Findings);
    }
}

// --------------------------------------------------------------------------------------------------------------------
