#include "CkDebuggerCommon_Module.h"

#include "CkDebuggerCommon/Gallery/SCkDebuggerGallery_Window.h"

#if WITH_EDITOR
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
#if WITH_EDITOR
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
#if WITH_EDITOR
	if (FGlobalTabmanager::Get()->HasTabSpawner(GalleryTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GalleryTabName);
	}

	_GalleryWindow.Reset();
	_GalleryTab.Reset();
#endif
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
		_GalleryTab->RequestCloseTab();
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
