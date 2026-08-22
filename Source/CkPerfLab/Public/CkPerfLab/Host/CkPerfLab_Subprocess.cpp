#include "CkPerfLab_Subprocess.h"

#include "CkPerfLab_Log.h"

#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_subprocess
{
    // How long a cancelled child is given to write its final session before it is terminated.
    constexpr auto k_CancelGraceSec = 10.0f;

    // How much of a dead child's log to surface. A child that dies before its first heartbeat is
    // otherwise completely mute, and the reason is always in the last few lines.
    constexpr auto k_LogTailLines = 25;

    auto
        Get_LogPathFor(
            const FString& InSessionDir)
        -> FString
    {
        return FPaths::Combine(InSessionDir, TEXT("child.log"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Get_SessionDirFor(
            const FString& InSessionId)
        -> FString
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CkPerfLab"), TEXT("Sessions"), InSessionId));
    }

    auto
        Build_ChildCommandLine(
            const FString& InProjectPath,
            const FString& InMapPath,
            const FString& InRequestPath,
            const FString& InLogPath)
        -> FString
    {
        // Every path is quoted: spaces in project paths are the norm on Windows, and an unquoted one
        // fails as a mangled map name rather than as an obvious error.
        //
        // -abslog keeps the child out of Saved/Logs, which matters because the repo's tooling probes
        // an exclusive lock on the project log to decide whether an editor is running.
        return FString::Printf(
            TEXT("\"%s\" \"%s\" -game -windowed -resx=1280 -resy=720 -unattended -nosplash -nopause ")
            TEXT("-SCCProvider=None -abslog=\"%s\" \"-CkPerfLab-Request=%s\""),
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
        const auto RequestPath = FPaths::Combine(SessionDir, TEXT("request.json"));

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
            ck_perf_lab_subprocess::Get_LogPathFor(SessionDir));

        _ProcessHandle = FPlatformProcess::CreateProc(
            Executable, *_CommandLine, true, false, false, nullptr, 0, nullptr, nullptr);

        if (NOT _ProcessHandle.IsValid())
        {
            _Outcome       = ECk_ChildOutcome::FailedToLaunch;
            _FailureReason = FString::Printf(TEXT("could not start [%s]"), Executable);
            return false;
        }

        _Outcome = ECk_ChildOutcome::Running;

        ck::perf_lab::Log(TEXT("PerfLab launched child [{}] {}"), Executable, _CommandLine);

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
                *ck_perf_lab_subprocess::Get_LogPathFor(Get_SessionDir())))
        {
            return;
        }

        auto Lines = TArray<FString>{};
        Contents.ParseIntoArrayLines(Lines);

        const auto First = FMath::Max(0, Lines.Num() - ck_perf_lab_subprocess::k_LogTailLines);

        auto Tail = TArray<FString>{};

        for (auto Index = First; Index < Lines.Num(); ++Index)
        {
            Tail.Add(Lines[Index]);
        }

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
                *FPaths::Combine(Get_SessionDir(), TEXT("heartbeat.json"))))
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
            *FPaths::Combine(Get_SessionDir(), TEXT("cancel.flag")));

        _CancelGraceSec = ck_perf_lab_subprocess::k_CancelGraceSec;
    }
}

// --------------------------------------------------------------------------------------------------------------------
