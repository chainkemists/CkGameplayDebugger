#include "ArgusDebugTcpServer.h"
#include "ArgusMsgPack.h"

#include "Common/TcpListener.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogArgusServer, Log, All);

// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

// ====================================================================================================================
// Frame helpers (shared by server — mirrors Argus frame.cpp)
// ====================================================================================================================

static auto BuildFrame(const TArray<uint8>& Payload) -> TArray<uint8>
{
    TArray<uint8> Frame;
    Frame.SetNumUninitialized(FRAME_HEADER_SIZE + Payload.Num());

    // Magic (4 bytes, big-endian)
    Frame[0] = static_cast<uint8>((FRAME_MAGIC >> 24) & 0xFF);
    Frame[1] = static_cast<uint8>((FRAME_MAGIC >> 16) & 0xFF);
    Frame[2] = static_cast<uint8>((FRAME_MAGIC >> 8) & 0xFF);
    Frame[3] = static_cast<uint8>(FRAME_MAGIC & 0xFF);

    // Version (2 bytes, big-endian)
    Frame[4] = static_cast<uint8>((FRAME_VERSION >> 8) & 0xFF);
    Frame[5] = static_cast<uint8>(FRAME_VERSION & 0xFF);

    // Payload length (4 bytes, big-endian)
    const uint32 Len = static_cast<uint32>(Payload.Num());
    Frame[6] = static_cast<uint8>((Len >> 24) & 0xFF);
    Frame[7] = static_cast<uint8>((Len >> 16) & 0xFF);
    Frame[8] = static_cast<uint8>((Len >> 8) & 0xFF);
    Frame[9] = static_cast<uint8>(Len & 0xFF);

    // Copy payload
    if (Payload.Num() > 0)
    {
        FMemory::Memcpy(Frame.GetData() + FRAME_HEADER_SIZE, Payload.GetData(), Payload.Num());
    }

    return Frame;
}

static auto ParseFrameHeader(const uint8* HeaderBytes) -> FFrameHeader
{
    FFrameHeader Header;

    Header.Magic = (static_cast<uint32>(HeaderBytes[0]) << 24) |
                   (static_cast<uint32>(HeaderBytes[1]) << 16) |
                   (static_cast<uint32>(HeaderBytes[2]) << 8) |
                    static_cast<uint32>(HeaderBytes[3]);

    Header.Version = (static_cast<uint16>(HeaderBytes[4]) << 8) |
                      static_cast<uint16>(HeaderBytes[5]);

    Header.PayloadLength = (static_cast<uint32>(HeaderBytes[6]) << 24) |
                           (static_cast<uint32>(HeaderBytes[7]) << 16) |
                           (static_cast<uint32>(HeaderBytes[8]) << 8) |
                            static_cast<uint32>(HeaderBytes[9]);

    return Header;
}

// ====================================================================================================================
// Serialization helpers
// ====================================================================================================================

