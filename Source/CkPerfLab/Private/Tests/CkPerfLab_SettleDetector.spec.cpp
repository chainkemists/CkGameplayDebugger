#include "CkPerfLab/Runner/CkPerfLab_SettleDetector.h"

#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_settle_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_Params()
        -> FCk_PerfLab_SettleParams
    {
        return FCk_PerfLab_SettleParams{}
            .Set_WarmupFrames(5)
            .Set_StabilityCv(0.05f)
            .Set_StabilityFrames(4)
            .Set_StreamingQuietFrames(3)
            .Set_SettleTimeoutSec(10.0f)
            .Set_TargetSampleCount(10);
    }

    /** Drive the detector with a steady frame time until it stops asking for more. */
    auto
        Run_ToCompletion(
            ck::perf_lab::FCk_SettleDetector& InDetector,
            float InFrameTimeMs,
            bool InStreamingQuiet,
            float InElapsedPerFrameSec,
            int32 InMaxFrames)
        -> int32
    {
        auto Elapsed = 0.0f;

        for (auto Frame = 0; Frame < InMaxFrames; ++Frame)
        {
            Elapsed += InElapsedPerFrameSec;

            if (InDetector.Observe(InFrameTimeMs, InStreamingQuiet, Elapsed) == ck::perf_lab::ECk_SettleVerdict::Complete)
            {
                return Frame + 1;
            }
        }

        return INDEX_NONE;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Settle_WarmupFramesAreNeverSampled,
    "Ck.PerfLab.Settle.WarmupFramesAreNeverSampled",
    ck_perf_lab_settle_spec::kFlags)

bool FCkPerfLab_Settle_WarmupFramesAreNeverSampled::RunTest(const FString& Parameters)
{
    auto Detector = ck::perf_lab::FCk_SettleDetector{ck_perf_lab_settle_spec::Make_Params()};

    // The frames straight after a teleport are dominated by the move itself. Even perfectly steady
    // ones must be thrown away, or the cost of getting somewhere is charged to being there.
    for (auto Frame = 0; Frame < 5; ++Frame)
    {
        TestTrue(TEXT("A warmup frame is not sampled"),
            Detector.Observe(10.0f, true, 0.1f) == ck::perf_lab::ECk_SettleVerdict::Waiting);
    }

    TestEqual(TEXT("Nothing was accepted during warmup"), Detector.Get_AcceptedCount(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Settle_SteadyFrames_SettleCleanly,
    "Ck.PerfLab.Settle.SteadyFrames_SettleCleanly",
    ck_perf_lab_settle_spec::kFlags)

bool FCkPerfLab_Settle_SteadyFrames_SettleCleanly::RunTest(const FString& Parameters)
{
    auto Detector = ck::perf_lab::FCk_SettleDetector{ck_perf_lab_settle_spec::Make_Params()};

    const auto Frames = ck_perf_lab_settle_spec::Run_ToCompletion(Detector, 10.0f, true, 0.01f, 200);

    TestTrue(TEXT("A steady dwell completes"), Frames != INDEX_NONE);
    TestEqual(TEXT("Exactly the target number of samples is taken"), Detector.Get_AcceptedCount(), 10);
    TestFalse(TEXT("A clean settle did not time out"),      Detector.Get_SettleTimedOut());
    TestFalse(TEXT("A clean settle saw quiet streaming"),    Detector.Get_StreamingWasActive());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Settle_BusyStreaming_DelaysSamplingThenTimesOut,
    "Ck.PerfLab.Settle.BusyStreaming_DelaysSamplingThenTimesOut",
    ck_perf_lab_settle_spec::kFlags)

bool FCkPerfLab_Settle_BusyStreaming_DelaysSamplingThenTimesOut::RunTest(const FString& Parameters)
{
    auto Detector = ck::perf_lab::FCk_SettleDetector{ck_perf_lab_settle_spec::Make_Params()};

    // Streaming never goes quiet. The detector must not wait forever: a position that will not
    // settle still has to be measured, because refusing to measure busy areas would systematically
    // hide the worst parts of a level.
    const auto Frames = ck_perf_lab_settle_spec::Run_ToCompletion(Detector, 10.0f, false, 1.0f, 200);

    TestTrue(TEXT("An unsettled dwell still completes"), Frames != INDEX_NONE);
    TestEqual(TEXT("It still takes a full sample window"), Detector.Get_AcceptedCount(), 10);

    // ...but it says so, and that is what downgrades the position's confidence.
    TestTrue(TEXT("The timeout is recorded"),            Detector.Get_SettleTimedOut());
    TestTrue(TEXT("The active streaming is recorded"),   Detector.Get_StreamingWasActive());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Settle_UnstableFrameTime_DelaysSampling,
    "Ck.PerfLab.Settle.UnstableFrameTime_DelaysSampling",
    ck_perf_lab_settle_spec::kFlags)

bool FCkPerfLab_Settle_UnstableFrameTime_DelaysSampling::RunTest(const FString& Parameters)
{
    auto Detector = ck::perf_lab::FCk_SettleDetector{ck_perf_lab_settle_spec::Make_Params()};

    // Warmup first, then wildly swinging frame times well inside the settle timeout. Nothing should
    // be accepted while the picture is still moving.
    for (auto Frame = 0; Frame < 5; ++Frame)
    {
        Detector.Observe(10.0f, true, 0.01f);
    }

    for (auto Frame = 0; Frame < 20; ++Frame)
    {
        const auto Swinging = (Frame % 2 == 0) ? 5.0f : 40.0f;
        Detector.Observe(Swinging, true, 0.02f * static_cast<float>(Frame + 1));
    }

    TestEqual(TEXT("An unstable frame time is never sampled before the timeout"),
        Detector.Get_AcceptedCount(), 0);

    // Once it steadies, sampling begins without needing the timeout.
    for (auto Frame = 0; Frame < 10; ++Frame)
    {
        Detector.Observe(10.0f, true, 1.0f);
    }

    TestTrue (TEXT("A steadied frame time starts sampling"), Detector.Get_AcceptedCount() > 0);
    TestFalse(TEXT("Settling on its own is not a timeout"),  Detector.Get_SettleTimedOut());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Settle_ReportsSpreadOfTheAcceptedWindow,
    "Ck.PerfLab.Settle.ReportsSpreadOfTheAcceptedWindow",
    ck_perf_lab_settle_spec::kFlags)

bool FCkPerfLab_Settle_ReportsSpreadOfTheAcceptedWindow::RunTest(const FString& Parameters)
{
    auto Detector = ck::perf_lab::FCk_SettleDetector{ck_perf_lab_settle_spec::Make_Params()};

    ck_perf_lab_settle_spec::Run_ToCompletion(Detector, 10.0f, true, 0.01f, 200);

    // A perfectly steady window has no spread, and the confidence rule reads this number to decide
    // whether to call out high variance.
    TestEqual(TEXT("A steady window reports no spread"), Detector.Get_CoefficientOfVariation(), 0.0f, 1e-5f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
