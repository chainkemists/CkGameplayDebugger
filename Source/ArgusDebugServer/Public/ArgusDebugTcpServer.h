#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "ArgusProtocolTypes.h"

class FSocket;
class FTcpListener;

// --------------------------------------------------------------------------------------------------------------------
// FDebugTcpServer
//
// TCP server that listens for a single Argus debugger client on port 47522.
// Runs I/O on a background FRunnable thread to avoid blocking the game thread.
// Handshake data is gathered on the game thread via async callback.
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

DECLARE_DELEGATE_RetVal_OneParam(
    FHandshakeResponse,
    FOnBuildHandshakeResponse,
    const FHandshakeRequest& /* Request */);

class FDebugTcpServer
{
public:
    FDebugTcpServer();
    ~FDebugTcpServer();

    auto StartListening(uint16 Port = DEFAULT_PORT) -> bool;
    auto StopListening() -> void;
    auto IsListening() const -> bool;
    auto IsClientConnected() const -> bool;

    FOnBuildHandshakeResponse OnBuildHandshakeResponse;

private:
    auto HandleConnectionAccepted(FSocket* InSocket, const FIPv4Endpoint& InEndpoint) -> bool;
    auto DisconnectClient() -> void;
    auto CleanupStaleWorker() -> void;

    // ----- Client worker (background thread) -----

    class FClientWorker : public FRunnable
    {
    public:
        FClientWorker(FSocket* InSocket, FDebugTcpServer& InOwner);
        virtual ~FClientWorker() override;

        auto Init() -> bool override;
        auto Run() -> uint32 override;
        auto Stop() -> void override;
        auto Exit() -> void override;

        auto IsFinished() const -> bool { return Finished; }

    private:
        auto RecvExact(uint8* Buffer, int32 NumBytes) -> bool;
        auto SendAll(const uint8* Buffer, int32 NumBytes) -> bool;
        auto ReadFrame() -> TArray<uint8>;
        auto SendFrame(const TArray<uint8>& Payload) -> bool;
        auto HandleHandshake(const TArray<uint8>& Payload) -> bool;

        FSocket*          ClientSocket = nullptr;
        FDebugTcpServer&  Owner;
        FThreadSafeBool   Stopping;
        FThreadSafeBool   Finished;
    };

    mutable FCriticalSection  ClientMutex;
    TUniquePtr<FTcpListener>  Listener;
    FSocket*                  ClientSocket = nullptr;
    TUniquePtr<FClientWorker> Worker;
    TUniquePtr<FRunnableThread> WorkerThread;
    FThreadSafeBool           Listening;
};

} // namespace Argus