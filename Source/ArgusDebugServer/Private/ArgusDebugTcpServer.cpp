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
// Frame helpers
// ====================================================================================================================

static auto BuildFrame(const TArray<uint8>& Payload) -> TArray<uint8>
{
    TArray<uint8> Frame;
    Frame.SetNumUninitialized(FRAME_HEADER_SIZE + Payload.Num());

    Frame[0] = static_cast<uint8>((FRAME_MAGIC >> 24) & 0xFF);
    Frame[1] = static_cast<uint8>((FRAME_MAGIC >> 16) & 0xFF);
    Frame[2] = static_cast<uint8>((FRAME_MAGIC >> 8) & 0xFF);
    Frame[3] = static_cast<uint8>(FRAME_MAGIC & 0xFF);

    Frame[4] = static_cast<uint8>((FRAME_VERSION >> 8) & 0xFF);
    Frame[5] = static_cast<uint8>(FRAME_VERSION & 0xFF);

    const auto Len = static_cast<uint32>(Payload.Num());
    Frame[6] = static_cast<uint8>((Len >> 24) & 0xFF);
    Frame[7] = static_cast<uint8>((Len >> 16) & 0xFF);
    Frame[8] = static_cast<uint8>((Len >> 8) & 0xFF);
    Frame[9] = static_cast<uint8>(Len & 0xFF);

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

    W.WriteArrayHeader(8);
    W.WriteBool(Resp.Accepted);
    W.WriteUInt32(Resp.ServerVersion);
    W.WriteString(Resp.ProjectName);
    W.WriteString(Resp.EngineVersion);
    W.WriteUInt32(Resp.ProcessId);
    W.WriteUInt32(Resp.EnttVersion);

    W.WriteArrayHeader(static_cast<uint32>(Resp.Registries.Num()));
    for (const auto& Reg : Resp.Registries)
    {
        W.WriteArrayHeader(5);
        W.WriteUInt32(Reg.WorldId);
        W.WriteString(Reg.WorldName);
        W.WriteUInt64(Reg.RegistryAddress);
        W.WriteUInt32(Reg.EntityCount);
        W.WriteUInt8(Reg.NetMode);
    }

    W.WriteArrayHeader(static_cast<uint32>(Resp.ComponentTypes.Num()));
    for (const auto& Comp : Resp.ComponentTypes)
    {
        W.WriteArrayHeader(2);
        W.WriteString(Comp.TypeName);

        W.WriteArrayHeader(static_cast<uint32>(Comp.Properties.Num()));
        for (const auto& Prop : Comp.Properties)
        {
            W.WriteArrayHeader(4);
            W.WriteString(Prop.Name);
            W.WriteString(Prop.TypeName);
            W.WriteUInt64(Prop.Offset);
            W.WriteUInt64(Prop.Size);
        }
    }

    return W.Move_Buffer();
}

static auto DeserializeHandshakeRequest(const uint8* Data, int32 Size) -> TOptional<FHandshakeRequest>
{
    FMsgPackReader R(Data, Size);

    const auto ArrayCount = R.ReadArrayHeader();
    if (R.IsError() || ArrayCount < 3)
    {
        return {};
    }

    if (ArrayCount > 3)
    {
        UE_LOG(LogArgusServer, Warning,
            TEXT("HandshakeRequest has %u fields (expected 3) — client may be newer than server"),
            ArrayCount);
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
    if (Listening)
    {
        return true;
    }

    const auto Endpoint = FIPv4Endpoint(FIPv4Address(127, 0, 0, 1), Port);

    constexpr auto Reusable = false;
    auto PendingListener = MakeUnique<FTcpListener>(
        Endpoint,
        FTimespan::FromSeconds(1.0),
        Reusable);

    PendingListener->OnConnectionAccepted().BindRaw(this, &FDebugTcpServer::HandleConnectionAccepted);

    // Poll for listener readiness instead of a fixed sleep
    constexpr int32 MaxAttempts = 20;
    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        if (PendingListener->IsActive())
        {
            break;
        }
        FPlatformProcess::Sleep(0.05f);
    }

    if (NOT PendingListener->IsActive())
    {
        UE_LOG(LogArgusServer, Error, TEXT("Failed to bind TCP listener on port %u"), Port);
        PendingListener.Reset();
        return false;
    }

    Listener = MoveTemp(PendingListener);
    Listening = true;

    UE_LOG(LogArgusServer, Log, TEXT("ArgusDebugServer listening on 127.0.0.1:%u"), Port);
    return true;
}

