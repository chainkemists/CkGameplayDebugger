#include "CkPerfLab_SessionCompare.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_compare
{
    // Below this, a difference is not worth a reader's attention no matter how quiet the two runs
    // were. Without it, two exceptionally stable sessions would earn a near-zero band and report
    // every hundredth of a millisecond as a regression.
    constexpr auto k_MinNoiseBandMs = 0.25f;

    // The band is widened past the measured spread because two runs differ for reasons the spread
    // within one run never sees: a different shader cache state, a different streaming order, a
    // different thermal point on the same GPU.
    constexpr auto k_NoiseBandSlack = 1.5f;

    const auto k_ComparedMetrics = TArray<ECk_PerfLab_Metric>
    {
        ECk_PerfLab_Metric::Frame,
        ECk_PerfLab_Metric::GameThread,
        ECk_PerfLab_Metric::RenderThread,
        ECk_PerfLab_Metric::RhiThread,
        ECk_PerfLab_Metric::Gpu,
    };

    /**
     * How much this measurement moved around within its own run, taken as the gap between the
     * average and the p99. One-sided on purpose: frame times have a floor and a long tail, so the
     * upper spread is what actually describes how noisy the sample was.
     */
    auto
        Get_Spread(
            const FCk_PerfLab_MetricStats& InStats)
        -> double
    {
        return FMath::Max(0.0, static_cast<double>(InStats.Get_P99Ms() - InStats.Get_AvgMs()));
    }

    auto
        Get_NoiseBand(
            const FCk_PerfLab_MetricStats& InBaseline,
            const FCk_PerfLab_MetricStats& InCurrent)
        -> float
    {
        const auto Spreads = TArray<double>{Get_Spread(InBaseline), Get_Spread(InCurrent)};
        const auto Typical = ck::algo::Mean(Spreads).Get(0.0);

        return FMath::Max(k_MinNoiseBandMs, static_cast<float>(Typical * k_NoiseBandSlack));
    }

    auto
        Get_Verdict(
            float InDeltaMs,
            float InNoiseBandMs)
        -> ECk_PerfLab_CompareVerdict
    {
        if (InDeltaMs > InNoiseBandMs)
        { return ECk_PerfLab_CompareVerdict::Regressed; }

        if (InDeltaMs < -InNoiseBandMs)
        { return ECk_PerfLab_CompareVerdict::Improved; }

        return ECk_PerfLab_CompareVerdict::Unchanged;
    }

    auto
        Build_MetricDelta(
            ECk_PerfLab_Metric InMetric,
            const FCk_PerfLab_MetricStats& InBaseline,
            const FCk_PerfLab_MetricStats& InCurrent)
        -> FCk_PerfLab_MetricDelta
    {
        auto Delta = FCk_PerfLab_MetricDelta{}.Set_Metric(InMetric);

        // A metric that either run could not measure has no difference to report. Treating an
        // unavailable side as zero would manufacture the largest regression in the table out of a
        // headless run.
        if (InBaseline.Get_Availability() != ECk_Stats_MetricAvailability::Available ||
            InCurrent.Get_Availability()  != ECk_Stats_MetricAvailability::Available)
        {
            return Delta.Set_Verdict(ECk_PerfLab_CompareVerdict::Incomparable);
        }

        const auto DeltaMs   = InCurrent.Get_AvgMs() - InBaseline.Get_AvgMs();
        const auto NoiseBand = Get_NoiseBand(InBaseline, InCurrent);

        return Delta.Set_BaselineMs(InBaseline.Get_AvgMs())
                    .Set_CurrentMs(InCurrent.Get_AvgMs())
                    .Set_DeltaMs(DeltaMs)
                    .Set_NoiseBandMs(NoiseBand)
                    .Set_Verdict(Get_Verdict(DeltaMs, NoiseBand));
    }

    /**
     * The verdict a group of verdicts adds up to. Regression wins outright — a position that got
     * slower somewhere got slower, whatever else improved alongside it — and Incomparable only
     * survives when there was nothing at all to compare.
     */
    auto
        Reduce_Verdicts(
            const TArray<ECk_PerfLab_CompareVerdict>& InVerdicts)
        -> ECk_PerfLab_CompareVerdict
    {
        if (ck::algo::AnyOf(InVerdicts, [](ECk_PerfLab_CompareVerdict InVerdict)
            { return InVerdict == ECk_PerfLab_CompareVerdict::Regressed; }))
        {
            return ECk_PerfLab_CompareVerdict::Regressed;
        }

        if (ck::algo::AnyOf(InVerdicts, [](ECk_PerfLab_CompareVerdict InVerdict)
            { return InVerdict == ECk_PerfLab_CompareVerdict::Improved; }))
        {
            return ECk_PerfLab_CompareVerdict::Improved;
        }

        if (ck::algo::AnyOf(InVerdicts, [](ECk_PerfLab_CompareVerdict InVerdict)
            { return InVerdict == ECk_PerfLab_CompareVerdict::Unchanged; }))
        {
            return ECk_PerfLab_CompareVerdict::Unchanged;
        }

        return ECk_PerfLab_CompareVerdict::Incomparable;
    }

    auto
        Build_Warnings(
            const FCk_PerfLab_Session& InBaseline,
            const FCk_PerfLab_Session& InCurrent)
        -> TArray<ECk_PerfLab_CompareWarning>
    {
        const auto& Left  = InBaseline.Get_Environment();
        const auto& Right = InCurrent.Get_Environment();

        auto Warnings = TArray<ECk_PerfLab_CompareWarning>{};

        const auto AddIf = [&Warnings](bool InDiffers, ECk_PerfLab_CompareWarning InWarning)
        {
            if (InDiffers)
            { Warnings.Add(InWarning); }
        };

        AddIf(Left.Get_MachineName()        != Right.Get_MachineName(),        ECk_PerfLab_CompareWarning::DifferentMachine);
        AddIf(Left.Get_GpuBrand()           != Right.Get_GpuBrand(),           ECk_PerfLab_CompareWarning::DifferentGpu);
        AddIf(Left.Get_CpuBrand()           != Right.Get_CpuBrand(),           ECk_PerfLab_CompareWarning::DifferentCpu);
        AddIf(Left.Get_RhiName()            != Right.Get_RhiName(),            ECk_PerfLab_CompareWarning::DifferentRhi);
        AddIf(Left.Get_BuildConfiguration() != Right.Get_BuildConfiguration(), ECk_PerfLab_CompareWarning::DifferentBuildConfiguration);
        AddIf(Left.Get_InstanceMode()       != Right.Get_InstanceMode(),       ECk_PerfLab_CompareWarning::DifferentInstanceMode);
        AddIf(Left.Get_ViewportSizeActual() != Right.Get_ViewportSizeActual(), ECk_PerfLab_CompareWarning::DifferentViewportSize);
        AddIf(Left.Get_EngineVersion()      != Right.Get_EngineVersion(),      ECk_PerfLab_CompareWarning::DifferentEngineVersion);

        AddIf(NOT FMath::IsNearlyEqual(InBaseline.Get_Request().Get_BudgetMs(),
                                       InCurrent.Get_Request().Get_BudgetMs()),
              ECk_PerfLab_CompareWarning::DifferentBudget);

        AddIf(InBaseline.Get_Request().Get_Mode() != InCurrent.Get_Request().Get_Mode(),
              ECk_PerfLab_CompareWarning::DifferentMode);

        return Warnings;
    }

    auto
        Get_Refusal(
            const FCk_PerfLab_Session& InBaseline,
            const FCk_PerfLab_Session& InCurrent,
            int32 InMatchedCount)
        -> ECk_PerfLab_CompareRefusal
    {
        // Two different maps have no shared coordinate space, so their position ids describe
        // unrelated places that happen to be near the same numbers.
        if (InBaseline.Get_Request().Get_MapPath() != InCurrent.Get_Request().Get_MapPath())
        { return ECk_PerfLab_CompareRefusal::DifferentMap; }

        if (InBaseline.Get_Positions().IsEmpty())
        { return ECk_PerfLab_CompareRefusal::BaselineHasNoPositions; }

        if (InCurrent.Get_Positions().IsEmpty())
        { return ECk_PerfLab_CompareRefusal::CurrentHasNoPositions; }

        if (InMatchedCount == 0)
        { return ECk_PerfLab_CompareRefusal::NoPositionsInCommon; }

        return ECk_PerfLab_CompareRefusal::None;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_PerfLab_PositionDelta::
    Get_FrameDelta() const
    -> TOptional<FCk_PerfLab_MetricDelta>
{
    return ck::algo::FindIf(_Metrics, [](const FCk_PerfLab_MetricDelta& InDelta)
    {
        return InDelta.Get_Metric() == ECk_PerfLab_Metric::Frame &&
               InDelta.Get_Verdict() != ECk_PerfLab_CompareVerdict::Incomparable;
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_PerfLab_Compare::
    Get_IsComparable() const
    -> bool
{
    return _Refusal == ECk_PerfLab_CompareRefusal::None;
}

auto
    FCk_PerfLab_Compare::
    Get_ScoreDelta() const
    -> TOptional<float>
{
    // A refused comparison is an ordinary outcome rather than a programming error, so it does not
    // ensure — there is simply no delta to hand back, and the caller renders the refusal instead.
    if (NOT Get_IsComparable())
    { return {}; }

    return _CurrentScore - _BaselineScore;
}

auto
    FCk_PerfLab_Compare::
    Get_RegressedCount() const
    -> int32
{
    return ck::algo::CountIf(_Positions, [](const FCk_PerfLab_PositionDelta& InPosition)
    {
        return InPosition.Get_Verdict() == ECk_PerfLab_CompareVerdict::Regressed;
    });
}

auto
    FCk_PerfLab_Compare::
    Get_ImprovedCount() const
    -> int32
{
    return ck::algo::CountIf(_Positions, [](const FCk_PerfLab_PositionDelta& InPosition)
    {
        return InPosition.Get_Verdict() == ECk_PerfLab_CompareVerdict::Improved;
    });
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Get_WarningText(
            ECk_PerfLab_CompareWarning InWarning)
        -> FString
    {
        switch (InWarning)
        {
            case ECk_PerfLab_CompareWarning::DifferentMachine:
                return TEXT("These sessions were measured on different machines. Timing differences "
                            "between them describe the two machines at least as much as the level.");

            case ECk_PerfLab_CompareWarning::DifferentGpu:
                return TEXT("Different GPUs. GPU and render-thread deltas are not attributable to "
                            "the level.");

            case ECk_PerfLab_CompareWarning::DifferentCpu:
                return TEXT("Different CPUs. Game-thread deltas are not attributable to the level.");

            case ECk_PerfLab_CompareWarning::DifferentRhi:
                return TEXT("Different RHIs. Render-thread, RHI-thread and GPU timings are measured "
                            "differently by each and are not directly comparable.");

            case ECk_PerfLab_CompareWarning::DifferentBuildConfiguration:
                return TEXT("Different build configurations. A Development build carries checks a "
                            "Test build does not, so a difference here is expected and is not the "
                            "level changing.");

            case ECk_PerfLab_CompareWarning::DifferentInstanceMode:
                return TEXT("Different instance modes. The two runs did not exercise the same code "
                            "path.");

            case ECk_PerfLab_CompareWarning::DifferentViewportSize:
                return TEXT("Different viewport sizes. GPU cost scales with pixel count, so this "
                            "alone can move every GPU number.");

            case ECk_PerfLab_CompareWarning::DifferentEngineVersion:
                return TEXT("Different engine versions. Anything could have changed underneath both "
                            "measurements.");

            case ECk_PerfLab_CompareWarning::DifferentBudget:
                return TEXT("These runs were REQUESTED at different frame budgets, so they may have "
                            "been measured under different settle settings. Both scores shown are "
                            "recomputed against one budget and do compare; what differs is the "
                            "conditions each run was taken under.");

            case ECk_PerfLab_CompareWarning::DifferentMode:
                return TEXT("Different measurement modes. The two runs used different position "
                            "counts, dwell lengths and sample targets.");
        }

        return TEXT("These sessions differ in a way that affects comparability.");
    }

    auto
        Get_RefusalText(
            ECk_PerfLab_CompareRefusal InRefusal)
        -> FString
    {
        switch (InRefusal)
        {
            case ECk_PerfLab_CompareRefusal::None:
                return {};

            case ECk_PerfLab_CompareRefusal::DifferentMap:
                return TEXT("These sessions measured different maps. Positions are matched by "
                            "location, so there is nothing meaningful to pair.");

            case ECk_PerfLab_CompareRefusal::BaselineHasNoPositions:
                return TEXT("The baseline session measured no positions.");

            case ECk_PerfLab_CompareRefusal::CurrentHasNoPositions:
                return TEXT("The current session measured no positions.");

            case ECk_PerfLab_CompareRefusal::NoPositionsInCommon:
                return TEXT("These sessions share no measured position. The map's navigable space "
                            "or content bounds changed enough that the planner chose entirely "
                            "different places to stand.");
        }

        return TEXT("These sessions cannot be compared.");
    }

    auto
        Compare_Sessions(
            const FCk_PerfLab_Session& InBaseline,
            const FCk_PerfLab_Session& InCurrent,
            float InBudgetMs)
        -> FCk_PerfLab_Compare
    {
        using namespace ck_perf_lab_compare;

        auto BaselineById = TMap<FString, FCk_PerfLab_PositionResult>{};

        for (const auto& Position : InBaseline.Get_Positions())
        {
            BaselineById.Add(Position.Get_PositionId(), Position);
        }

        auto Deltas       = TArray<FCk_PerfLab_PositionDelta>{};
        auto OnlyInCurrent = TArray<FString>{};
        auto MatchedIds    = TSet<FString>{};

        for (const auto& Position : InCurrent.Get_Positions())
        {
            const auto* Match = BaselineById.Find(Position.Get_PositionId());

            if (ck::Is_NOT_Valid(Match, ck::IsValid_Policy_NullptrOnly{}))
            {
                OnlyInCurrent.Add(Position.Get_PositionId());
                continue;
            }

            MatchedIds.Add(Position.Get_PositionId());

            const auto MetricDeltas = ck::algo::Transform<TArray<FCk_PerfLab_MetricDelta>>(
                k_ComparedMetrics,
                [&](ECk_PerfLab_Metric InMetric)
                {
                    return Build_MetricDelta(
                        InMetric,
                        Match->Get_Aggregate().Get_ByMetric(InMetric),
                        Position.Get_Aggregate().Get_ByMetric(InMetric));
                });

            const auto Verdicts = ck::algo::Transform<TArray<ECk_PerfLab_CompareVerdict>>(
                MetricDeltas,
                [](const FCk_PerfLab_MetricDelta& InDelta) { return InDelta.Get_Verdict(); });

            Deltas.Add(FCk_PerfLab_PositionDelta{}
                .Set_PositionId(Position.Get_PositionId())
                .Set_Location(Position.Get_Location())
                .Set_Metrics(MetricDeltas)
                .Set_Verdict(Reduce_Verdicts(Verdicts)));
        }

        auto OnlyInBaseline = TArray<FString>{};

        for (const auto& Position : InBaseline.Get_Positions())
        {
            if (NOT MatchedIds.Contains(Position.Get_PositionId()))
            {
                OnlyInBaseline.Add(Position.Get_PositionId());
            }
        }

        // Worst first, then by id. The id tie-break is what makes the table byte-stable: without it
        // two positions with the same verdict would order by whatever the session file happened to
        // list first, and an export diff would show churn that is not a change in the data.
        ck::algo::Sort(Deltas, [](const FCk_PerfLab_PositionDelta& InA, const FCk_PerfLab_PositionDelta& InB)
        {
            if (InA.Get_Verdict() != InB.Get_Verdict())
            { return InA.Get_Verdict() < InB.Get_Verdict(); }

            return InA.Get_PositionId() < InB.Get_PositionId();
        });

        ck::algo::Sort(OnlyInBaseline);
        ck::algo::Sort(OnlyInCurrent);

        const auto Warnings = Build_Warnings(InBaseline, InCurrent);

        const auto AllVerdicts = ck::algo::Transform<TArray<ECk_PerfLab_CompareVerdict>>(
            Deltas, [](const FCk_PerfLab_PositionDelta& InDelta) { return InDelta.Get_Verdict(); });

        return FCk_PerfLab_Compare{}
            .Set_MapPath(InCurrent.Get_Request().Get_MapPath())
            .Set_Refusal(Get_Refusal(InBaseline, InCurrent, MatchedIds.Num()))
            .Set_Warnings(Warnings)
            .Set_Positions(Deltas)
            .Set_OnlyInBaseline(OnlyInBaseline)
            .Set_OnlyInCurrent(OnlyInCurrent)
            .Set_BaselineScore(Compute_Score(InBaseline, InBudgetMs).Get_Value())
            .Set_CurrentScore(Compute_Score(InCurrent, InBudgetMs).Get_Value())
            .Set_Verdict(Reduce_Verdicts(AllVerdicts));
    }
}

// --------------------------------------------------------------------------------------------------------------------
