#include "CkDebuggerCommon_Module.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Gallery/SCkDebuggerGallery_Window.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Settings/CkDebuggerUserSettingsMigration.h"
#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkDebuggerCommonModule"

const FName FCkDebuggerCommonModule::GalleryTabName = FName("CkDebuggerGallery");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdDebuggerGallery(
	TEXT("ck.DebuggerGallery"),
	TEXT("Opens (1) or closes (0) the CK Debugger Widget Gallery. Usage: ck.DebuggerGallery [0/1]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
	{
		auto& Module = FCkDebuggerCommonModule::Get();

		if (InArgs.IsEmpty())
		{
			Module.ToggleGallery();
			return;
		}

		const auto Value = FCString::Atoi(*InArgs[0]);
		if (Value == 1)      { Module.OpenGallery(); }
		else if (Value == 0) { Module.CloseGallery(); }
		else                 { Module.ToggleGallery(); }
	})
);

// --------------------------------------------------------------------------------------------------------------------

void FCkDebuggerCommonModule::StartupModule()
{
	ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(GetMutableDefault<UCkDebuggerSettings>());
	ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(GetMutableDefault<UCkDebuggerStyleSettings>());
	ck::debugger_settings::Migrate_EditorUserSettingsIfNeeded(GetMutableDefault<UCkDebuggerWindowSettings>());

	FCkDebuggerCommonStyle::Initialize();

	// The suite-wide brush/text set (promoted out of CkEcsDebugger). Owned here so every
	// debugger module — not just the ECS one — can rely on it being registered.
	FCkDebuggerStyle::Initialize();

	_WorldBeginTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddRaw(
		this, &FCkDebuggerCommonModule::HandleWorldBeginTearDown);

#if WITH_EDITOR
	_BeginPieHandle = FEditorDelegates::BeginPIE.AddRaw(
		this, &FCkDebuggerCommonModule::HandlePieSessionBoundary);
	_EndPieHandle = FEditorDelegates::EndPIE.AddRaw(
		this, &FCkDebuggerCommonModule::HandlePieSessionBoundary);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GalleryTabName,
		FOnSpawnTab::CreateRaw(this, &FCkDebuggerCommonModule::OnSpawnGalleryTab))
		.SetDisplayName(LOCTEXT("GalleryTabTitle", "CK Debugger — Widget Gallery"))
		.SetTooltipText(LOCTEXT("GalleryTabTooltip", "Showcase every common Ck debugger widget with all its variants and states"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif
}

void FCkDebuggerCommonModule::ShutdownModule()
{
	if (_WorldBeginTearDownHandle.IsValid())
	{
		FWorldDelegates::OnWorldBeginTearDown.Remove(_WorldBeginTearDownHandle);
		_WorldBeginTearDownHandle.Reset();
	}

#if WITH_EDITOR
	if (_BeginPieHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(_BeginPieHandle);
		_BeginPieHandle.Reset();
	}
	if (_EndPieHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(_EndPieHandle);
		_EndPieHandle.Reset();
	}

	if (FGlobalTabmanager::Get()->HasTabSpawner(GalleryTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GalleryTabName);
	}

	_GalleryWindow.Reset();
	_GalleryTab.Reset();
#endif

	FCkDebuggerStyle::Shutdown();
	FCkDebuggerCommonStyle::Shutdown();
}

void FCkDebuggerCommonModule::HandlePieSessionBoundary(bool)
{
	// Release registry-backed debugger state both before a new session starts
	// and while the old session is ending. This makes cleanup idempotent and
	// protects consumers even if they missed a selected-world UI transition.
	ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
}

auto FCkDebuggerCommonModule::HandleWorldBeginTearDown(UWorld* InWorld) -> void
{
	const auto WorldIsValid = ck::IsValid(InWorld);
	CK_ENSURE_IF_NOT(WorldIsValid, TEXT("Debugger session teardown requires a valid world"))
	{ return; }

	if (NOT InWorld->IsGameWorld())
	{ return; }

	// This runtime boundary precedes world-subsystem destruction, so every debugger can
	// synchronously release registry-backed handles before their ECS registry disappears.
	ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(InWorld);
}

auto FCkDebuggerCommonModule::Get() -> FCkDebuggerCommonModule&
{
	return FModuleManager::GetModuleChecked<FCkDebuggerCommonModule>("CkDebuggerCommon");
}

auto FCkDebuggerCommonModule::OpenGallery() -> void
{
#if WITH_EDITOR
	FGlobalTabmanager::Get()->TryInvokeTab(GalleryTabName);
#endif
}

auto FCkDebuggerCommonModule::CloseGallery() -> void
{
#if WITH_EDITOR
	if (_GalleryTab.IsValid())
	{
		// Engine shutdown destroys Slate windows BEFORE module unload — by then
		// the tab's TSharedFromThis backing is gone and RequestCloseTab →
		// SharedThis(this) trips the AsShared check. Just drop the ref on exit.
		if (NOT IsEngineExitRequested())
		{ _GalleryTab->RequestCloseTab(); }
		_GalleryTab.Reset();
	}
	_GalleryWindow.Reset();
#endif
}

auto FCkDebuggerCommonModule::ToggleGallery() -> void
{
	if (IsGalleryOpen()) { CloseGallery(); } else { OpenGallery(); }
}

auto FCkDebuggerCommonModule::IsGalleryOpen() const -> bool
{
	return _GalleryWindow.IsValid() && _GalleryTab.IsValid();
}

#if WITH_EDITOR
auto FCkDebuggerCommonModule::OnSpawnGalleryTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
	_GalleryWindow = SNew(SCkDebuggerGallery_Window);

	_GalleryTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("GalleryTabLabel", "CK Debugger — Widget Gallery"))
		.OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
		{
			_GalleryWindow.Reset();
			_GalleryTab.Reset();
		})
		[
			_GalleryWindow.ToSharedRef()
		];

	// Hand the window a weak ref to its tab so the refresh gate can query visibility.
	_GalleryWindow->Set_OwningTab(_GalleryTab);

	return _GalleryTab.ToSharedRef();
}
#endif

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkDebuggerCommonModule, CkDebuggerCommon)
