#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Argus Debug Protocol — Frame & Message Types
// Protocol version 1 — must match Argus client (D:\Repositories\Argus\src\protocol)
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{
    // Frame header constants (big-endian on wire)
    constexpr uint32 FRAME_MAGIC   = 0x41524753; // "ARGS"
    constexpr uint16 FRAME_VERSION = 0x0001;
    constexpr int32  FRAME_HEADER_SIZE = 10;      // 4 magic + 2 version + 4 length

    // Server identity
    constexpr uint32 SERVER_VERSION = 0x010000;   // v1.0.0

    // Default listen port
    constexpr uint16 DEFAULT_PORT = 47522;

    // --------------------------------------------------------------------------------------------------------------------
    // Parsed frame header
    // --------------------------------------------------------------------------------------------------------------------

    struct FFrameHeader
    {
        uint32 Magic          = 0;
        uint16 Version        = 0;
        uint32 PayloadLength  = 0;
    };

    // --------------------------------------------------------------------------------------------------------------------
    // Message structs — field order matches Argus MSGPACK_DEFINE declarations exactly
    // --------------------------------------------------------------------------------------------------------------------

    /** Client → Server: first message after TCP connect. */
    struct FHandshakeRequest
    {
        uint32  ClientVersion        = 0;       // e.g. 0x020000
        FString ClientName;                     // "Argus"
        uint32  ExpectedEnttVersion  = 0;       // e.g. 0x031000 for 3.16.0
    };

    /** Per-world registry metadata. */
    struct FRegistryInfo
    {
        uint32  WorldId          = 0;
        FString WorldName;
        uint64  RegistryAddress  = 0;           // Raw entt::registry* for ReadProcessMemory
        uint32  EntityCount      = 0;
        uint8   NetMode          = 0;           // 0=Standalone,1=DedServer,2=ListenServer,3=Client
    };

    /** Single UPROPERTY field within a component struct. */
    struct FPropertyInfo
    {
        FString Name;
        FString TypeName;
        uint64  Offset = 0;
        uint64  Size   = 0;
    };

    /** A registered ECS fragment/component type with its reflected properties. */
    struct FComponentTypeInfo
    {
        FString                 TypeName;
        TArray<FPropertyInfo>   Properties;
    };

    /** Server → Client: response to HandshakeRequest. */
    struct FHandshakeResponse
    {
        bool                        bAccepted       = false;
        uint32                      ServerVersion   = SERVER_VERSION;
        FString                     ProjectName;
        FString                     EngineVersion;
        uint32                      ProcessId       = 0;
        uint32                      EnttVersion     = 0;
        TArray<FRegistryInfo>       Registries;
        TArray<FComponentTypeInfo>  ComponentTypes;
    };
}
