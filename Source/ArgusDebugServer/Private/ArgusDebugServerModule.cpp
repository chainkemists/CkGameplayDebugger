#include "ArgusDebugServerModule.h"
#include "ArgusDebugTcpServer.h"
#include "ArgusHandshakeBuilder.h"

#include "CkEcs/Reflection/CkFragmentReflectionRegistry.h"

#define LOCTEXT_NAMESPACE "FArgusDebugServerModule"

DEFINE_LOG_CATEGORY_STATIC(LogArgusModule, Log, All);

// --------------------------------------------------------------------------------------------------------------------

auto FArgusDebugServerModule::StartupModule() -> void
{
    // Fragment types are self-registered via CK_REGISTER_ECS_FRAGMENT_REFLECTED macros
    // in static initializers. No explicit registration call needed.
    UE_LOG(LogArgusModule, Log, TEXT("ArgusDebugServer: %d fragment types available in reflection registry"),
        ck::FCk_FragmentReflectionRegistry::Get().Num());

    // 1. Create and start TCP server
    TcpServer = MakeUnique<Argus::FDebugTcpServer>();

    // 2. Bind the handshake builder
    TcpServer->OnBuildHandshakeResponse.BindLambda(
        [](const Argus::FHandshakeRequest& Request) -> Argus::FHandshakeResponse
        {
            return Argus::FHandshakeBuilder::Build(Request);
        });

    // 3. Start listening
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
