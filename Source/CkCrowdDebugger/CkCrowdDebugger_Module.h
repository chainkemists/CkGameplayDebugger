#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebuggerWindow;
class SDockTab;
class FSpawnTabArgs;
struct FCkCrowdDebuggerLifecycleTestAccess;

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebuggerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static auto Get() -> FCkCrowdDebuggerModule&
	{
		return FModuleManager::GetModuleChecked<FCkCrowdDebuggerModule>("CkCrowdDebugger");
	}

	auto OpenDebugger() -> void;
	auto CloseDebugger() -> void;
	auto ToggleDebugger() -> void;
	auto IsDebuggerOpen() const -> bool;

private:
	friend struct FCkCrowdDebuggerLifecycleTestAccess;
	auto OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
	auto HandleEnginePreExit() -> void;

private:
	TSharedPtr<SCkCrowdDebuggerWindow> _Window;
	TSharedPtr<SDockTab> _Tab;
	FDelegateHandle _EnginePreExitHandle;
	uint64 _DebuggerToolRegistrationId = 0;
	uint64 _EntityTargetRouteRegistrationId = 0;

	static const FName _TabId;
};

// --------------------------------------------------------------------------------------------------------------------