static auto SerializeHandshakeResponse(const FHandshakeResponse& Resp) -> TArray<uint8>
{
    FMsgPackWriter W;
    W.Reserve(512);

    // HandshakeResponse = array[8]
    W.WriteArrayHeader(8);

    // [0] accepted: bool
    W.WriteBool(Resp.bAccepted);

    // [1] server_version: uint32
    W.WriteUInt32(Resp.ServerVersion);

    // [2] project_name: string
    W.WriteString(Resp.ProjectName);

    // [3] engine_version: string
    W.WriteString(Resp.EngineVersion);

    // [4] process_id: uint32
    W.WriteUInt32(Resp.ProcessId);

    // [5] entt_version: uint32
    W.WriteUInt32(Resp.EnttVersion);

    // [6] registries: array of RegistryInfo
    W.WriteArrayHeader(static_cast<uint32>(Resp.Registries.Num()));
    for (const auto& Reg : Resp.Registries)
    {
        // RegistryInfo = array[5]
        W.WriteArrayHeader(5);
        W.WriteUInt32(Reg.WorldId);
        W.WriteString(Reg.WorldName);
        W.WriteUInt64(Reg.RegistryAddress);
        W.WriteUInt32(Reg.EntityCount);
        W.WriteUInt8(Reg.NetMode);
    }

    // [7] component_types: array of ComponentTypeInfo
    W.WriteArrayHeader(static_cast<uint32>(Resp.ComponentTypes.Num()));
    for (const auto& Comp : Resp.ComponentTypes)
    {
        // ComponentTypeInfo = array[2]
        W.WriteArrayHeader(2);
        W.WriteString(Comp.TypeName);

        // properties: array of PropertyInfo
        W.WriteArrayHeader(static_cast<uint32>(Comp.Properties.Num()));
        for (const auto& Prop : Comp.Properties)
        {
            // PropertyInfo = array[4]
            W.WriteArrayHeader(4);
            W.WriteString(Prop.Name);
            W.WriteString(Prop.TypeName);
            W.WriteUInt64(Prop.Offset);
            W.WriteUInt64(Prop.Size);
        }
    }

    return W.MoveBuffer();
}

static auto DeserializeHandshakeRequest(const uint8* Data, int32 Size) -> TOptional<FHandshakeRequest>
{
    FMsgPackReader R(Data, Size);

    const uint32 ArrayCount = R.ReadArrayHeader();
    if (R.IsError() || ArrayCount < 3)
    {
        return {};
    }

    FHandshakeRequest Req;
    Req.ClientVersion       = R.ReadUInt32();
    Req.ClientName          = R.ReadString();
    Req.ExpectedEnttVersion = R.ReadUInt32();

    if (R.IsError())
    {
        return {};
    }

    return Req;
}

// ====================================================================================================================
// FDebugTcpServer
// ====================================================================================================================

FDebugTcpServer::FDebugTcpServer()  = default;

FDebugTcpServer::~FDebugTcpServer()
{
    StopListening();
}

auto FDebugTcpServer::StartListening(uint16 Port) -> bool
{
    if (bListening)
    {
        return true;
    }

    const auto Endpoint = FIPv4Endpoint(FIPv4Address(127, 0, 0, 1), Port);

    // FTcpListener auto-starts on construction (spawns thread -> Init -> Run).
    constexpr auto Reusable = false;
    auto PendingListener = MakeUnique<FTcpListener>(
        Endpoint,
        FTimespan::FromSeconds(1.0),
        Reusable);

    PendingListener->OnConnectionAccepted().BindRaw(this, &FDebugTcpServer::HandleConnectionAccepted);

    // Give the background thread a moment to create the socket
    FPlatformProcess::Sleep(0.1f);

    if (!PendingListener->IsActive())
    {
        UE_LOG(LogArgusServer, Error, TEXT("ArgusDebugServer: Failed to bind TCP listener on port %u"), Port);
        PendingListener.Reset();
        return false;
    }

    Listener = MoveTemp(PendingListener);

    bListening = true;
    UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Listening on 127.0.0.1:%u"), Port);
    return true;
}

auto FDebugTcpServer::StopListening() -> void
{
    bListening = false;

    DisconnectClient();

    if (Listener.IsValid())
    {
        Listener.Reset();
    }

    UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Stopped listening"));
}

auto FDebugTcpServer::IsListening() const -> bool
{
    return bListening;
}

auto FDebugTcpServer::IsClientConnected() const -> bool
{
    return ClientSocket != nullptr;
}

