#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkNavDebuggerWindow;
class SDockTab;

class FCkNavDebuggerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static auto Get() -> FCkNavDebuggerModule&;

	auto OpenDebugger() -> void;
	auto CloseDebugger() -> void;
	auto ToggleDebugger() -> void;
	auto IsDebuggerOpen() const -> bool;

private:
	auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

	TSharedPtr<SCkNavDebuggerWindow> _DebuggerWindow;
	TSharedPtr<SDockTab> _DebuggerTab;

	static const FName _DebuggerTabName;
};
