#include "CkHangMonitor/CkHangMonitorController.h"

#include "CkHangMonitor_Module.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_hang_monitor_tests
{
    constexpr auto kTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    constexpr auto kControllerTimeoutSeconds = 20.0;

    static auto Test_IsStoppedWithoutPartialState(
        FAutomationTestBase& InTest,
        const FCkHangMonitorSnapshot& InSnapshot,
        const TCHAR* InContext) -> void
    {
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report monitoring"), InContext), InSnapshot.IsMonitoring);
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report stopping"), InContext), InSnapshot.IsStopping);
        InTest.TestFalse(*FString::Printf(TEXT("%s does not report ready"), InContext), InSnapshot.IsReady);
        InTest.TestEqual(*FString::Printf(TEXT("%s retains no target PID"), InContext), InSnapshot.TargetPid, uint32{0});
        InTest.TestTrue(*FString::Printf(TEXT("%s retains no output directory"), InContext), InSnapshot.OutputDirectory.IsEmpty());
        InTest.TestTrue(*FString::Printf(TEXT("%s retains no ProcDump path"), InContext), InSnapshot.ProcDumpPath.IsEmpty());
        InTest.TestEqual(*FString::Printf(TEXT("%s retains no dump count"), InContext), InSnapshot.NumDumps, 0);
        InTest.TestEqual(*FString::Printf(TEXT("%s retains the default dump bound"), InContext), InSnapshot.MaxDumps, 1);
    }

    static auto Get_MissingProcDumpPath() -> FString
    {
        return FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("Automation"),
            TEXT("CkHangMonitor"),
            TEXT("does-not-exist"),
            TEXT("procdump64.exe"));
    }

    class FCk_Latent_HangMonitorArmStop final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_HangMonitorArmStop(FAutomationTestBase* InTest, FString InProcDumpPath)
            : _Test(InTest)
            , _ProcDumpPath(MoveTemp(InProcDumpPath))
        {
        }

        auto Update() -> bool override
        {
            auto& Module = FCkHangMonitorModule::Get();
            auto& Controller = Module.Get_Controller();
            const auto Now = FPlatformTime::Seconds();

            switch (_Phase)
            {
                case EPhase::Start:
                {
                    if (Controller.Get_Snapshot().IsMonitoring)
                    {
                        _Test->AddError(TEXT("Opt-in test requires an inactive Hang Monitor controller."));
                        return true;
                    }

                    auto Error = FString{};
                    if (!Controller.TryStart(_ProcDumpPath, 1, Error))
                    {
                        _Test->AddError(FString::Printf(TEXT("Current-editor ProcDump monitor did not arm: %s"), *Error));
                        return true;
                    }

                    _Deadline = Now + kControllerTimeoutSeconds;
                    _Phase = EPhase::AwaitReady;
                    return false;
                }

                case EPhase::AwaitReady:
                {
                    const auto Armed = Controller.Get_Snapshot();
                    if (Armed.IsMonitoring && Armed.IsReady)
                    {
                        _Test->TestFalse(TEXT("ready monitor is not stopping"), Armed.IsStopping);
                        _Test->TestEqual(TEXT("ready monitor targets the current editor process"),
                            Armed.TargetPid, FPlatformProcess::GetCurrentProcessId());
                        _Test->TestEqual(TEXT("ready monitor retains the requested one-dump bound"), Armed.MaxDumps, 1);
                        _Test->TestEqual(TEXT("ready monitor retains the explicit ProcDump path"), Armed.ProcDumpPath, _ProcDumpPath);
                        _Test->TestFalse(TEXT("ready monitor publishes an output directory"), Armed.OutputDirectory.IsEmpty());

                        auto DuplicateError = FString{};
                        _Test->TestFalse(TEXT("duplicate arm is rejected while monitoring"),
                            Controller.TryStart(_ProcDumpPath, 1, DuplicateError));
                        _Test->TestFalse(TEXT("duplicate arm rejection explains why"), DuplicateError.IsEmpty());
                        const auto AfterDuplicate = Controller.Get_Snapshot();
                        _Test->TestTrue(TEXT("duplicate arm retains the original monitor"), AfterDuplicate.IsMonitoring);
                        _Test->TestEqual(TEXT("duplicate arm retains the original target"), AfterDuplicate.TargetPid, Armed.TargetPid);
                        _Test->TestEqual(TEXT("duplicate arm retains the original output directory"),
                            AfterDuplicate.OutputDirectory, Armed.OutputDirectory);

                        Module.OpenDebugger();
                        _Test->TestTrue(TEXT("the Hang Monitor tab opens while its controller is armed"), Module.IsDebuggerOpen());
                        Module.CloseDebugger();
                        _Test->TestFalse(TEXT("the Hang Monitor tab closes"), Module.IsDebuggerOpen());
                        _Test->TestTrue(TEXT("tab closure preserves the armed controller"),
                            Controller.Get_Snapshot().IsMonitoring);

                        auto StopError = FString{};
                        if (!Controller.TryStop(StopError))
                        {
                            _Test->AddError(FString::Printf(TEXT("ProcDump graceful cancellation was rejected: %s"), *StopError));
                            return true;
                        }

                        _Deadline = Now + kControllerTimeoutSeconds;
                        _Phase = EPhase::AwaitStopped;
                        return false;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("ProcDump did not reach its real attachment banner before timeout. Status=[%s] Error=[%s]"),
                        *Armed.Status,
                        *Armed.LastError));
                    RequestStop(Controller);
                    _Deadline = Now + kControllerTimeoutSeconds;
                    _Phase = EPhase::AwaitStopped;
                    return false;
                }

                case EPhase::AwaitStopped:
                {
                    const auto Stopped = Controller.Get_Snapshot();
                    if (!Stopped.IsMonitoring)
                    {
                        _Test->TestFalse(TEXT("stopped monitor leaves no cancellation-in-progress state"), Stopped.IsStopping);
                        _Test->TestFalse(TEXT("stopped monitor clears its readiness state"), Stopped.IsReady);
                        _Test->TestTrue(TEXT("gracefully stopped monitor reports no ProcDump error"), Stopped.LastError.IsEmpty());

                        auto RestartError = FString{};
                        if (!Controller.TryStart(_ProcDumpPath, 1, RestartError))
                        {
                            _Test->AddError(FString::Printf(TEXT("Second monitor arm for early-stop coverage failed: %s"), *RestartError));
                            return true;
                        }

                        auto EarlyStopError = FString{};
                        if (!Controller.TryStop(EarlyStopError))
                        {
                            _Test->AddError(FString::Printf(TEXT("Early ProcDump cancellation was rejected: %s"), *EarlyStopError));
                            return true;
                        }

                        const auto EarlyStopping = Controller.Get_Snapshot();
                        _Test->TestTrue(TEXT("early cancellation retains its child until attachment/cancellation completes"),
                            EarlyStopping.IsMonitoring);
                        _Test->TestTrue(TEXT("early cancellation is queued as stopping"), EarlyStopping.IsStopping);
                        _Deadline = Now + kControllerTimeoutSeconds;
                        _Phase = EPhase::AwaitEarlyStopped;
                        return false;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("ProcDump did not exit after bounded graceful cancellation. Status=[%s] Error=[%s]"),
                        *Stopped.Status,
                        *Stopped.LastError));
                    return true;
                }

                case EPhase::AwaitEarlyStopped:
                {
                    const auto Stopped = Controller.Get_Snapshot();
                    if (!Stopped.IsMonitoring)
                    {
                        _Test->TestFalse(TEXT("early stopped monitor clears cancellation-in-progress state"), Stopped.IsStopping);
                        _Test->TestFalse(TEXT("early stopped monitor clears readiness state"), Stopped.IsReady);
                        _Test->TestTrue(TEXT("early graceful cancellation reports no ProcDump error"), Stopped.LastError.IsEmpty());
                        return true;
                    }

                    if (Now < _Deadline)
                    { return false; }

                    _Test->AddError(FString::Printf(
                        TEXT("Early ProcDump cancellation did not finish. Status=[%s] Error=[%s]"),
                        *Stopped.Status,
                        *Stopped.LastError));
                    return true;
                }
            }

            _Test->AddError(TEXT("Hang Monitor test reached an unknown latent phase."));
            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitReady,
            AwaitStopped,
            AwaitEarlyStopped
        };

        auto RequestStop(FCkHangMonitorController& InController) -> void
        {
            auto IgnoredError = FString{};
            InController.TryStop(IgnoredError);
        }

        FAutomationTestBase* _Test = nullptr;
        FString _ProcDumpPath;
        double _Deadline = 0.0;
        EPhase _Phase = EPhase::Start;
    };
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_RejectsInvalidStartWithoutPartialState,
    "Ck.DebuggerLauncher.HangMonitor.RejectsInvalidStartWithoutPartialState",
    ck_hang_monitor_tests::kTestFlags)

