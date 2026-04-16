#include "CkGoapDebugger_Module.h"

#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraphFactory.h"

#include "EdGraphUtilities.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

// ====================================================================================================================

#define LOCTEXT_NAMESPACE "FCkGoapDebuggerModule"

const FName FCkGoapDebuggerModule::_TabId{TEXT("CkGoapDebugger")};

// ====================================================================================================================

static FAutoConsoleCommand CmdGoapDebugger(
	TEXT("ck.GoapDebugger"),
	TEXT("Opens (1) or closes (0) the CK GOAP Debugger. Usage: ck.GoapDebugger [0/1]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
	{
		auto& Module = FCkGoapDebuggerModule::Get();

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

// ====================================================================================================================

void FCkGoapDebuggerModule::StartupModule()
{
	_NodeFactory = MakeShared<FCkGoapDebugGraphFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(_NodeFactory);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		_TabId,
		FOnSpawnTab::CreateRaw(this, &FCkGoapDebuggerModule::OnSpawnDebuggerTab))
		.SetDisplayName(LOCTEXT("TabTitle", "CK GOAP Debugger"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Opens the CK GOAP planner debugger window"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
}

void FCkGoapDebuggerModule::ShutdownModule()
{
	if (FGlobalTabmanager::Get()->HasTabSpawner(_TabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_TabId);
	}

	if (_NodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(_NodeFactory);
		_NodeFactory.Reset();
	}

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
		_Tab.Reset();
	}
	_Window.Reset();
}

auto
	FCkGoapDebuggerModule::
	ToggleDebugger()
	-> void
{
	if (IsDebuggerOpen()) { CloseDebugger(); }
	else { OpenDebugger(); }
}

auto
	FCkGoapDebuggerModule::
	IsDebuggerOpen() const
	-> bool
{
	return _Window.IsValid() && _Tab.IsValid();
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
		.Label(FText::FromString(TEXT("CK GOAP Debugger")))
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
