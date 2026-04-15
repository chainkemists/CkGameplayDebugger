#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// ====================================================================================================================

class SCkGoapDebuggerWindow;

// ====================================================================================================================

class FCkGoapDebuggerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static auto Get() -> FCkGoapDebuggerModule& { return FModuleManager::GetModuleChecked<FCkGoapDebuggerModule>("CkGoapDebugger"); }

	auto OpenDebugger() -> void;
	auto CloseDebugger() -> void;
	auto ToggleDebugger() -> void;
	auto IsDebuggerOpen() const -> bool;

private:
	auto OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

private:
	TSharedPtr<SCkGoapDebuggerWindow> _Window;
	TSharedPtr<SDockTab> _Tab;

	static const FName _TabId;
};

// ====================================================================================================================
