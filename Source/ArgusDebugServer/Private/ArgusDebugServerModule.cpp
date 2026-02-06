#include "ArgusDebugServerModule.h"
#include "ArgusDebugTcpServer.h"
#include "ArgusHandshakeBuilder.h"
#include "ArgusFragmentTypeMap.h"

#define LOCTEXT_NAMESPACE "FArgusDebugServerModule"

DEFINE_LOG_CATEGORY_STATIC(LogArgusModule, Log, All);

// Forward declaration — defined in ArgusFragmentRegistration.cpp
namespace Argus { auto RegisterAllFragments() -> void; }

// --------------------------------------------------------------------------------------------------------------------

auto FArgusDebugServerModule::StartupModule() -> void
{
    // 1. Register fragment types for UE reflection
    Argus::RegisterAllFragments();

    // 2. Create and start TCP server
    TcpServer = MakeUnique<Argus::FDebugTcpServer>();

    // 3. Bind the handshake builder
    TcpServer->OnBuildHandshakeResponse.BindLambda(
        [](const Argus::FHandshakeRequest& Request) -> Argus::FHandshakeResponse
        {
            return Argus::FHandshakeBuilder::Build(Request);
        });

    // 4. Start listening
    if (TcpServer->StartListening())
    {
        UE_LOG(LogArgusModule, Log, TEXT("ArgusDebugServer module started"));
    }
    else
    {
        UE_LOG(LogArgusModule, Error, TEXT("ArgusDebugServer module failed to start TCP server"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FArgusDebugServerModule::ShutdownModule() -> void
{
    if (TcpServer.IsValid())
    {
        TcpServer->StopListening();
        TcpServer.Reset();
    }

    UE_LOG(LogArgusModule, Log, TEXT("ArgusDebugServer module shut down"));
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FArgusDebugServerModule, ArgusDebugServer)
