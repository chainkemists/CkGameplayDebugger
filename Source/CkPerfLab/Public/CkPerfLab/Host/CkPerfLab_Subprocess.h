#pragma once

#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include <HAL/PlatformProcess.h>

// --------------------------------------------------------------------------------------------------------------------
// Launching and supervising the measurement child from the host editor.
//
// The child is the host's OWN executable. That is not a convenience: a project using
// TargetBuildEnvironment.Unique builds its own editor binary and module set, which the engine's
// stock UnrealEditor-Cmd.exe cannot resolve at all, so a path composed from the engine directory
// finds a binary that dies at PostConfigInit. Launching ExecutablePath() is correct for both build
// environments without having to know which one this is.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /** Why a run stopped, from the host's point of view. */
    enum class ECk_ChildOutcome : uint8
    {
        Running,
        ExitedCleanly,
        ExitedWithError,
        KilledByTimeout,
        KilledByUser,
        FailedToLaunch,
    };

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * One live measurement child. Non-blocking: the host polls it from whatever timer it already
     * runs, because the editor must stay usable for the whole run — that is the entire point of
     * measuring in a separate process.
     */
    class CKPERFLAB_API FCk_MeasurementChild
    {
    public:
        FCk_MeasurementChild() = default;
        ~FCk_MeasurementChild();

        FCk_MeasurementChild(const FCk_MeasurementChild&) = delete;
        auto operator=(const FCk_MeasurementChild&) -> FCk_MeasurementChild& = delete;

    public:
        /**
         * Write the request and spawn the child. Returns false when the process could not be
         * started at all, with the reason available from Get_FailureReason.
         */
        auto
        Try_Launch(
            const FCk_PerfLab_Request& InRequest) -> bool;

        /** Poll. Call from a timer; cheap enough to call every frame, but no need to. */
        auto
        Poll(
            float InDeltaSeconds) -> ECk_ChildOutcome;

        /** Ask the child to stop, then insist if it does not. */
        auto
        Request_Cancel() -> void;

    public:
        auto Get_Outcome() const -> ECk_ChildOutcome { return _Outcome; }
        auto Get_FailureReason() const -> const FString& { return _FailureReason; }
        auto Get_SessionDir() const -> FString;

        /** The most recent heartbeat the child wrote, or an empty one if it has not written yet. */
        auto Get_Heartbeat() const -> const FCk_PerfLab_Heartbeat& { return _Heartbeat; }

        /** The exact command line used, recorded so a failed run can be reproduced by hand. */
        auto Get_CommandLine() const -> const FString& { return _CommandLine; }

    private:
        auto DoCapture_ChildLogTail() -> void;

    private:
        FCk_PerfLab_Request   _Request;
        FCk_PerfLab_Heartbeat _Heartbeat;

        FProcHandle _ProcessHandle;

        FString _CommandLine;
        FString _FailureReason;

        ECk_ChildOutcome _Outcome = ECk_ChildOutcome::Running;

        float _ElapsedSec       = 0.0f;
        float _CancelGraceSec   = -1.0f;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Compose the child's command line. Pure and separately testable, because it is the piece most
     * likely to be silently wrong and least likely to be noticed until a run dies at boot.
     */
    CKPERFLAB_API auto
    Build_ChildCommandLine(
        const FString& InProjectPath,
        const FString& InMapPath,
        const FString& InRequestPath,
        const FString& InLogPath) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
