#pragma once

#include "CoreMinimal.h"

#include <Containers/Ticker.h>
#include <HAL/PlatformProcess.h>

// --------------------------------------------------------------------------------------------------------------------

struct CKHANGMONITOR_API FCkHangMonitorSnapshot
{
    bool    IsMonitoring = false;
    bool    IsReady = false;
    bool    IsStopping = false;
    FString Status;
    FString LastError;
    FString OutputDirectory;
    FString ProcDumpPath;
    uint32  TargetPid = 0;
    int32   NumDumps = 0;
    int32   MaxDumps = 1;
};

// --------------------------------------------------------------------------------------------------------------------

class CKHANGMONITOR_API FCkHangMonitorController
{
public:
    ~FCkHangMonitorController() { Shutdown(); }
    FCkHangMonitorController() = default;
    FCkHangMonitorController(const FCkHangMonitorController&) = delete;
    FCkHangMonitorController& operator=(const FCkHangMonitorController&) = delete;
    auto Initialize() -> void;
    auto Shutdown() -> void;

    auto Get_Snapshot() const -> FCkHangMonitorSnapshot;

    auto TryStart(
        const FString& InProcDumpPath,
        int32 InMaxDumps,
        FString& OutError)
        -> bool;

    auto TryStop(FString& OutError) -> bool;

    static auto Get_DefaultProcDumpPath() -> FString;

private:
    auto DoTick(float InDeltaSeconds) -> bool;
    auto DoClose_Process(FProcHandle& InOutProcess) -> void;
    auto DoClose_OutputPipes() -> void;
    auto DoRefresh_DumpCount() -> void;
    auto DoWrite_Manifest() const -> void;
    auto DoWrite_OutputTail() const -> void;

    FCkHangMonitorSnapshot _Snapshot;
    FProcHandle             _MonitorProcess;
    FProcHandle             _CancelProcess;
    void*                   _StdOutPipeRead = nullptr;
    void*                   _StdErrPipeRead = nullptr;
    TArray<uint8>           _StdOutTail;
    FString                 _StdErrTail;
    FString                 _StartedUtc;
    bool                    _CancelIssued = false;
    FTSTicker::FDelegateHandle _TickerHandle;
};

// --------------------------------------------------------------------------------------------------------------------
