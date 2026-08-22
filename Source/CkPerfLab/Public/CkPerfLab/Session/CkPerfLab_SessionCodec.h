#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

// --------------------------------------------------------------------------------------------------------------------
// JSON for the three files a run exchanges. Two rules the whole design leans on:
//
//   Decoding is whole-or-nothing. An unrecognised schemaVersion, malformed JSON or a missing
//   required field fails the read outright rather than handing back a half-populated object — a
//   partially decoded session would be indistinguishable from a measured one.
//
//   Encoding is deterministic. Given the same object, the bytes are the same: fixed field order,
//   camelCase keys, enums written by identifier, arrays emitted in their stored order (which the
//   producers sort). Timestamps are carried on the object, never read from the clock here.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    inline constexpr int32 k_SchemaVersion = 1;

    // ----------------------------------------------------------------------------------------------------------------
    // The layout of a session directory is the wire format between two processes. It lives here, in
    // one place, because a typo in either half is a silent hang rather than a compile error: the
    // child writes a heartbeat nobody reads, and the host waits for one that never arrives.

    CKPERFLAB_API auto Get_SessionsRoot() -> FString;
    CKPERFLAB_API auto Get_SessionDirFor(const FString& InSessionId) -> FString;

    CKPERFLAB_API auto Get_SessionFilePath(const FString& InSessionDir) -> FString;
    CKPERFLAB_API auto Get_HeartbeatFilePath(const FString& InSessionDir) -> FString;
    CKPERFLAB_API auto Get_RequestFilePath(const FString& InSessionDir) -> FString;
    CKPERFLAB_API auto Get_CancelFlagFilePath(const FString& InSessionDir) -> FString;
    CKPERFLAB_API auto Get_ChildLogFilePath(const FString& InSessionDir) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    CKPERFLAB_API auto
    Write_RequestToJson(
        const FCk_PerfLab_Request& InRequest) -> FString;

    CKPERFLAB_API auto
    TryRead_RequestFromJson(
        const FString& InJson,
        FCk_PerfLab_Request& OutRequest) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    CKPERFLAB_API auto
    Write_HeartbeatToJson(
        const FCk_PerfLab_Heartbeat& InHeartbeat) -> FString;

    CKPERFLAB_API auto
    TryRead_HeartbeatFromJson(
        const FString& InJson,
        FCk_PerfLab_Heartbeat& OutHeartbeat) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    CKPERFLAB_API auto
    Write_SessionToJson(
        const FCk_PerfLab_Session& InSession) -> FString;

    CKPERFLAB_API auto
    TryRead_SessionFromJson(
        const FString& InJson,
        FCk_PerfLab_Session& OutSession) -> bool;

    /**
     * Read only the header fields — id, state, map, mode, timestamps — without decoding positions.
     * The session list shows dozens of rows and must not pay for every measurement to draw them.
     */
    CKPERFLAB_API auto
    TryRead_SessionSummaryFromJson(
        const FString& InJson,
        FCk_PerfLab_Session& OutSummary) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
