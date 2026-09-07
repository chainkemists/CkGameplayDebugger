#include "CkHangMonitor/CkHangMonitorController.h"

#include <HAL/FileManager.h>
#include <HAL/PlatformMisc.h>
#include <HAL/PlatformProcess.h>
#include <Misc/App.h>
#include <Misc/DateTime.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>

namespace ck_hang_monitor
{
    constexpr auto MaxAllowedDumps = 3;
    constexpr auto PollIntervalSeconds = 1.0f;

    auto Get_IsProcDumpExecutable(const FString& InPath) -> bool
    {
        return FPaths::GetCleanFilename(InPath).Equals(TEXT("procdump64.exe"), ESearchCase::IgnoreCase)
            && IFileManager::Get().FileExists(*InPath);
    }

    auto Get_OutputDirectory() -> FString
    {
        const auto Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
        return FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("Diagnostics/HangCaptures"),
            FString::Format(TEXT("{0}-{1}"), {Timestamp, FGuid::NewGuid().ToString(EGuidFormats::Digits)})));
    }

    auto Get_IsMonitorReadyOutput(const FString& InOutput) -> bool
    {
        return InOutput.Contains(TEXT("Press Ctrl-C to end monitoring"), ESearchCase::IgnoreCase);
    }

    auto Decode_StdOut(const TArray<uint8>& InBytes) -> FString
    {
        FString Result;
        Result.Reserve(InBytes.Num() / 2);
        for (int32 Index = 0; Index + 1 < InBytes.Num(); Index += 2)
        {
            Result.AppendChar(static_cast<TCHAR>(InBytes[Index] | (uint16(InBytes[Index + 1]) << 8)));
        }
        return Result;
    }

    auto Get_BuildConfigurationName() -> const TCHAR*
    {
        switch (FApp::GetBuildConfiguration())
        {
            case EBuildConfiguration::Debug: return TEXT("Debug");
            case EBuildConfiguration::DebugGame: return TEXT("DebugGame");
            case EBuildConfiguration::Development: return TEXT("Development");
            case EBuildConfiguration::Test: return TEXT("Test");
            case EBuildConfiguration::Shipping: return TEXT("Shipping");
            default: return TEXT("Unknown");
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkHangMonitorController::Initialize() -> void
{
    if (_TickerHandle.IsValid())
    { return; }

    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FCkHangMonitorController::DoTick),
        ck_hang_monitor::PollIntervalSeconds);
}

auto FCkHangMonitorController::Shutdown() -> void
{
    if (!_TickerHandle.IsValid())
    { return; }
    FString IgnoredError;
    TryStop(IgnoredError);

    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

    DoClose_Process(_MonitorProcess);
    DoClose_Process(_CancelProcess);
    DoClose_OutputPipes();
    _Snapshot.IsMonitoring = false;
    _Snapshot.IsReady = false;
    _Snapshot.IsStopping = false;
}

auto FCkHangMonitorController::Get_Snapshot() const -> FCkHangMonitorSnapshot
{ return _Snapshot; }

auto FCkHangMonitorController::TryStart(
    const FString& InProcDumpPath,
    const int32 InMaxDumps,
    FString& OutError)
    -> bool
{
    OutError.Reset();

    if (!_TickerHandle.IsValid())
    {
        OutError = TEXT("Hang monitor is not initialized.");
        return false;
    }

    if (_Snapshot.IsMonitoring || _Snapshot.IsStopping)
    {
        OutError = TEXT("Hang monitoring is already active or stopping.");
        return false;
    }

    if (InMaxDumps < 1 || InMaxDumps > ck_hang_monitor::MaxAllowedDumps)
    {
        OutError = TEXT("Dump count must be between 1 and 3.");
        return false;
    }

    if (!ck_hang_monitor::Get_IsProcDumpExecutable(InProcDumpPath))
    {
        OutError = TEXT("ProcDump must be an existing procdump64.exe executable.");
        return false;
    }

    const auto TargetPid = FPlatformProcess::GetCurrentProcessId();
    if (TargetPid == 0)
    {
        OutError = TEXT("Could not determine the current game process id.");
        return false;
    }

    const auto OutputDirectory = ck_hang_monitor::Get_OutputDirectory();
    if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
    {
        OutError = TEXT("Could not create the hang-capture output directory.");
        return false;
    }

    void* StdOutRead = nullptr;
    void* StdOutWrite = nullptr;
    void* StdErrRead = nullptr;
    void* StdErrWrite = nullptr;
    if (!FPlatformProcess::CreatePipe(StdOutRead, StdOutWrite)
        || !FPlatformProcess::CreatePipe(StdErrRead, StdErrWrite))
    {
        FPlatformProcess::ClosePipe(StdOutRead, StdOutWrite);
        FPlatformProcess::ClosePipe(StdErrRead, StdErrWrite);
        IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
        OutError = TEXT("Could not create the ProcDump output pipes.");
        return false;
    }

    const auto DumpStem = FPaths::Combine(OutputDirectory, TEXT("HangCapture"));
    const auto Arguments = FString::Format(
        TEXT("-accepteula -h -n {0} -mm {1} \"{2}\""),
        {InMaxDumps, TargetPid, DumpStem});
    auto Process = FPlatformProcess::CreateProc(
        *InProcDumpPath,
        *Arguments,
        false,
        true,
        true,
        nullptr,
        0,
        nullptr,
        StdOutWrite,
        nullptr,
        StdErrWrite);

    if (!Process.IsValid())
    {
        FPlatformProcess::ClosePipe(StdOutRead, StdOutWrite);
        FPlatformProcess::ClosePipe(StdErrRead, StdErrWrite);
        IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
        OutError = TEXT("ProcDump could not be started.");
        return false;
    }

    // The child inherits this handle. The parent only retains the read end for bounded polling.
    FPlatformProcess::ClosePipe(nullptr, StdOutWrite);
    FPlatformProcess::ClosePipe(nullptr, StdErrWrite);

    _MonitorProcess = MoveTemp(Process);
    _StdOutPipeRead = StdOutRead;
    _StdErrPipeRead = StdErrRead;
    _StdOutTail.Reset();
    _StdErrTail.Reset();
    _CancelIssued = false;
    _StartedUtc = FDateTime::UtcNow().ToIso8601();
    _Snapshot = FCkHangMonitorSnapshot{};
    _Snapshot.IsMonitoring = true;
    _Snapshot.Status = TEXT("Starting ProcDump hang monitor...");
    _Snapshot.OutputDirectory = OutputDirectory;
    _Snapshot.ProcDumpPath = InProcDumpPath;
    _Snapshot.TargetPid = TargetPid;
    _Snapshot.MaxDumps = InMaxDumps;
    DoWrite_Manifest();
    return true;
}

