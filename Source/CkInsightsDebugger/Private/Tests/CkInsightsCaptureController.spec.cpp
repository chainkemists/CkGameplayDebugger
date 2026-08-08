#include "CkInsightsDebugger/Capture/CkInsightsCaptureController.h"

#include "CkCore/Format/CkFormat.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Trace/Trace.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
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
                    const auto Started = _Controller->TrySet_Tracing(true, Error);
                    _Test->TestTrue(*ck::Format_UE(TEXT("Trace starts: {}"), Error), Started);
                    if (NOT Started)
                    { return true; }

                    const auto Snapshot = _Controller->Get_Snapshot();
                    _Test->TestTrue(TEXT("Started trace remains owned by the controller"), Snapshot.bIsOwnedByTool);
                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::AwaitConnection;
                    return false;
                }

                case EPhase::AwaitConnection:
                {
                    if (_Controller->Can_ToggleTracing())
                    {
                        auto Error = FString{};
                        const auto Stopped = _Controller->TrySet_Tracing(
                            false,
                            Error,
                            &_StoppedTracePath,
                            &_StoppedTraceGuid);
                        _Test->TestTrue(*ck::Format_UE(TEXT("Connected trace stops: {}"), Error), Stopped);
                        _Test->TestTrue(
                            TEXT("Stopped file capture returns its exact path"),
                            FPaths::IsSamePath(_StoppedTracePath, _TracePath));
                        _Test->TestTrue(
                            TEXT("Stopped file capture returns its trace identity"),
                            _StoppedTraceGuid.IsValid());
                        if (NOT Stopped && UE::Trace::IsTracing())
                        { FTraceAuxiliary::Stop(); }

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
                        IFileManager::Get().Delete(*_TracePath, false, true);
                        return true;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(TEXT("The trace writer did not finish closing before timeout."));
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
