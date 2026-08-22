#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

// --------------------------------------------------------------------------------------------------------------------
// Pure reduction of a dwell's raw samples into the summary a session stores. No world, no engine
// state, no clock — everything here is a function of its arguments, which is what makes it testable
// against fixtures rather than against a running level.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /**
     * Reduce accepted per-frame samples to a metric summary.
     *
     * Outliers are classified by median + InMadK * MAD and then treated asymmetrically on purpose:
     * excluded from the average so typical behaviour is not skewed, but RETAINED in the worst case
     * and the 1% low so bad frames keep their impact. They are counted, never silently erased.
     *
     * An empty sample array yields Unavailable_NotYetSampled rather than a set of zeros.
     */
    CKPERFLAB_API auto
    Reduce_Samples(
        const TArray<float>& InSamples,
        float InMadK) -> FCk_PerfLab_MetricStats;

    /** The summary for a metric that could not be measured: a reason, and no numbers to misread. */
    CKPERFLAB_API auto
    Make_UnavailableStats(
        ECk_Stats_MetricAvailability InAvailability,
        const FString& InReason) -> FCk_PerfLab_MetricStats;

    // ----------------------------------------------------------------------------------------------------------------

    /** Everything the confidence rule looks at. Thresholds are passed in, never hard-coded here. */
    struct CKPERFLAB_API FCk_ConfidenceInputs
    {
        bool  _WarmupConverged        = true;
        bool  _StreamingQuiet         = true;
        bool  _ShaderCompileActivity  = false;
        int32 _SampleCount            = 0;
        int32 _TargetSampleCount      = 0;
        float _CoefficientOfVariation = 0.0f;
        int32 _OutlierCount           = 0;

        /** Spread above this is called out as HighVariance. */
        float _MaxAcceptableCv = 0.25f;

        /** Outliers above this share of the samples are called out as OutlierHeavy. */
        float _MaxAcceptableOutlierFraction = 0.10f;

        /** Samples below this share of the target are called out as LowSampleCount. */
        float _MinAcceptableSampleFraction = 0.75f;
    };

    /**
     * Rate how much a position's numbers can be trusted.
     *
     * The rule is deliberately countable rather than clever: each independent problem contributes
     * one reason, and the rating is High with none, Medium with one, Low with two, Unusable with
     * three or more — or immediately Unusable if nothing was sampled at all. A reader can therefore
     * reconstruct the rating from the reasons, which is the whole point of publishing them.
     */
    CKPERFLAB_API auto
    Compute_Confidence(
        const FCk_ConfidenceInputs& InInputs) -> FCk_PerfLab_ConfidenceResult;
}

// --------------------------------------------------------------------------------------------------------------------
