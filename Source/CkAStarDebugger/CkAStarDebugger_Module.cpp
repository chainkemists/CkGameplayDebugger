#include "CkAStarDebugger_Module.h"

// TODO: Implement — see CkAStarDebugger_Plan.md

#define LOCTEXT_NAMESPACE "FCkAStarDebuggerModule"

const FName FCkAStarDebuggerModule::_DebuggerTabName = FName("CkAStarDebugger");

auto FCkAStarDebuggerModule::StartupModule() -> void
{
	// TODO: Register NomadTabSpawner under Developer Tools > Debug
	// TODO: Console command: ck.AStarDebugger [0/1]
}

auto FCkAStarDebuggerModule::ShutdownModule() -> void
{
	// TODO: Unregister tab spawner, reset window/tab
}

auto FCkAStarDebuggerModule::Get() -> FCkAStarDebuggerModule&
{
	return FModuleManager::GetModuleChecked<FCkAStarDebuggerModule>("CkAStarDebugger");
}

auto FCkAStarDebuggerModule::OpenDebugger() -> void {}
auto FCkAStarDebuggerModule::CloseDebugger() -> void {}
auto FCkAStarDebuggerModule::ToggleDebugger() -> void {}
auto FCkAStarDebuggerModule::IsDebuggerOpen() const -> bool { return false; }

auto FCkAStarDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
	// TODO: Create SCkAStarDebuggerWindow, wrap in SDockTab
	return SNew(SDockTab);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkAStarDebuggerModule, CkAStarDebugger)
