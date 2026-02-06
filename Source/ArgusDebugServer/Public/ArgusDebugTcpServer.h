#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "ArgusProtocolTypes.h"

class FSocket;
class FTcpListener;

// --------------------------------------------------------------------------------------------------------------------
// FArgusDebugTcpServer
//
// TCP server that listens for a single Argus debugger client on port 47522.
// Runs I/O on a background FRunnable thread to avoid blocking the game thread.
// Handshake data is gathered on the game thread via async callback.
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

/** Delegate that builds the handshake response. Called on the game thread. */
DECLARE_DELEGATE_RetVal_OneParam(
    FHandshakeResponse,
    FOnBuildHandshakeResponse,
    const FHandshakeRequest& /* Request */);

class FDebugTcpServer
{
public:
    FDebugTcpServer();
    ~FDebugTcpServer();

    /** Begin listening for connections. */
    auto StartListening(uint16 Port = DEFAULT_PORT) -> bool;

    /** Stop listening and disconnect any active client. */
    auto StopListening() -> void;

    /** Is the server currently listening? */
    auto IsListening() const -> bool;

    /** Is a debugger client currently connected? */
    auto IsClientConnected() const -> bool;

    /** Bind the delegate that produces a HandshakeResponse. */
    FOnBuildHandshakeResponse OnBuildHandshakeResponse;

private:
    /** Called by FTcpListener on the listener thread when a new connection arrives. */
    auto HandleConnectionAccepted(FSocket* InSocket, const FIPv4Endpoint& InEndpoint) -> bool;

    /** Disconnect the current client. Thread-safe. */
    auto DisconnectClient() -> void;

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

    private:
        /** Read exactly NumBytes into Buffer. Returns false on error/disconnect. */
        auto RecvExact(uint8* Buffer, int32 NumBytes) -> bool;

        /** Send all bytes. Returns false on error. */
        auto SendAll(const uint8* Buffer, int32 NumBytes) -> bool;

        /** Read one protocol frame (header + payload). Returns empty on error. */
        auto ReadFrame() -> TArray<uint8>;

        /** Build and send a response frame from a payload. */
        auto SendFrame(const TArray<uint8>& Payload) -> bool;

        /** Process the handshake after receiving a request frame. */
        auto HandleHandshake(const TArray<uint8>& Payload) -> bool;

        FSocket*          ClientSocket = nullptr;
        FDebugTcpServer&  Owner;
        FThreadSafeBool   bStopping;
        FThreadSafeBool   bFinished;
    };

    TUniquePtr<FTcpListener>     Listener;
    FSocket*                     ClientSocket   = nullptr;
    TUniquePtr<FClientWorker>    Worker;
    TUniquePtr<FRunnableThread>  WorkerThread;
    FThreadSafeBool              bListening;
};

} // namespace Argus