bool FCkHangMonitor_RejectsInvalidStartWithoutPartialState::RunTest(const FString&)
{
    using namespace ck_hang_monitor_tests;

    auto Controller = FCkHangMonitorController{};
    Controller.Initialize();
    ON_SCOPE_EXIT
    {
        Controller.Shutdown();
    };

    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("fresh controller"));

    for (const auto InvalidCount : {0, 4})
    {
        auto Error = FString{};
        TestFalse(
            *FString::Printf(TEXT("invalid dump bound [%d] is rejected"), InvalidCount),
            Controller.TryStart(Get_MissingProcDumpPath(), InvalidCount, Error));
        TestFalse(
            *FString::Printf(TEXT("invalid dump bound [%d] reports its error"), InvalidCount),
            Error.IsEmpty());
        Test_IsStoppedWithoutPartialState(
            *this,
            Controller.Get_Snapshot(),
            *FString::Printf(TEXT("invalid dump bound [%d]"), InvalidCount));
    }

    auto MissingToolError = FString{};
    TestFalse(
        TEXT("missing ProcDump tool is rejected"),
        Controller.TryStart(Get_MissingProcDumpPath(), 1, MissingToolError));
    TestFalse(TEXT("missing ProcDump tool reports its error"), MissingToolError.IsEmpty());
    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("missing ProcDump tool"));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_RejectsStaleStopWithoutPartialState,
    "Ck.DebuggerLauncher.HangMonitor.RejectsStaleStopWithoutPartialState",
    ck_hang_monitor_tests::kTestFlags)