auto FDebugTcpServer::StopListening() -> void
{
    Listening = false;

    DisconnectClient();

    if (Listener.IsValid())
    {
        Listener.Reset();
    }
}

auto FDebugTcpServer::IsListening() const -> bool
{
    return Listening;
}

auto FDebugTcpServer::IsClientConnected() const -> bool
{
    FScopeLock Lock(&ClientMutex);
    return ClientSocket != nullptr;
}

auto FDebugTcpServer::CleanupStaleWorker() -> void
{
    // Caller must hold ClientMutex
    if (NOT Worker.IsValid() || NOT Worker->IsFinished())
    {
        return;
    }

    WorkerThread->WaitForCompletion();
    WorkerThread.Reset();
    Worker.Reset();

    if (ClientSocket != nullptr)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        ClientSocket = nullptr;
    }

    UE_LOG(LogArgusServer, Verbose, TEXT("Cleaned up stale client connection"));
}

auto FDebugTcpServer::HandleConnectionAccepted(FSocket* InSocket, const FIPv4Endpoint& InEndpoint) -> bool
{
    FScopeLock Lock(&ClientMutex);

    CleanupStaleWorker();

    if (ClientSocket != nullptr)
    {
        UE_LOG(LogArgusServer, Warning,
            TEXT("Rejecting connection from %s — another client is already connected"),
            *InEndpoint.ToString());

        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(InSocket);
        return false;
    }

    UE_LOG(LogArgusServer, Log, TEXT("Accepted connection from %s"), *InEndpoint.ToString());

    ClientSocket = InSocket;

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
    FScopeLock Lock(&ClientMutex);

    if (Worker.IsValid())
    {
        Worker->Stop();
    }

    if (WorkerThread.IsValid())
    {
        // Must release lock before WaitForCompletion to avoid deadlock
        // if worker thread tries to access server state
        ClientMutex.Unlock();
        WorkerThread->WaitForCompletion();
        ClientMutex.Lock();
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
    auto Payload = ReadFrame();
    if (Payload.Num() == 0)
    {
        UE_LOG(LogArgusServer, Warning, TEXT("Failed to read handshake frame"));
        return 1;
    }

    if (NOT HandleHandshake(Payload))
    {
        return 1;
    }

    while (NOT Stopping)
    {
        auto CmdPayload = ReadFrame();
        if (CmdPayload.Num() == 0)
        {
            break;
        }

        // Future: dispatch by message type
        UE_LOG(LogArgusServer, Verbose, TEXT("Received command frame (%d bytes payload)"), CmdPayload.Num());
    }

    return 0;
}

auto FDebugTcpServer::FClientWorker::Stop() -> void
{
    Stopping = true;

    if (ClientSocket != nullptr)
    {
        ClientSocket->Close();
    }
}

auto FDebugTcpServer::FClientWorker::Exit() -> void
{
    Finished = true;
}

auto FDebugTcpServer::FClientWorker::RecvExact(uint8* Buffer, int32 NumBytes) -> bool
{
    int32 TotalRead = 0;
    while (TotalRead < NumBytes && NOT Stopping)
    {
        // Block until data is available or timeout (1s intervals to check Stopping flag)
        if (NOT ClientSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1.0)))
        {
            continue;
        }

        int32 BytesRead = 0;
        if (NOT ClientSocket->Recv(Buffer + TotalRead, NumBytes - TotalRead, BytesRead))
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
    while (TotalSent < NumBytes && NOT Stopping)
    {
        int32 BytesSent = 0;
        if (NOT ClientSocket->Send(Buffer + TotalSent, NumBytes - TotalSent, BytesSent))
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
    uint8 HeaderBuf[FRAME_HEADER_SIZE];
    if (NOT RecvExact(HeaderBuf, FRAME_HEADER_SIZE))
    {
        return {};
    }

    const auto Header = ParseFrameHeader(HeaderBuf);

    if (Header.Magic != FRAME_MAGIC)
    {
        UE_LOG(LogArgusServer, Error, TEXT("Invalid frame magic: 0x%08X"), Header.Magic);
        return {};
    }

    if (Header.Version != FRAME_VERSION)
    {
        UE_LOG(LogArgusServer, Error, TEXT("Invalid frame version: 0x%04X"), Header.Version);
        return {};
    }

    if (Header.PayloadLength == 0)
    {
        return {};
    }

    constexpr uint32 MaxPayloadSize = 16 * 1024 * 1024;
    if (Header.PayloadLength > MaxPayloadSize)
    {
        UE_LOG(LogArgusServer, Error, TEXT("Frame payload too large: %u bytes"), Header.PayloadLength);
        return {};
    }

    TArray<uint8> FramePayload;
    FramePayload.SetNumUninitialized(Header.PayloadLength);

    if (NOT RecvExact(FramePayload.GetData(), Header.PayloadLength))
    {
        return {};
    }

    return FramePayload;
}

auto FDebugTcpServer::FClientWorker::SendFrame(const TArray<uint8>& Payload) -> bool
{
    const auto Frame = BuildFrame(Payload);
    return SendAll(Frame.GetData(), Frame.Num());
}

auto FDebugTcpServer::FClientWorker::HandleHandshake(const TArray<uint8>& Payload) -> bool
{
    const auto RequestOpt = DeserializeHandshakeRequest(Payload.GetData(), Payload.Num());
    if (NOT RequestOpt.IsSet())
    {
        UE_LOG(LogArgusServer, Error, TEXT("Failed to deserialize HandshakeRequest"));
        return false;
    }

    const auto& Request = RequestOpt.GetValue();
    UE_LOG(LogArgusServer, Log,
        TEXT("HandshakeRequest from '%s' (version 0x%06X, entt 0x%06X)"),
        *Request.ClientName,
        Request.ClientVersion,
        Request.ExpectedEnttVersion);

    FHandshakeResponse Response;

    if (Owner.OnBuildHandshakeResponse.IsBound())
    {
        FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

        AsyncTask(ENamedThreads::GameThread, [&]()
        {
            Response = Owner.OnBuildHandshakeResponse.Execute(Request);
            DoneEvent->Trigger();
        });

        constexpr float HandshakeTimeoutSeconds = 5.0f;
        if (NOT DoneEvent->Wait(FTimespan::FromSeconds(HandshakeTimeoutSeconds)))
        {
            FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
            UE_LOG(LogArgusServer, Error, TEXT("Game thread handshake timed out after %.0fs"), HandshakeTimeoutSeconds);

            Response.Accepted     = false;
            Response.ServerVersion = SERVER_VERSION;
            Response.ProjectName   = TEXT("Timeout");
            Response.ProcessId     = FPlatformProcess::GetCurrentProcessId();

            const auto PayloadBytes = SerializeHandshakeResponse(Response);
            SendFrame(PayloadBytes);
            return false;
        }

        FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    }
    else
    {
        Response.Accepted      = true;
        Response.ServerVersion = SERVER_VERSION;
        Response.ProjectName   = TEXT("Unknown");
        Response.EngineVersion = TEXT("Unknown");
        Response.ProcessId     = FPlatformProcess::GetCurrentProcessId();
        Response.EnttVersion   = 0;
    }

    UE_LOG(LogArgusServer, Log,
        TEXT("Sending HandshakeResponse (accepted=%s, registries=%d, types=%d)"),
        Response.Accepted ? TEXT("true") : TEXT("false"),
        Response.Registries.Num(),
        Response.ComponentTypes.Num());

    const auto PayloadBytes = SerializeHandshakeResponse(Response);
    return SendFrame(PayloadBytes);
}

} // namespace Argus