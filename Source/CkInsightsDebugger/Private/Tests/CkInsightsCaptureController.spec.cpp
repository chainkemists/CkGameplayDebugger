#include "CkInsightsDebugger/Capture/CkInsightsCaptureController.h"

#include "CkInsightsAnalyzer/Core/CkTraceSession.h"
#include "CkInsightsAnalyzer/Report/CkJsonReport.h"
#include "CkInsightsAnalyzer/Report/CkMultiFrameReport.h"
#include "CkCore/Format/CkFormat.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Trace/Trace.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_insights_capture_tests
{
    class FCk_Latent_TraceStartStopRoundTrip final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_TraceStartStopRoundTrip(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _TracePath(FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("Automation"),
                TEXT("Tmp"),
                FString::Printf(
                    TEXT("CkInsightsCaptureController-%u.utrace"),
                    FPlatformProcess::GetCurrentProcessId())))
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(_TracePath), true);
            IFileManager::Get().Delete(*_TracePath, false, true);
        }

        virtual auto Update() -> bool override
        {
#if UE_TRACE_ENABLED
            const auto Now = FPlatformTime::Seconds();
            switch (_Phase)
            {
                case EPhase::Start:
                {
                    if (UE::Trace::IsTracing())
                    {
                        _Test->AddError(TEXT(
                            "The trace round-trip test requires a process with no existing active trace."));
                        return true;
                    }

                    _Controller = MakeUnique<FCkInsightsCaptureController>(
                        FTraceAuxiliary::EConnectionType::File,
                        _TracePath);
                    auto Error = FString{};
                    constexpr double TimedCaptureSeconds = 0.25;
                    const auto Started = _Controller->TryStart_TimedCapture(
                        TimedCaptureSeconds,
                        TimedCaptureScreenshotCount,
                        Error);
                    _Test->TestTrue(*ck::Format_UE(TEXT("Timed trace starts: {}"), Error), Started);
                    if (NOT Started)
                    { return true; }

                    const auto Snapshot = _Controller->Get_Snapshot();
                    _Test->TestTrue(TEXT("Started trace remains owned by the controller"), Snapshot.bIsOwnedByTool);
                    _Test->TestEqual(
                        TEXT("Timed trace retains its requested duration"),
                        Snapshot.TargetDurationSeconds,
                        TimedCaptureSeconds);
                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::AwaitConnection;
                    return false;
                }

                case EPhase::AwaitConnection:
                {
                    const auto Snapshot = _Controller->Get_Snapshot();
                    if (Snapshot.State == ECkInsightsCaptureState::Recording)
                    {
                        _Deadline = Now + TimeoutSeconds;
                        _Phase = EPhase::AwaitInactive;
                        return false;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(TEXT("The trace writer did not report a ready connection before timeout."));
                    if (UE::Trace::IsTracing() && FTraceAuxiliary::Stop())
                    {
                        _Deadline = Now + TimeoutSeconds;
                        _Phase = EPhase::AwaitInactive;
                        return false;
                    }
                    return true;
                }

                case EPhase::AwaitInactive:
                {
                    if (NOT UE::Trace::IsTracing())
                    {
                        const auto HasCompletedCapture = _Controller->Get_CompletedCapture(
                            _StoppedTracePath,
                            _StoppedTraceGuid);
                        _Test->TestTrue(
                            TEXT("Timed capture retains a completion record"),
                            HasCompletedCapture);
                        _Test->TestTrue(
                            TEXT("Timed capture completion returns its exact path"),
                            FPaths::IsSamePath(_StoppedTracePath, _TracePath));
                        _Test->TestTrue(
                            TEXT("Timed capture completion returns its trace identity"),
                            _StoppedTraceGuid.IsValid());
                        _Test->TestTrue(
                            TEXT("Stopped trace identity reports a finalized writer"),
                            _Controller->IsTraceWriterFinalized(_StoppedTraceGuid));
                        _Test->TestFalse(
                            TEXT("Trace is inactive after stop"),
                            _Controller->Get_Snapshot().bIsTracing);
                        _Test->TestTrue(
                            TEXT("Trace file was written"),
                            IFileManager::Get().FileExists(*_TracePath));
                        _Test->TestTrue(
                            TEXT("Trace file contains data"),
                            IFileManager::Get().FileSize(*_TracePath) > 0);

                        auto Session = FCk_TraceSession{};
                        const auto OpenedTrace = Session.Open(_TracePath);
                        _Test->TestTrue(TEXT("Completed timed trace opens for analysis"), OpenedTrace);
                        if (OpenedTrace)
                        {
                            const auto Screenshots = Session.GetScreenshots();
                            const auto UsesNullRhi = FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
                            if (NOT UsesNullRhi)
                            {
                                _Test->TestEqual(
                                     TEXT("Real-RHI timed trace contains every screenshot milestone"),
                                     Screenshots.Num(),
                                    TimedCaptureScreenshotCount);

                                auto ScreenshotNames = TSet<FString>{};
                                for (const auto& Screenshot : Screenshots)
                                {
                                    ScreenshotNames.Add(Screenshot.Name);
                                    _Test->TestTrue(
                                        *ck::Format_UE(TEXT("Screenshot [{}] payload is complete"), Screenshot.Name),
                                        Screenshot.bIsPayloadComplete);
                                    _Test->TestTrue(
                                        *ck::Format_UE(TEXT("Screenshot [{}] maps to a game frame"), Screenshot.Name),
                                        Screenshot.GameFrameIndex != INDEX_NONE);

                                    auto Payload = TArray<uint8>{};
                                    _Test->TestTrue(
                                        *ck::Format_UE(TEXT("Screenshot [{}] payload can be copied"), Screenshot.Name),
                                        Session.TryCopyScreenshotData(Screenshot.Id, Payload));
                                    _Test->TestTrue(
                                        *ck::Format_UE(TEXT("Screenshot [{}] payload contains image bytes"), Screenshot.Name),
                                        Payload.Num() > 8);
                                }

                                for (const auto* Milestone : {
                                    TEXT("CkInsights_TimedCapture_10Percent"),
                                    TEXT("CkInsights_TimedCapture_30Percent"),
                                    TEXT("CkInsights_TimedCapture_50Percent"),
                                    TEXT("CkInsights_TimedCapture_70Percent"),
                                    TEXT("CkInsights_TimedCapture_90Percent")})
                                {
                                    _Test->TestTrue(
                                        *ck::Format_UE(TEXT("Trace contains [{}]"), Milestone),
                                        ScreenshotNames.Contains(Milestone));
                                }
                            }

                            auto Report = FCk_MultiFrameReport{};
                            const auto Markdown = Report.AnalyzeWorstFrames(Session, 5);
                            const auto Json = FCk_JsonReport::GenerateMultiFrame(
                                Session,
                                Report.GetStats(),
                                Report.GetConfig());
                            _Test->TestTrue(
                                TEXT("Timed trace contains analyzable game frames"),
                                Report.GetStats().FrameCount > 0);
                            _Test->TestTrue(
                                TEXT("Markdown contains detailed hot-frame sections"),
                                Markdown.Contains(TEXT("Hot Frame 1 Detail")));
                            _Test->TestTrue(
                                TEXT("JSON declares schema version 2"),
                                Json.Contains(TEXT("\"schemaVersion\": 2")));
                            _Test->TestTrue(
                                TEXT("JSON contains detailed hot frames"),
                                Json.Contains(TEXT("\"hotFrames\"")));
                        }
                        Session.Close();

                        _Test->TestTrue(
                            TEXT("Matching completion acknowledgement succeeds"),
                            _Controller->Acknowledge_CompletedCapture(_StoppedTraceGuid));
                        IFileManager::Get().Delete(*_TracePath, false, true);
                        return true;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(TEXT("The timed trace did not auto-stop and finish closing before timeout."));
                    if (UE::Trace::IsTracing())
                    { FTraceAuxiliary::Stop(); }
                    return true;
                }
            }
#endif
            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitConnection,
            AwaitInactive,
        };

        static constexpr double TimeoutSeconds = 10.0;
        static constexpr int32 TimedCaptureScreenshotCount = 5;

        FAutomationTestBase* _Test = nullptr;
        TUniquePtr<FCkInsightsCaptureController> _Controller;
        FString _TracePath;
        FString _StoppedTracePath;
        FGuid _StoppedTraceGuid;
        EPhase _Phase = EPhase::Start;
        double _Deadline = 0.0;
    };
}

