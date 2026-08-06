#include "CkCrowdDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FCkCrowdDebuggerModule"

// --------------------------------------------------------------------------------------------------------------------

const FName FCkCrowdDebuggerModule::_TabId{TEXT("CkCrowdDebugger")};

// --------------------------------------------------------------------------------------------------------------------

void FCkCrowdDebuggerModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		_TabId,
		FOnSpawnTab::CreateRaw(this, &FCkCrowdDebuggerModule::OnSpawnDebuggerTab))
		.SetDisplayName(LOCTEXT("TabTitle", "CK Crowd Debugger"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Opens the CK Crowd / Navigation debugger window"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

	_DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
		TEXT("CkCrowdDebugger"),
		_TabId,
		LOCTEXT("LauncherDisplayName", "CK Crowd Debugger"),
		LOCTEXT("LauncherTooltip", "Inspect crowd agents, navigation, paths, and avoidance"),
		TEXT("People"),
		ECkDebuggerToolCategory::Ai,
		30});

	// The embedded editor viewport owns editor modes. Release it before Slate and the editor-mode subsystem shut down.
	_EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(
		this,
		&FCkCrowdDebuggerModule::HandleEnginePreExit);
}

// --------------------------------------------------------------------------------------------------------------------

void FCkCrowdDebuggerModule::ShutdownModule()
{
	FCkDebuggerToolRegistry::Get().Unregister(_TabId, _DebuggerToolRegistrationId);
	_DebuggerToolRegistrationId = 0;

	if (_EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
		_EnginePreExitHandle.Reset();
	}

	if (FGlobalTabmanager::Get()->HasTabSpawner(_TabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_TabId);
	}

	_Window.Reset();
	_Tab.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkCrowdDebuggerModule::HandleEnginePreExit() -> void
{
	_Tab.Reset();
	_Window.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkCrowdDebuggerModule::OpenDebugger() -> void
{
	FGlobalTabmanager::Get()->TryInvokeTab(_TabId);
}

auto FCkCrowdDebuggerModule::CloseDebugger() -> void
{
	if (_Tab.IsValid())
	{
		// Engine shutdown destroys Slate windows BEFORE module unload — by then
		// the tab's TSharedFromThis backing is gone and RequestCloseTab →
		// SharedThis(this) trips the AsShared check. Just drop the ref on exit.
		if (NOT IsEngineExitRequested())
		{ _Tab->RequestCloseTab(); }
	}
}

auto FCkCrowdDebuggerModule::ToggleDebugger() -> void
{
	if (IsDebuggerOpen()) { CloseDebugger(); }
	else { OpenDebugger(); }
}

auto FCkCrowdDebuggerModule::IsDebuggerOpen() const -> bool
{
	return _Tab.IsValid() && _Window.IsValid();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkCrowdDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
	_Window = SNew(SCkCrowdDebuggerWindow);

	_Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(FText::FromString(TEXT("CK Crowd Debugger")))
		.OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
		{
			_Window.Reset();
			_Tab.Reset();
		})
		[
			_Window.ToSharedRef()
		];

	_Window->Set_OwningTab(_Tab);

	return _Tab.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdCrowdDebugger(
	TEXT("ck.CrowdDebugger"),
	TEXT("Opens (1) or closes (0) the CK Crowd Debugger. Usage: ck.CrowdDebugger [0/1]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
	{
		auto& Module = FCkCrowdDebuggerModule::Get();

		if (InArgs.IsEmpty())
		{
			Module.ToggleDebugger();
			return;
		}

		const auto Value = FCString::Atoi(*InArgs[0]);
		if (Value == 1) { Module.OpenDebugger(); }
		else if (Value == 0) { Module.CloseDebugger(); }
		else { Module.ToggleDebugger(); }
	})
);

// --------------------------------------------------------------------------------------------------------------------

static TAutoConsoleVariable<int32> CVarCrowdDebuggerPathNetworkTrace(
	TEXT("ck.CrowdDebugger.PathNetworkTrace"),
	0,
	TEXT("Logs same-frame Path Network counts at the Crowd Debugger collector, view model, and active viewport draw stages. 0=off, 1=on."),
	ECVF_Default);

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkCrowdDebuggerModule, CkCrowdDebugger)
