#include "CkPerfLab_Subprocess.h"

#include "CkPerfLab_Log.h"

#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_subprocess
{
    // How long a cancelled child is given to write its final session before it is terminated.
    constexpr auto k_CancelGraceSec = 10.0f;

    // The same courtesy at teardown, but far shorter: this one blocks the game thread while the
    // editor closes the window, so it buys a chance at a readable session without being felt as a
    // hang. One second total.
    constexpr auto k_TeardownGraceTicks   = 20;
    constexpr auto k_TeardownGraceTickSec = 0.05f;

    // How much of a dead child's log to surface. A child that dies before its first heartbeat is
    // otherwise completely mute, and the reason is always in the last few lines.
    constexpr auto k_LogTailLines = 25;

}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Build_ChildCommandLine(
            const FString& InProjectPath,
            const FString& InMapPath,
            const FString& InRequestPath,
            const FString& InLogPath)
        -> FString
    {
        // Paths are quoted because spaces in them are the norm on Windows, and an unquoted one fails
        // as a mangled map name rather than as an obvious error.
        //
        // The VALUE is quoted, never the whole token. FParse::Value locates its key with
        // FCString::Strifind(..., bSkipQuotedChars = true), so a key sitting inside a quoted region
        // is skipped and the switch reads as absent: the child boots, finds no request, stays inert,
        // and the host waits out its entire timeout for a run that never started. -abslog already
        // uses the value-quoted form, and this one has to match it.
        return FString::Printf(
            TEXT("\"%s\" \"%s\" -game -windowed -resx=1280 -resy=720 -unattended -nosplash -nopause ")
            TEXT("-SCCProvider=None -abslog=\"%s\" -CkPerfLab-Request=\"%s\""),
            *InProjectPath,
            *InMapPath,
            *InLogPath,
            *InRequestPath);
    }

    // ----------------------------------------------------------------------------------------------------------------

    FCk_MeasurementChild::~FCk_MeasurementChild()
    {
        // Never leave an orphan behind: a measurement child holds a rendering window and would
        // outlive the editor that forgot it.
        //
        // The cancel flag goes out FIRST so a child mid-measurement gets the chance to finish writing its session
        // before it is killed. Without it, abandoning a run leaves a directory with a request and no session, which
        // the store lists forever as an unreadable row. The wait is deliberately short — this runs on the game
        // thread during teardown, so a long block would stall the editor closing.
        if (_ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(_ProcessHandle))
        {
            Request_Cancel();

            for (auto Attempt = 0; Attempt < ck_perf_lab_subprocess::k_TeardownGraceTicks; ++Attempt)
            {
                if (NOT FPlatformProcess::IsProcRunning(_ProcessHandle))
                { break; }

                FPlatformProcess::Sleep(ck_perf_lab_subprocess::k_TeardownGraceTickSec);
            }
        }

        if (_ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(_ProcessHandle))
        {
            FPlatformProcess::TerminateProc(_ProcessHandle, true);
        }

        if (_ProcessHandle.IsValid())
        {
            FPlatformProcess::CloseProc(_ProcessHandle);
        }
    }

    auto
        FCk_MeasurementChild::
        Get_SessionDir() const
        -> FString
    {
        return Get_SessionDirFor(_Request.Get_SessionId());
    }

    auto
        FCk_MeasurementChild::
        Try_Launch(
            const FCk_PerfLab_Request& InRequest)
        -> bool
    {
        _Request = InRequest;

        const auto SessionDir  = Get_SessionDir();
        const auto RequestPath = Get_RequestFilePath(SessionDir);

        if (NOT FFileHelper::SaveStringToFile(Write_RequestToJson(_Request), *RequestPath))
        {
            _Outcome       = ECk_ChildOutcome::FailedToLaunch;
            _FailureReason = FString::Printf(TEXT("could not write the request to [%s]"), *RequestPath);
            return false;
        }

        const auto Executable = FPlatformProcess::ExecutablePath();

        _CommandLine = Build_ChildCommandLine(
            FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),
            _Request.Get_MapPath(),
            RequestPath,
            Get_ChildLogFilePath(SessionDir));

        _ProcessHandle = FPlatformProcess::CreateProc(
            Executable, *_CommandLine, true, false, false, nullptr, 0, nullptr, nullptr);

        if (NOT _ProcessHandle.IsValid())
        {
            _Outcome       = ECk_ChildOutcome::FailedToLaunch;
            _FailureReason = FString::Printf(TEXT("could not start [%s]"), Executable);
            return false;
        }

        _Outcome = ECk_ChildOutcome::Running;

        ck::perf_lab::Log(TEXT("PerfLab launched child [{}] with [{}]"), Executable, _CommandLine);

        return true;
    }

    auto
        FCk_MeasurementChild::
        DoCapture_ChildLogTail()
        -> void
    {
        // A child that died before its first heartbeat has told us nothing through the session
        // files, so its log is the only evidence of why. Surfacing the tail turns a silent dead
        // process into something diagnosable.
        auto Contents = FString{};

        if (NOT FFileHelper::LoadFileToString(Contents,
                *Get_ChildLogFilePath(Get_SessionDir())))
        {
            return;
        }

        auto Lines = TArray<FString>{};
        Contents.ParseIntoArrayLines(Lines);

        const auto Tail = ck::algo::TakeLast(Lines, ck_perf_lab_subprocess::k_LogTailLines);

        _FailureReason += FString::Printf(TEXT("\n--- last %d lines of the child log ---\n%s"),
            Tail.Num(), *FString::Join(Tail, TEXT("\n")));
    }

    auto
        FCk_MeasurementChild::
        Poll(
            float InDeltaSeconds)
        -> ECk_ChildOutcome
    {
        if (_Outcome != ECk_ChildOutcome::Running)
        {
            return _Outcome;
        }

        _ElapsedSec += InDeltaSeconds;

        auto HeartbeatJson = FString{};

        if (FFileHelper::LoadFileToString(HeartbeatJson,
                *Get_HeartbeatFilePath(Get_SessionDir())))
        {
            // A partially written heartbeat is expected — the child rewrites it while we read — so a
            // failed decode is simply last frame's state, not an error.
            TryRead_HeartbeatFromJson(HeartbeatJson, _Heartbeat);
        }

        if (_CancelGraceSec >= 0.0f)
        {
            _CancelGraceSec -= InDeltaSeconds;

            if (_CancelGraceSec <= 0.0f && FPlatformProcess::IsProcRunning(_ProcessHandle))
            {
                FPlatformProcess::TerminateProc(_ProcessHandle, true);
                _Outcome = ECk_ChildOutcome::KilledByUser;
                return _Outcome;
            }
        }

        // The host's own deadline, separate from the child's internal one. Both exist because either
        // can be the thing that wedged.
        const auto HostDeadlineSec = static_cast<float>(_Request.Get_ChildWallClockBudgetSec()) * 1.5f;

        if (_ElapsedSec > HostDeadlineSec)
        {
            FPlatformProcess::TerminateProc(_ProcessHandle, true);
            _Outcome       = ECk_ChildOutcome::KilledByTimeout;
            _FailureReason = FString::Printf(
                TEXT("the child outlived the host deadline of %.0f seconds"), HostDeadlineSec);
            DoCapture_ChildLogTail();
            return _Outcome;
        }

        if (FPlatformProcess::IsProcRunning(_ProcessHandle))
        {
            return ECk_ChildOutcome::Running;
        }

        auto ReturnCode = int32{0};
        FPlatformProcess::GetProcReturnCode(_ProcessHandle, &ReturnCode);

        if (ReturnCode == 0)
        {
            _Outcome = ECk_ChildOutcome::ExitedCleanly;
            return _Outcome;
        }

        // A child that exits non-zero DURING the cancel grace window was obeying the cancel, not
        // failing. Reporting an error here would tell the user their own Cancel press went wrong.
        if (_CancelGraceSec >= 0.0f)
        {
            _Outcome = ECk_ChildOutcome::KilledByUser;
            return _Outcome;
        }

        _Outcome       = ECk_ChildOutcome::ExitedWithError;
        _FailureReason = FString::Printf(TEXT("the child exited with code %d"), ReturnCode);
        DoCapture_ChildLogTail();

        return _Outcome;
    }

    auto
        FCk_MeasurementChild::
        Request_Cancel()
        -> void
    {
        if (_Outcome != ECk_ChildOutcome::Running)
        {
            return;
        }

        // Ask first: a child given a moment to finish writes a session that says it was cancelled,
        // where one killed outright leaves a file nobody can explain.
        FFileHelper::SaveStringToFile(FString{TEXT("cancel")},
            *Get_CancelFlagFilePath(Get_SessionDir()));

        _CancelGraceSec = ck_perf_lab_subprocess::k_CancelGraceSec;
    }
}

// --------------------------------------------------------------------------------------------------------------------
