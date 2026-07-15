#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkDebuggerWindow_Main;
class SDockTab;
struct FGraphPanelNodeFactory;

class FCkEcsDebuggerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static auto Get() -> FCkEcsDebuggerModule&;

	auto OpenDebugger() -> void;
	auto CloseDebugger() -> void;
	auto ToggleDebugger() -> void;
	auto IsDebuggerOpen() const -> bool;

private:
	auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

	auto HandleEnginePreExit() -> void;

	TSharedPtr<SCkDebuggerWindow_Main> DebuggerWindow;
	TSharedPtr<SDockTab> DebuggerTab;
	TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;

	FDelegateHandle _EnginePreExitHandle;
	FDelegateHandle _SelectionSyncHandle;
	uint64 _DebuggerToolRegistrationId = 0;

	static const FName DebuggerTabName;
};
