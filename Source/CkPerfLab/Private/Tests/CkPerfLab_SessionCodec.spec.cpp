#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_codec_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_PopulatedSession()
        -> FCk_PerfLab_Session
    {
        auto Metrics = FCk_PerfLab_MetricSet{};
        Metrics.Set_Frame(ck::perf_lab::Reduce_Samples({14.0f, 15.0f, 16.0f}, 3.5f))
               .Set_GameThread(ck::perf_lab::Reduce_Samples({9.0f, 9.5f}, 3.5f))
               .Set_RenderThread(ck::perf_lab::Reduce_Samples({12.0f}, 3.5f))
               .Set_RhiThread(ck::perf_lab::Reduce_Samples({2.0f}, 3.5f))
               .Set_Gpu(ck::perf_lab::Make_UnavailableStats(
                   ECk_Stats_MetricAvailability::Unavailable_NullRhi, TEXT("headless run, no RHI")));

        auto Census = FCk_PerfLab_ActorCensusRow{};
        Census.Set_ObjectPath(TEXT("/Game/Maps/Test.Test:PersistentLevel.SM_Rock_42"))
              .Set_ClassName(TEXT("StaticMeshActor"))
              .Set_DistanceCm(640.25f)
              .Set_InViewYaws({0.0f, 90.0f})
              .Set_TriangleCount(184320)
              .Set_MaterialSlotCount(3)
              .Set_CastsDynamicShadow(true);

        auto Position = FCk_PerfLab_PositionResult{};
        Position.Set_PositionId(TEXT("p_0011"))
                .Set_Location(FVector{1250.0, -880.0, 312.5})
                .Set_EyeHeightCm(160.0f)
                .Set_Revisited(true)
                .Set_Directions({FCk_PerfLab_DirectionResult{}.Set_YawDegrees(90.0f).Set_Metrics(Metrics)})
                .Set_Aggregate(Metrics)
                .Set_Confidence(FCk_PerfLab_ConfidenceResult{}
                    .Set_Rating(ECk_PerfLab_Confidence::Medium)
                    .Set_Reasons({ECk_PerfLab_ConfidenceReason::StreamingActive}))
                .Set_ActorCensus({Census});

        auto Environment = FCk_PerfLab_Environment{};
        Environment.Set_BuildConfiguration(TEXT("Development"))
                   .Set_InstanceMode(TEXT("game"))
                   .Set_RhiName(TEXT("D3D12"))
                   .Set_GpuBrand(TEXT("Test GPU"))
                   .Set_CpuBrand(TEXT("Test CPU"))
                   .Set_CpuCoreCount(16)
                   .Set_MachineName(TEXT("TEST-MACHINE"))
                   .Set_EngineVersion(TEXT("5.6.0"))
                   .Set_ChildBinary(TEXT("CkPluginsEditor-Cmd.exe"))
                   .Set_ViewportSizeActual(FIntPoint{1280, 720})
                   .Set_SourceControlDisabled(true)
                   .Set_StartedUtc(TEXT("2026-08-21T14:03:15Z"))
                   .Set_FinishedUtc(TEXT("2026-08-21T14:21:02Z"))
                   .Set_ForcedCvars({{TEXT("r.VSync"), TEXT("0")}, {TEXT("t.MaxFPS"), TEXT("0")}});

        auto Request = FCk_PerfLab_Request{};
        Request.Set_SessionId(TEXT("20260821-140311-9f3ac1b0"))
               .Set_MapPath(TEXT("/Game/Maps/Test.Test"))
               .Set_BudgetMs(16.67f)
               .Set_Seed(1337)
               .Set_RequestingHostPid(24188)
               .Set_CreatedUtc(TEXT("2026-08-21T14:03:11Z"))
               .Set_Mode(ECk_PerfLab_Mode::Deep)
               .Set_ModeParams(FCk_PerfLab_ModeParams::Get_ForMode(ECk_PerfLab_Mode::Deep));

        return FCk_PerfLab_Session{}
            .Set_SessionId(Request.Get_SessionId())
            .Set_State(ECk_PerfLab_RunState::Done)
            .Set_Request(Request)
            .Set_Environment(Environment)
            .Set_Positions({Position});
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_Session_RoundTrips,
    "Ck.PerfLab.Codec.Session_RoundTrips",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_Session_RoundTrips::RunTest(const FString& Parameters)
{
    const auto Original = ck_perf_lab_codec_spec::Make_PopulatedSession();
    const auto Json     = ck::perf_lab::Write_SessionToJson(Original);

    auto Decoded = FCk_PerfLab_Session{};

    TestTrue(TEXT("A written session reads back"), ck::perf_lab::TryRead_SessionFromJson(Json, Decoded));

    TestEqual(TEXT("Session id survives"),  Decoded.Get_SessionId(), Original.Get_SessionId());
    TestTrue (TEXT("State survives"),       Decoded.Get_State() == Original.Get_State());
    TestEqual(TEXT("Map path survives"),    Decoded.Get_Request().Get_MapPath(), Original.Get_Request().Get_MapPath());
    TestTrue (TEXT("Mode survives"),        Decoded.Get_Request().Get_Mode() == ECk_PerfLab_Mode::Deep);
    TestEqual(TEXT("Deep direction count survives"),
        Decoded.Get_Request().Get_ModeParams().Get_DirectionsPerPosition(), 16);

    TestEqual(TEXT("Position count survives"), Decoded.Get_Positions().Num(), 1);

    const auto& Position = Decoded.Get_Positions()[0];

    TestEqual(TEXT("Position id survives"),    Position.Get_PositionId(), TEXT("p_0011"));
    TestEqual(TEXT("Location survives"),       Position.Get_Location().X, 1250.0);
    TestTrue (TEXT("Confidence survives"),     Position.Get_Confidence().Get_Rating() == ECk_PerfLab_Confidence::Medium);
    TestEqual(TEXT("Confidence reasons survive"), Position.Get_Confidence().Get_Reasons().Num(), 1);
    TestEqual(TEXT("Census survives"),         Position.Get_ActorCensus().Num(), 1);
    TestEqual(TEXT("Census triangle count survives"),
        Position.Get_ActorCensus()[0].Get_TriangleCount(), static_cast<int64>(184320));
    TestEqual(TEXT("Forced cvars survive"),    Decoded.Get_Environment().Get_ForcedCvars().Num(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_Session_IsDeterministic,
    "Ck.PerfLab.Codec.Session_IsDeterministic",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_Session_IsDeterministic::RunTest(const FString& Parameters)
{
    // Two encodes of one object must be byte-identical, or every export built on this codec would
    // show spurious diffs between runs that measured the same thing.
    const auto Session = ck_perf_lab_codec_spec::Make_PopulatedSession();

    TestEqual(TEXT("Encoding is byte-stable"),
        ck::perf_lab::Write_SessionToJson(Session), ck::perf_lab::Write_SessionToJson(Session));

    // And a decode/re-encode cycle must land on the same bytes, which is the stronger property:
    // it says the codec loses nothing it also writes.
    auto Decoded = FCk_PerfLab_Session{};
    ck::perf_lab::TryRead_SessionFromJson(ck::perf_lab::Write_SessionToJson(Session), Decoded);

    TestEqual(TEXT("A round trip re-encodes identically"),
        ck::perf_lab::Write_SessionToJson(Decoded), ck::perf_lab::Write_SessionToJson(Session));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_BadInput_FailsWholly,
    "Ck.PerfLab.Codec.BadInput_FailsWholly",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_BadInput_FailsWholly::RunTest(const FString& Parameters)
{
    auto Decoded = FCk_PerfLab_Session{};

    TestFalse(TEXT("Malformed JSON is refused"),
        ck::perf_lab::TryRead_SessionFromJson(TEXT("{ this is not json"), Decoded));

    TestFalse(TEXT("JSON without a schema version is refused"),
        ck::perf_lab::TryRead_SessionFromJson(TEXT("{\"sessionId\":\"x\"}"), Decoded));

    // The version gate is what stops a future format being read with this version's field meanings.
    TestFalse(TEXT("An unknown schema version is refused"),
        ck::perf_lab::TryRead_SessionFromJson(TEXT("{\"schemaVersion\":9999,\"sessionId\":\"x\"}"), Decoded));

    TestTrue(TEXT("A refused decode leaves the output untouched"), Decoded.Get_SessionId().IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_UnavailableGpu_KeepsItsReason,
    "Ck.PerfLab.Codec.UnavailableGpu_KeepsItsReason",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_UnavailableGpu_KeepsItsReason::RunTest(const FString& Parameters)
{
    const auto Session = ck_perf_lab_codec_spec::Make_PopulatedSession();

    auto Decoded = FCk_PerfLab_Session{};
    ck::perf_lab::TryRead_SessionFromJson(ck::perf_lab::Write_SessionToJson(Session), Decoded);

    const auto& Gpu = Decoded.Get_Positions()[0].Get_Aggregate().Get_Gpu();

    // An unavailable metric must survive a round trip as unavailable WITH its reason. If this ever
    // decoded as Available with a zero, every score built on it would silently treat a missing GPU
    // measurement as a free one.
    TestFalse(TEXT("An unavailable GPU stays unavailable"), Gpu.Get_IsAvailable());
    TestTrue (TEXT("The unavailability reason survives"),   Gpu.Get_UnavailableReason().Contains(TEXT("no RHI")));
    TestTrue (TEXT("The specific unavailability cause survives"),
        Gpu.Get_Availability() == ECk_Stats_MetricAvailability::Unavailable_NullRhi);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_Request_RoundTrips,
    "Ck.PerfLab.Codec.Request_RoundTrips",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_Request_RoundTrips::RunTest(const FString& Parameters)
{
    const auto Original = ck_perf_lab_codec_spec::Make_PopulatedSession().Get_Request();

    auto Decoded = FCk_PerfLab_Request{};

    TestTrue(TEXT("A written request reads back"),
        ck::perf_lab::TryRead_RequestFromJson(ck::perf_lab::Write_RequestToJson(Original), Decoded));

    TestEqual(TEXT("Budget survives"), Decoded.Get_BudgetMs(), Original.Get_BudgetMs());
    TestEqual(TEXT("Seed survives"),   Decoded.Get_Seed(),     Original.Get_Seed());
    TestEqual(TEXT("Settle target sample count survives"),
        Decoded.Get_Settle().Get_TargetSampleCount(), Original.Get_Settle().Get_TargetSampleCount());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_Heartbeat_RoundTrips,
    "Ck.PerfLab.Codec.Heartbeat_RoundTrips",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_Heartbeat_RoundTrips::RunTest(const FString& Parameters)
{
    const auto Original = FCk_PerfLab_Heartbeat{}
        .Set_SessionId(TEXT("20260821-140311-9f3ac1b0"))
        .Set_State(ECk_PerfLab_RunState::Measuring)
        .Set_PositionsTotal(25)
        .Set_PositionsDone(11)
        .Set_CurrentPositionId(TEXT("p_0011"))
        .Set_LastUpdateUtc(TEXT("2026-08-21T14:07:44Z"));

    auto Decoded = FCk_PerfLab_Heartbeat{};

    TestTrue(TEXT("A written heartbeat reads back"),
        ck::perf_lab::TryRead_HeartbeatFromJson(ck::perf_lab::Write_HeartbeatToJson(Original), Decoded));

    TestTrue (TEXT("State survives"),          Decoded.Get_State() == ECk_PerfLab_RunState::Measuring);
    TestEqual(TEXT("Progress survives"),       Decoded.Get_PositionsDone(), 11);
    TestEqual(TEXT("Current position survives"), Decoded.Get_CurrentPositionId(), TEXT("p_0011"));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Codec_Summary_ReadsWithoutPositions,
    "Ck.PerfLab.Codec.Summary_ReadsWithoutPositions",
    ck_perf_lab_codec_spec::kFlags)

bool FCkPerfLab_Codec_Summary_ReadsWithoutPositions::RunTest(const FString& Parameters)
{
    const auto Json = ck::perf_lab::Write_SessionToJson(ck_perf_lab_codec_spec::Make_PopulatedSession());

    auto Summary = FCk_PerfLab_Session{};

    TestTrue(TEXT("A summary reads from a full session"),
        ck::perf_lab::TryRead_SessionSummaryFromJson(Json, Summary));

    TestEqual(TEXT("The summary carries the id"),  Summary.Get_SessionId(), TEXT("20260821-140311-9f3ac1b0"));
    TestTrue (TEXT("The summary carries state"),   Summary.Get_State() == ECk_PerfLab_RunState::Done);
    TestEqual(TEXT("The summary carries the map"), Summary.Get_Request().Get_MapPath(), TEXT("/Game/Maps/Test.Test"));

    // The fast path exists so a list of dozens of sessions costs nothing to draw: it must not have
    // decoded the measurements.
    TestEqual(TEXT("The summary does not decode positions"), Summary.Get_Positions().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
