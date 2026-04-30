#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebuggerWindow;
class SDockTab;
struct FSpawnTabArgs;

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
	auto OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

private:
	TSharedPtr<SCkCrowdDebuggerWindow> _Window;
	TSharedPtr<SDockTab> _Tab;

	static const FName _TabId;
};

// --------------------------------------------------------------------------------------------------------------------