bool FCkHangMonitor_RejectsStaleStopWithoutPartialState::RunTest(const FString&)
{
    using namespace ck_hang_monitor_tests;

    auto Controller = FCkHangMonitorController{};
    Controller.Initialize();
    ON_SCOPE_EXIT
    {
        Controller.Shutdown();
    };

    auto Error = FString{};
    TestFalse(TEXT("stopping an inactive monitor is rejected"), Controller.TryStop(Error));
    TestFalse(TEXT("stale-stop rejection explains why"), Error.IsEmpty());
    Test_IsStoppedWithoutPartialState(*this, Controller.Get_Snapshot(), TEXT("stale stop"));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkHangMonitor_OptInCurrentProcessArmAndStop,
    "Ck.DebuggerLauncher.HangMonitor.OptInCurrentProcessArmAndStop",
    ck_hang_monitor_tests::kTestFlags)

bool FCkHangMonitor_OptInCurrentProcessArmAndStop::RunTest(const FString&)
{
    const auto ProcDumpPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_HANG_MONITOR_PROCDUMP"));
    if (ProcDumpPath.IsEmpty())
    {
        AddInfo(TEXT("CK_HANG_MONITOR_PROCDUMP is unavailable; opt-in ProcDump arm/stop verification was not run."));
        return true;
    }

    if (!IFileManager::Get().FileExists(*ProcDumpPath))
    {
        AddError(FString::Printf(
            TEXT("CK_HANG_MONITOR_PROCDUMP was set but is not an existing file: %s"),
            *ProcDumpPath));
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(
        ck_hang_monitor_tests::FCk_Latent_HangMonitorArmStop(this, ProcDumpPath));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