auto FDebugTcpServer::HandleConnectionAccepted(FSocket* InSocket, const FIPv4Endpoint& InEndpoint) -> bool
{
    // Clean up stale worker from a previous disconnected client
    if (Worker.IsValid() && Worker->IsFinished())
    {
        WorkerThread->WaitForCompletion();
        WorkerThread.Reset();
        Worker.Reset();

        if (ClientSocket != nullptr)
        {
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
            ClientSocket = nullptr;
        }

        UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Cleaned up stale client connection"));
    }

    if (IsClientConnected())
    {
        UE_LOG(LogArgusServer, Warning,
            TEXT("ArgusDebugServer: Rejecting connection from %s — another client is already connected"),
            *InEndpoint.ToString());

        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(InSocket);
        return false;
    }

    UE_LOG(LogArgusServer, Log,
        TEXT("ArgusDebugServer: Accepted connection from %s"),
        *InEndpoint.ToString());

    ClientSocket = InSocket;

    // Spawn worker thread for I/O
    Worker = MakeUnique<FClientWorker>(ClientSocket, *this);
    WorkerThread = TUniquePtr<FRunnableThread>(
        FRunnableThread::Create(
            Worker.Get(),
            TEXT("ArgusDebugServerWorker"),
            0,
            TPri_Normal));

    return true;
}

auto FDebugTcpServer::DisconnectClient() -> void
{
    if (Worker.IsValid())
    {
        Worker->Stop();
    }

    if (WorkerThread.IsValid())
    {
        WorkerThread->WaitForCompletion();
        WorkerThread.Reset();
    }

    Worker.Reset();

    if (ClientSocket != nullptr)
    {
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        ClientSocket = nullptr;
    }
}

// ====================================================================================================================
// FClientWorker
// ====================================================================================================================

FDebugTcpServer::FClientWorker::FClientWorker(FSocket* InSocket, FDebugTcpServer& InOwner)
    : ClientSocket(InSocket)
    , Owner(InOwner)
{
}

FDebugTcpServer::FClientWorker::~FClientWorker() = default;

auto FDebugTcpServer::FClientWorker::Init() -> bool
{
    return true;
}

auto FDebugTcpServer::FClientWorker::Run() -> uint32
{
    UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Worker thread started"));

    // --- Read HandshakeRequest frame ---
    auto Payload = ReadFrame();
    if (Payload.Num() == 0)
    {
        UE_LOG(LogArgusServer, Warning, TEXT("ArgusDebugServer: Failed to read handshake frame"));
        return 1;
    }

    if (!HandleHandshake(Payload))
    {
        UE_LOG(LogArgusServer, Warning, TEXT("ArgusDebugServer: Handshake failed"));
        return 1;
    }

    // --- Keep connection open, wait for future control commands ---
    while (!bStopping)
    {
        auto CmdPayload = ReadFrame();
        if (CmdPayload.Num() == 0)
        {
            // Client disconnected or error
            UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Client disconnected"));
            break;
        }

        // Future: dispatch DrawGizmoRequest, etc.
        UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Received command frame (%d bytes payload)"), CmdPayload.Num());
    }

    UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer: Worker thread exiting"));
    return 0;
}

auto FDebugTcpServer::FClientWorker::Stop() -> void
{
    bStopping = true;

    // Close socket to unblock any pending recv
    if (ClientSocket != nullptr)
    {
        ClientSocket->Close();
    }
}

auto FDebugTcpServer::FClientWorker::Exit() -> void
{
    bFinished = true;
}

// --- Socket I/O ---

auto FDebugTcpServer::FClientWorker::RecvExact(uint8* Buffer, int32 NumBytes) -> bool
{
    int32 TotalRead = 0;
    while (TotalRead < NumBytes && !bStopping)
    {
        int32 BytesRead = 0;
        if (!ClientSocket->Recv(Buffer + TotalRead, NumBytes - TotalRead, BytesRead))
        {
            return false;
        }
        if (BytesRead <= 0)
        {
            return false;
        }
        TotalRead += BytesRead;
    }
    return TotalRead == NumBytes;
}

auto FDebugTcpServer::FClientWorker::SendAll(const uint8* Buffer, int32 NumBytes) -> bool
{
    int32 TotalSent = 0;
    while (TotalSent < NumBytes && !bStopping)
    {
        int32 BytesSent = 0;
        if (!ClientSocket->Send(Buffer + TotalSent, NumBytes - TotalSent, BytesSent))
        {
            return false;
        }
        if (BytesSent <= 0)
        {
            return false;
        }
        TotalSent += BytesSent;
    }
    return TotalSent == NumBytes;
}