auto FCkHangMonitorController::TryStop(FString& OutError) -> bool
{
    OutError.Reset();

    if (!_Snapshot.IsMonitoring)
    {
        OutError = TEXT("Hang monitoring is not active.");
        return false;
    }

    if (_Snapshot.IsStopping && _CancelIssued)
    { return true; }

    // ProcDump's PID cancellation event does not exist until it has attached.
    // Preserve an early Stop request and issue it as soon as attachment is confirmed.
    if (!_Snapshot.IsReady)
    {
        _Snapshot.IsStopping = true;
        _Snapshot.Status = TEXT("Stop requested; waiting for ProcDump attachment...");
        DoWrite_Manifest();
        return true;
    }

    const auto Arguments = FString::Format(TEXT("-accepteula -cancel {0}"), {_Snapshot.TargetPid});
    auto CancelProcess = FPlatformProcess::CreateProc(
        *_Snapshot.ProcDumpPath,
        *Arguments,
        false,
        true,
        true,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (!CancelProcess.IsValid())
    {
        OutError = TEXT("ProcDump cancellation command could not be started.");
        _Snapshot.LastError = OutError;
        DoWrite_Manifest();
        return false;
    }

    _CancelProcess = MoveTemp(CancelProcess);
    _CancelIssued = true;
    _Snapshot.IsStopping = true;
    _Snapshot.LastError.Reset();
    _Snapshot.Status = TEXT("Cancelling ProcDump hang monitor...");
    DoWrite_Manifest();
    return true;
}

auto FCkHangMonitorController::Get_DefaultProcDumpPath() -> FString
{
    auto LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    if (LocalAppData.IsEmpty())
    { LocalAppData = FPlatformProcess::UserSettingsDir(); }
    return FPaths::Combine(LocalAppData, TEXT("CkDiagnostics/ProcDump/procdump64.exe"));
}

auto FCkHangMonitorController::DoTick(const float InDeltaSeconds) -> bool
{
    if (!_Snapshot.IsMonitoring)
    { return true; }

    if (_StdOutPipeRead != nullptr)
    {
        auto NewStdOut = TArray<uint8>{};
        FPlatformProcess::ReadPipeToArray(_StdOutPipeRead, NewStdOut);
        _StdOutTail.Append(NewStdOut);
        if (_StdOutTail.Num() > 8192)
        { _StdOutTail.RemoveAt(0, ((_StdOutTail.Num() - 8192) / 2) * 2, EAllowShrinking::No); }
        if (!NewStdOut.IsEmpty())
        { DoWrite_OutputTail(); }
        if (!_Snapshot.IsReady && ck_hang_monitor::Get_IsMonitorReadyOutput(ck_hang_monitor::Decode_StdOut(_StdOutTail)))
        {
            _Snapshot.IsReady = true;
            if (!_Snapshot.IsStopping)
            { _Snapshot.Status = TEXT("Monitoring current game process for hangs."); }
            DoWrite_Manifest();
        }
    }

    if (_StdErrPipeRead != nullptr)
    {
        const auto NewStdErr = FPlatformProcess::ReadPipe(_StdErrPipeRead);
        _StdErrTail += NewStdErr;
        _StdErrTail = _StdErrTail.Right(4096);
        if (!NewStdErr.IsEmpty())
        { FFileHelper::SaveStringToFile(_StdErrTail, *FPaths::Combine(_Snapshot.OutputDirectory, TEXT("procdump.stderr.log"))); }
    }

    DoRefresh_DumpCount();

    if (_Snapshot.IsReady && _Snapshot.IsStopping && !_CancelIssued)
    {
        FString Error;
        if (!TryStop(Error))
        { _Snapshot.IsStopping = false; }
    }

    if (_CancelProcess.IsValid() && !FPlatformProcess::IsProcRunning(_CancelProcess))
    {
        int32 CancelCode = 0;
        FPlatformProcess::GetProcReturnCode(_CancelProcess, &CancelCode);
        DoClose_Process(_CancelProcess);
        if (CancelCode != 0 && FPlatformProcess::IsProcRunning(_MonitorProcess))
        {
            _Snapshot.IsStopping = false;
            _CancelIssued = false;
            _Snapshot.LastError = FString::Printf(TEXT("ProcDump cancellation exited with code %d. Stop can be retried."), CancelCode);
            DoWrite_Manifest();
        }
    }

    if (_MonitorProcess.IsValid() && !FPlatformProcess::IsProcRunning(_MonitorProcess))
    {
        int32 ReturnCode = 0;
        FPlatformProcess::GetProcReturnCode(_MonitorProcess, &ReturnCode);
        DoClose_Process(_MonitorProcess);
        DoClose_OutputPipes();

        _Snapshot.IsMonitoring = false;
        _Snapshot.IsReady = false;
        _Snapshot.IsStopping = false;
        if (ReturnCode == 0 || (_Snapshot.NumDumps >= _Snapshot.MaxDumps
            && ck_hang_monitor::Decode_StdOut(_StdOutTail).Contains(TEXT("Dump count reached"))))
        {
            _Snapshot.LastError.Reset();
            _Snapshot.Status = _Snapshot.NumDumps >= _Snapshot.MaxDumps
                ? TEXT("Capture complete. Dump budget reached.") : TEXT("ProcDump hang monitor stopped.");
        }
        else
        {
            _Snapshot.Status = TEXT("ProcDump hang monitor exited unexpectedly.");
            _Snapshot.LastError = FString::Format(TEXT("ProcDump exited with code {0}: {1}"),
                {ReturnCode, (_StdErrTail + ck_hang_monitor::Decode_StdOut(_StdOutTail)).Right(4096).TrimStartAndEnd()});
        }
        DoWrite_Manifest();
    }

    return true;
}

auto FCkHangMonitorController::DoClose_Process(FProcHandle& InOutProcess) -> void
{
    if (InOutProcess.IsValid())
    { FPlatformProcess::CloseProc(InOutProcess); }
}

auto FCkHangMonitorController::DoClose_OutputPipes() -> void
{
    if (_StdOutPipeRead != nullptr)
    { FPlatformProcess::ClosePipe(_StdOutPipeRead, nullptr); }
    if (_StdErrPipeRead != nullptr)
    { FPlatformProcess::ClosePipe(_StdErrPipeRead, nullptr); }
    _StdOutPipeRead = nullptr;
    _StdErrPipeRead = nullptr;
}

auto FCkHangMonitorController::DoRefresh_DumpCount() -> void
{
    auto DumpPaths = TArray<FString>{};
    IFileManager::Get().FindFilesRecursive(DumpPaths, *_Snapshot.OutputDirectory, TEXT("*.dmp"), true, false);
    _Snapshot.NumDumps = DumpPaths.Num();
}

auto FCkHangMonitorController::DoWrite_Manifest() const -> void
{
    if (_Snapshot.OutputDirectory.IsEmpty())
    { return; }

    const auto Manifest = FString::Format(TEXT("startedUtc={0}\nstate={1}\ntargetPid={2}\ntargetExecutable={3}\ntargetBuildConfiguration={4}\nprocDumpPath={5}\noutputDirectory={6}\nmaxDumps={7}\nnumDumps={8}\nbuildVersion={9}\nlastError={10}\n"),
        {
            _StartedUtc,
            _Snapshot.IsMonitoring
                ? (_Snapshot.IsStopping
                    ? TEXT("stopping")
                    : (_Snapshot.IsReady ? TEXT("monitoring") : TEXT("starting")))
                : TEXT("stopped"),
            _Snapshot.TargetPid,
            FPaths::ConvertRelativePathToFull(FPlatformProcess::ExecutablePath()),
            ck_hang_monitor::Get_BuildConfigurationName(),
            _Snapshot.ProcDumpPath,
            _Snapshot.OutputDirectory,
            _Snapshot.MaxDumps,
            _Snapshot.NumDumps,
            FApp::GetBuildVersion(),
            _Snapshot.LastError
        });
    FFileHelper::SaveStringToFile(Manifest, *FPaths::Combine(_Snapshot.OutputDirectory, TEXT("manifest.txt")));
}

auto FCkHangMonitorController::DoWrite_OutputTail() const -> void
{
    if (!_Snapshot.OutputDirectory.IsEmpty())
    {
        FFileHelper::SaveArrayToFile(
            _StdOutTail,
            *FPaths::Combine(_Snapshot.OutputDirectory, TEXT("procdump.stdout.utf16le.log")));
    }
}
