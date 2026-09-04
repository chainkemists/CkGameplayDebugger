#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkJoltBakeInspectorWindow;
class SDockTab;

class FCkJoltBakeInspectorModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkJoltBakeInspectorModule&;
    auto OpenInspector() -> void;
    auto CloseInspector() -> void;

private:
    auto OnSpawnTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkJoltBakeInspectorWindow> _Window;
    TSharedPtr<SDockTab> _Tab;
    FDelegateHandle _EnginePreExitHandle;
    uint64 _ToolRegistrationId = 0;
};
