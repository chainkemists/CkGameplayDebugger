#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

// --------------------------------------------------------------------------------------------------------------------
// Deciding when a dwell's frames are worth believing.
//
// This is the credibility half of the whole tool: numbers taken while the camera is still settling,
// while streaming is still resolving, or while shaders are still compiling are not measurements of
// the level, they are measurements of the transition. The detector is pure and frame-driven so the
// policy can be tested exhaustively without a world.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /** What the detector wants the caller to do with the frame it just described. */
    enum class ECk_SettleVerdict : uint8
    {
        /** Still warming up or still unstable — keep feeding frames, record nothing. */
        Waiting,

        /** Stable: this frame and subsequent ones belong in the sample window. */
        Sampling,

        /** The sample window is full. Stop. */
        Complete,
    };

    /**
     * Drives one dwell: discard warmup, wait for the frame time to stop moving and for streaming to
     * go quiet, then accept frames until the target count is reached.
     *
     * The settle timeout is deliberately not a failure. A position that never stabilises still gets
     * measured — its numbers are simply marked lower-confidence, with the reason recorded. Refusing
     * to measure busy areas would systematically hide the worst parts of a level, which are exactly
     * the parts worth finding.
     */
    class CKPERFLAB_API FCk_SettleDetector
    {
    public:
        FCk_SettleDetector() = default;

        explicit FCk_SettleDetector(
            const FCk_PerfLab_SettleParams& InParams);

    public:
        /**
         * Feed one frame.
         *
         * @param InFrameTimeMs      that frame's frame time
         * @param InStreamingQuiet   whether streaming and async loading were idle this frame
         * @param InElapsedSec       seconds since this dwell began, for the settle timeout
         */
        auto
        Observe(
            float InFrameTimeMs,
            bool InStreamingQuiet,
            float InElapsedSec) -> ECk_SettleVerdict;

    public:
        /** Frames accepted into the window so far. */
        auto Get_AcceptedCount() const -> int32 { return _Accepted.Num(); }

        auto Get_Accepted() const -> const TArray<float>& { return _Accepted; }

        /** True when sampling began only because the timeout expired, not because it stabilised. */
        auto Get_SettleTimedOut() const -> bool { return _SettleTimedOut; }

        /** True when streaming was still active at the moment sampling began. */
        auto Get_StreamingWasActive() const -> bool { return _StreamingWasActive; }

        /** Spread of the accepted window, as the coefficient of variation. */
        auto Get_CoefficientOfVariation() const -> float;

    private:
        FCk_PerfLab_SettleParams _Params;

        TArray<float> _Accepted;
        TArray<float> _StabilityWindow;

        int32 _WarmupSeen        = 0;
        int32 _QuietFramesSeen   = 0;
        bool  _IsSampling        = false;
        bool  _SettleTimedOut    = false;
        bool  _StreamingWasActive = false;
    };
}

// --------------------------------------------------------------------------------------------------------------------
