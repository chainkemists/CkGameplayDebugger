#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Argus Debug Protocol — Frame & Message Types
// Protocol version 1 — must match Argus client (D:\Repositories\Argus\src\protocol)
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{
    constexpr uint32 FRAME_MAGIC       = 0x41524753; // "ARGS"
    constexpr uint16 FRAME_VERSION     = 0x0001;
    constexpr int32  FRAME_HEADER_SIZE = 10;         // 4 magic + 2 version + 4 length

    constexpr uint32 SERVER_VERSION    = 0x010000;   // v1.0.0
    constexpr uint16 DEFAULT_PORT      = 47522;

    // --------------------------------------------------------------------------------------------------------------------

    struct FFrameHeader
    {
        uint32 Magic         = 0;
        uint16 Version       = 0;
        uint32 PayloadLength = 0;
    };

    // --------------------------------------------------------------------------------------------------------------------
    // Message structs — field order matches Argus MSGPACK_DEFINE declarations exactly
    // --------------------------------------------------------------------------------------------------------------------

    struct FHandshakeRequest
    {
        uint32  ClientVersion       = 0;
        FString ClientName;
        uint32  ExpectedEnttVersion = 0;
    };

    struct FRegistryInfo
    {
        uint32  WorldId         = 0;
        FString WorldName;
        uint64  RegistryAddress = 0;
        uint32  EntityCount     = 0;
        uint8   NetMode         = 0;
    };

    struct FPropertyInfo
    {
        FString Name;
        FString TypeName;
        uint64  Offset = 0;
        uint64  Size   = 0;
    };

    struct FComponentTypeInfo
    {
        FString               TypeName;
        TArray<FPropertyInfo> Properties;
    };

    struct FHandshakeResponse
    {
        bool                       Accepted      = false;
        uint32                     ServerVersion = SERVER_VERSION;
        FString                    ProjectName;
        FString                    EngineVersion;
        uint32                     ProcessId     = 0;
        uint32                     EnttVersion   = 0;
        TArray<FRegistryInfo>      Registries;
        TArray<FComponentTypeInfo> ComponentTypes;
    };
}