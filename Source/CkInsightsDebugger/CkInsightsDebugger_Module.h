#pragma once

#include "CoreMinimal.h"
#include "CkInsightsDebugger/Capture/CkInsightsCaptureController.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SCkInsightsAnalyzerTab;
class SDockTab;
struct FCkInsightsLifecycleTestAccess;

// --------------------------------------------------------------------------------------------------------------------

class FCkInsightsDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkInsightsDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;
    auto Get_CaptureController() -> FCkInsightsCaptureController& { return _CaptureController; }

private:
    friend struct FCkInsightsLifecycleTestAccess;
    auto OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkInsightsAnalyzerTab> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;
    FCkInsightsCaptureController _CaptureController;
    uint64 _DebuggerToolRegistrationId = 0;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};

// --------------------------------------------------------------------------------------------------------------------
