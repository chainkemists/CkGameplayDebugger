#include "CkPerfLab_SettleDetector.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_settle
{
    // A window with no spread and a window with no meaningful mean are both "not moving" as far as
    // the settle gate is concerned, so the unset case collapses to zero here rather than at every
    // call site.
    auto
        Get_Steadiness(
            const TArray<float>& InValues)
        -> float
    {
        return static_cast<float>(ck::algo::CoefficientOfVariation(InValues).Get(0.0));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    FCk_SettleDetector::FCk_SettleDetector(
        const FCk_PerfLab_SettleParams& InParams)
        : _Params(InParams)
    {
        _Accepted.Reserve(FMath::Max(0, InParams.Get_TargetSampleCount()));
    }

    auto
        FCk_SettleDetector::
        Get_CoefficientOfVariation() const
        -> float
    {
        return ck_perf_lab_settle::Get_Steadiness(_Accepted);
    }

    auto
        FCk_SettleDetector::
        Observe(
            float InFrameTimeMs,
            bool InStreamingQuiet,
            float InElapsedSec)
        -> ECk_SettleVerdict
    {
        if (_IsSampling)
        {
            _Accepted.Add(InFrameTimeMs);

            return _Accepted.Num() >= _Params.Get_TargetSampleCount()
                ? ECk_SettleVerdict::Complete
                : ECk_SettleVerdict::Sampling;
        }

        // Warmup frames are discarded unconditionally and before anything else is considered. The
        // frames right after a teleport are dominated by whatever the move itself cost, and no
        // amount of stability testing makes them representative.
        if (_WarmupSeen < _Params.Get_WarmupFrames())
        {
            ++_WarmupSeen;
            return ECk_SettleVerdict::Waiting;
        }

        _QuietFramesSeen = InStreamingQuiet ? _QuietFramesSeen + 1 : 0;

        _StabilityWindow.Add(InFrameTimeMs);

        while (_StabilityWindow.Num() > _Params.Get_StabilityFrames())
        {
            _StabilityWindow.RemoveAt(0);
        }

        const auto HasEnoughHistory = _StabilityWindow.Num() >= _Params.Get_StabilityFrames();
        const auto IsFrameTimeStable = HasEnoughHistory &&
            ck_perf_lab_settle::Get_Steadiness(_StabilityWindow) <= _Params.Get_StabilityCv();
        const auto IsStreamingSettled = _QuietFramesSeen >= _Params.Get_StreamingQuietFrames();

        const auto HasTimedOut = InElapsedSec >= _Params.Get_SettleTimeoutSec();

        if (NOT ((IsFrameTimeStable && IsStreamingSettled) || HasTimedOut))
        {
            return ECk_SettleVerdict::Waiting;
        }

        // Sampling begins. Both reasons are latched here rather than re-derived later, because they
        // describe the conditions at the moment the window opened — which is what the confidence
        // rating is a statement about.
        _IsSampling         = true;
        _SettleTimedOut     = HasTimedOut && NOT (IsFrameTimeStable && IsStreamingSettled);
        _StreamingWasActive = NOT IsStreamingSettled;

        _Accepted.Add(InFrameTimeMs);

        return _Accepted.Num() >= _Params.Get_TargetSampleCount()
            ? ECk_SettleVerdict::Complete
            : ECk_SettleVerdict::Sampling;
    }
}

// --------------------------------------------------------------------------------------------------------------------
