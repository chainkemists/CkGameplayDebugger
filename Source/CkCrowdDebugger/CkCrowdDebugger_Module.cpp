#include "CkCrowdDebugger_Module.h"

#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "Framework/Docking/TabManager.h"
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
}

// --------------------------------------------------------------------------------------------------------------------

void FCkCrowdDebuggerModule::ShutdownModule()
{
	if (FGlobalTabmanager::Get()->HasTabSpawner(_TabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_TabId);
	}

	_Window.Reset();
	_Tab.Reset();
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
		_Tab->RequestCloseTab();
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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkCrowdDebuggerModule, CkCrowdDebugger)