auto FDebugTcpServer::FClientWorker::ReadFrame() -> TArray<uint8>
{
    // 1. Read 10-byte header
    uint8 HeaderBuf[FRAME_HEADER_SIZE];
    if (!RecvExact(HeaderBuf, FRAME_HEADER_SIZE))
    {
        return {};
    }

    // 2. Parse & validate header
    const auto Header = ParseFrameHeader(HeaderBuf);

    if (Header.Magic != FRAME_MAGIC)
    {
        UE_LOG(LogArgusServer, Error, TEXT("ArgusDebugServer: Invalid frame magic: 0x%08X"), Header.Magic);
        return {};
    }

    if (Header.Version != FRAME_VERSION)
    {
        UE_LOG(LogArgusServer, Error, TEXT("ArgusDebugServer: Invalid frame version: 0x%04X"), Header.Version);
        return {};
    }

    if (Header.PayloadLength == 0)
    {
        return {};
    }

    // Sanity limit: 16 MB
    if (Header.PayloadLength > 16 * 1024 * 1024)
    {
        UE_LOG(LogArgusServer, Error, TEXT("ArgusDebugServer: Frame payload too large: %u bytes"), Header.PayloadLength);
        return {};
    }

    // 3. Read payload
    TArray<uint8> Payload;
    Payload.SetNumUninitialized(Header.PayloadLength);

    if (!RecvExact(Payload.GetData(), Header.PayloadLength))
    {
        return {};
    }

    return Payload;
}

auto FDebugTcpServer::FClientWorker::SendFrame(const TArray<uint8>& Payload) -> bool
{
    const auto Frame = BuildFrame(Payload);
    return SendAll(Frame.GetData(), Frame.Num());
}

auto FDebugTcpServer::FClientWorker::HandleHandshake(const TArray<uint8>& Payload) -> bool
{
    // Deserialize request
    const auto RequestOpt = DeserializeHandshakeRequest(Payload.GetData(), Payload.Num());
    if (!RequestOpt.IsSet())
    {
        UE_LOG(LogArgusServer, Error, TEXT("ArgusDebugServer: Failed to deserialize HandshakeRequest"));
        return false;
    }

    const auto& Request = RequestOpt.GetValue();
    UE_LOG(LogArgusServer, Log,
        TEXT("ArgusDebugServer: HandshakeRequest from '%s' (version 0x%06X, entt 0x%06X)"),
        *Request.ClientName,
        Request.ClientVersion,
        Request.ExpectedEnttVersion);

    // Build response on the game thread
    FHandshakeResponse Response;

    if (Owner.OnBuildHandshakeResponse.IsBound())
    {
        // Block worker thread until game thread produces the response
        FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

        AsyncTask(ENamedThreads::GameThread, [&]()
        {
            Response = Owner.OnBuildHandshakeResponse.Execute(Request);
            DoneEvent->Trigger();
        });

        DoneEvent->Wait();
        FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    }
    else
    {
        // No delegate bound — send a minimal accepted response
        Response.bAccepted     = true;
        Response.ServerVersion = SERVER_VERSION;
        Response.ProjectName   = TEXT("Unknown");
        Response.EngineVersion = TEXT("Unknown");
        Response.ProcessId     = FPlatformProcess::GetCurrentProcessId();
        Response.EnttVersion   = 0;
    }

    UE_LOG(LogArgusServer, Log,
        TEXT("ArgusDebugServer: Sending HandshakeResponse (accepted=%s, registries=%d, types=%d)"),
        Response.bAccepted ? TEXT("true") : TEXT("false"),
        Response.Registries.Num(),
        Response.ComponentTypes.Num());

    // Serialize & send
    const auto PayloadBytes = SerializeHandshakeResponse(Response);
    return SendFrame(PayloadBytes);
}

} // namespace Argus
