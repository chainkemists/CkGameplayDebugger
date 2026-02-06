#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace Argus { class FDebugTcpServer; }

class FArgusDebugServerModule : public IModuleInterface
{
public:
    virtual auto StartupModule() -> void override;
    virtual auto ShutdownModule() -> void override;

private:
    TUniquePtr<Argus::FDebugTcpServer> TcpServer;
};