// --------------------------------------------------------------------------------------------------------------------

using ck_insights_capture_tests::FCk_Latent_TraceStartStopRoundTrip;

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTraceSession_RejectsScreenshotReadsWithoutOpenSession,
    "Ck.InsightsDebugger.TraceSession.RejectsScreenshotReadsWithoutOpenSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTraceSession_RejectsScreenshotReadsWithoutOpenSession::RunTest(const FString& Parameters)
{
    auto Session = FCk_TraceSession{};
    auto Payload = TArray<uint8>{1, 2, 3};

    TestTrue(TEXT("An unopened session enumerates no screenshots"), Session.GetScreenshots().IsEmpty());
    TestFalse(TEXT("An unopened session rejects payload reads"), Session.TryCopyScreenshotData(17, Payload));
    TestTrue(TEXT("A rejected payload read clears caller-owned output"), Payload.IsEmpty());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_SetsStatMaxPerGroup,
    "Ck.InsightsDebugger.Capture.SetsStatMaxPerGroup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_SetsStatMaxPerGroup::RunTest(const FString& Parameters)
{
    auto Controller = FCkInsightsCaptureController{};
    const auto OriginalValue = Controller.Get_StatMaxPerGroup();
    if (NOT OriginalValue.IsSet())
    {
        AddError(TEXT("stats.MaxPerGroup is not registered."));
        return false;
    }

    const auto TestValue = OriginalValue.GetValue() == 37 ? 38 : 37;
    auto Error = FString{};
    const auto Changed = Controller.TrySet_StatMaxPerGroup(TestValue, Error);
    TestTrue(*ck::Format_UE(TEXT("MaxPerGroup changes: {}"), Error), Changed);
    TestEqual(TEXT("MaxPerGroup reports the changed value"), Controller.Get_StatMaxPerGroup().Get(-1), TestValue);

    Error.Reset();
    AddExpectedError(
        TEXT("Stats MaxPerGroup must be at least one"),
        EAutomationExpectedErrorFlags::Contains,
        -1);
    const auto InvalidWasRejected = NOT Controller.TrySet_StatMaxPerGroup(0, Error);
    TestTrue(TEXT("MaxPerGroup rejects zero"), InvalidWasRejected);
    TestEqual(TEXT("Rejected value does not mutate MaxPerGroup"), Controller.Get_StatMaxPerGroup().Get(-1), TestValue);

    Error.Reset();
    const auto Restored = Controller.TrySet_StatMaxPerGroup(OriginalValue.GetValue(), Error);
    TestTrue(*ck::Format_UE(TEXT("MaxPerGroup restores: {}"), Error), Restored);
    TestEqual(
        TEXT("MaxPerGroup reports the restored value"),
        Controller.Get_StatMaxPerGroup().Get(-1),
        OriginalValue.GetValue());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_StartStopRoundTrip,
    "Ck.InsightsDebugger.Capture.StartStopRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_StartStopRoundTrip::RunTest(const FString& Parameters)
{
    AddExpectedError(
        TEXT("[MemAlloc] Invalid Tag"),
        EAutomationExpectedErrorFlags::Contains,
        -1,
        false);
    AddExpectedError(
        TEXT("TagTracker errors:"),
        EAutomationExpectedErrorFlags::Contains,
        -1,
        false);
    ADD_LATENT_AUTOMATION_COMMAND(FCk_Latent_TraceStartStopRoundTrip(this));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_UsesFocusedTraceProfile,
    "Ck.InsightsDebugger.Capture.UsesFocusedTraceProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_UsesFocusedTraceProfile::RunTest(const FString& Parameters)
{
    auto Channels = TArray<FString>{};
    FCkInsightsCaptureController::Get_TraceChannels().ParseIntoArray(Channels, TEXT(","), true);

    auto UniqueChannels = TSet<FString>{};
    UniqueChannels.Append(Channels);
    TestEqual(TEXT("Trace profile has no duplicate channels"), UniqueChannels.Num(), Channels.Num());
    for (const auto* Required : {
        TEXT("cpu"),
        TEXT("gpu"),
        TEXT("frame"),
        TEXT("screenshot"),
        TEXT("stats"),
        TEXT("object"),
        TEXT("rhicommands"),
        TEXT("rendercommands")})
    {
        TestTrue(
            *FString::Printf(TEXT("Trace profile contains %s"), Required),
            UniqueChannels.Contains(Required));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_MapsTimedCaptureProgress,
    "Ck.InsightsDebugger.Capture.MapsTimedCaptureProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_MapsTimedCaptureProgress::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Negative elapsed time clamps to zero progress"),
        FCkInsightsCaptureController::DoGet_TimedCaptureProgress(-2.0, 20.0),
        0.0);
    TestEqual(
        TEXT("Half duration reports half progress"),
        FCkInsightsCaptureController::DoGet_TimedCaptureProgress(10.0, 20.0),
        0.5);
    TestEqual(
        TEXT("Elapsed time beyond duration clamps to full progress"),
        FCkInsightsCaptureController::DoGet_TimedCaptureProgress(25.0, 20.0),
        1.0);
    TestEqual(
        TEXT("Invalid duration fails closed at zero progress"),
        FCkInsightsCaptureController::DoGet_TimedCaptureProgress(10.0, 0.0),
        0.0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_MapsTimedCaptureScreenshotThresholds,
    "Ck.InsightsDebugger.Capture.MapsTimedCaptureScreenshotThresholds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_MapsTimedCaptureScreenshotThresholds::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("No screenshot is due before the first milestone"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(1.9, 20.0, 3).Num(),
        0);
    TestTrue(
        TEXT("Zero screenshot capture has no milestones"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 0).IsEmpty());
    TestTrue(
        TEXT("One screenshot capture uses the center milestone"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 1) == TArray<int32>{50});
    TestTrue(
        TEXT("Two screenshot capture uses the endpoint milestones"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 2) == TArray<int32>{10, 90});
    TestTrue(
        TEXT("Default screenshot count preserves the 10, 50, and 90 percent milestones"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 3) == TArray<int32>{10, 50, 90});
    TestTrue(
        TEXT("Five screenshot capture is evenly distributed"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 5) == TArray<int32>{10, 30, 50, 70, 90});
    TestTrue(
        TEXT("Maximum screenshot count produces unique rounded percentage labels"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(
            20.0,
            20.0,
            FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount)
            == TArray<int32>{10, 17, 25, 32, 39, 46, 54, 61, 68, 75, 83, 90});
    TestTrue(
        TEXT("Large ticks include every crossed default milestone exactly once"),
        FCkInsightsCaptureController::DoGet_ScreenshotMilestones(20.0, 20.0, 3) == TArray<int32>{10, 50, 90});
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_RejectsTimedCaptureScreenshotCount,
    "Ck.InsightsDebugger.Capture.RejectsTimedCaptureScreenshotCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_RejectsTimedCaptureScreenshotCount::RunTest(const FString& Parameters)
{
    auto Controller = FCkInsightsCaptureController{};
    auto Error = FString{};
    const auto WasTracing = Controller.Get_Snapshot().bIsTracing;

    AddExpectedError(
        TEXT("Timed Insights capture screenshot count must be from"),
        EAutomationExpectedErrorFlags::Contains,
        -1);
    TestFalse(
        TEXT("Negative screenshot count is rejected before tracing"),
        Controller.TryStart_TimedCapture(1.0, -1, Error));
    TestEqual(
        TEXT("Negative screenshot count does not change tracing state"),
        Controller.Get_Snapshot().bIsTracing,
        WasTracing);

    Error.Reset();
    AddExpectedError(
        TEXT("Timed Insights capture screenshot count must be from"),
        EAutomationExpectedErrorFlags::Contains,
        -1);
    TestFalse(
        TEXT("Too-large screenshot count is rejected before tracing"),
        Controller.TryStart_TimedCapture(
            1.0,
            FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount + 1,
            Error));
    TestEqual(
        TEXT("Too-large screenshot count does not change tracing state"),
        Controller.Get_Snapshot().bIsTracing,
        WasTracing);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_MapsCommonStatProfiles,
    "Ck.InsightsDebugger.Capture.MapsCommonStatProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_MapsCommonStatProfiles::RunTest(const FString& Parameters)
{
    const auto TestProfile = [this](
        ECkInsightsStatProfile InProfile,
        std::initializer_list<const TCHAR*> InExpectedGroups)
    {
        const auto Groups = FCkInsightsCaptureController::Get_StatGroups(InProfile);
        TestEqual(TEXT("Stat profile group count"), Groups.Num(), static_cast<int32>(InExpectedGroups.size()));
        for (const auto* Expected : InExpectedGroups)
        {
            TestTrue(
                *FString::Printf(TEXT("Stat profile contains %s"), Expected),
                Groups.Contains(FName{Expected}));
        }
    };

    TestProfile(
        ECkInsightsStatProfile::CkProcessors,
        {TEXT("STATGROUP_CkProcessors"), TEXT("STATGROUP_CkProcessors_Details")});
    TestProfile(ECkInsightsStatProfile::CkScheduler, {TEXT("STATGROUP_CkScheduler")});
    TestProfile(
        ECkInsightsStatProfile::Script,
        {TEXT("STATGROUP_CkScript")});
    TestProfile(ECkInsightsStatProfile::UObjects, {TEXT("STATGROUP_UObjects")});
    TestProfile(ECkInsightsStatProfile::Rhi, {TEXT("STATGROUP_RHI")});
    TestProfile(
        ECkInsightsStatProfile::Rendering,
        {
            TEXT("STATGROUP_SceneRendering"),
            TEXT("STATGROUP_RenderThreadProcessing"),
            TEXT("STATGROUP_RenderThreadCommands"),
        });

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsCaptureController_TogglesCkStatProfiles,
    "Ck.InsightsDebugger.Capture.TogglesCkStatProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsCaptureController_TogglesCkStatProfiles::RunTest(const FString& Parameters)
{
    auto Controller = FCkInsightsCaptureController{};
    for (const auto Profile : {
        ECkInsightsStatProfile::CkProcessors,
        ECkInsightsStatProfile::CkScheduler,
        ECkInsightsStatProfile::Script})
    {
        for (const auto Group : Controller.Get_StatGroups(Profile))
        {
            const auto OriginalEnabled = Controller.DoIs_StatGroupEnabled(Group);
            const auto Changed = Controller.DoTrySet_StatGroupDisplayEnabled(Group, NOT OriginalEnabled);
            TestTrue(
                *ck::Format_UE(TEXT("[{}] changes in the active viewport"), Group),
                Changed);
            if (Changed)
            {
                TestEqual(
                    *ck::Format_UE(TEXT("[{}] reports the changed viewport state"), Group),
                    Controller.DoIs_StatGroupEnabled(Group),
                    NOT OriginalEnabled);
            }

            const auto Restored = Controller.DoTrySet_StatGroupDisplayEnabled(Group, OriginalEnabled);
            TestTrue(
                *ck::Format_UE(TEXT("[{}] restores in the active viewport"), Group),
                Restored);
            if (Restored)
            {
                TestEqual(
                    *ck::Format_UE(TEXT("[{}] reports the restored viewport state"), Group),
                    Controller.DoIs_StatGroupEnabled(Group),
                    OriginalEnabled);
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
