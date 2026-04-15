#include "CkGoapDebugger_Module.h"

#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

// ====================================================================================================================

#define LOCTEXT_NAMESPACE "FCkGoapDebuggerModule"

const FName FCkGoapDebuggerModule::_TabId{TEXT("CkGoapDebugger")};

// ====================================================================================================================

void FCkGoapDebuggerModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		_TabId,
		FOnSpawnTab::CreateRaw(this, &FCkGoapDebuggerModule::OnSpawnDebuggerTab))
		.SetDisplayName(LOCTEXT("TabTitle", "GOAP Debugger"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Open the GOAP planner debugger"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

	// Console command
	static auto CVar = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ck.GoapDebugger"),
		TEXT("Toggle the GOAP debugger window. Usage: ck.GoapDebugger [0/1]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& InArgs)
		{
			if (InArgs.Num() > 0)
			{
				if (InArgs[0] == TEXT("0")) { CloseDebugger(); }
				else { OpenDebugger(); }
			}
			else
			{
				ToggleDebugger();
			}
		}),
		ECVF_Default);
}

void FCkGoapDebuggerModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_TabId);
	_Window.Reset();
	_Tab.Reset();
}

// ====================================================================================================================

auto
	FCkGoapDebuggerModule::
	OpenDebugger()
	-> void
{
	FGlobalTabmanager::Get()->TryInvokeTab(_TabId);
}

auto
	FCkGoapDebuggerModule::
	CloseDebugger()
	-> void
{
	if (_Tab.IsValid())
	{
		_Tab->RequestCloseTab();
	}
}

auto
	FCkGoapDebuggerModule::
	ToggleDebugger()
	-> void
{
	if (IsDebuggerOpen())
	{
		CloseDebugger();
	}
	else
	{
		OpenDebugger();
	}
}

auto
	FCkGoapDebuggerModule::
	IsDebuggerOpen() const
	-> bool
{
	return _Tab.IsValid() && _Tab->IsForeground();
}

// ====================================================================================================================

auto
	FCkGoapDebuggerModule::
	OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs)
	-> TSharedRef<SDockTab>
{
	_Window = SNew(SCkGoapDebuggerWindow);

	_Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
		{
			_Window.Reset();
			_Tab.Reset();
		})
		[
			_Window.ToSharedRef()
		];

	return _Tab.ToSharedRef();
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkGoapDebuggerModule, CkGoapDebugger)
