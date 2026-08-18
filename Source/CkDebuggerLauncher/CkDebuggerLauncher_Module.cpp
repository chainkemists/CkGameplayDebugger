#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkDebuggerLauncher_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "Styles/CkDebuggerLauncherStyle.h"
#include "Window/SCkDebuggerLauncher.h"

#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkDebuggerLauncherModule"

const FName FCkDebuggerLauncherModule::LauncherTabName = FName{TEXT("CkDebuggerLauncher")};

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdDebuggerLauncher(
    TEXT("ck.DebuggerLauncher"),
    TEXT("Opens (1) or closes (0) the CK Debugger Launcher. Usage: ck.DebuggerLauncher [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkDebuggerLauncherModule::Get();

        if (InArgs.IsEmpty())
        {
            Module.ToggleLauncher();
            return;
        }

        const auto Value = FCString::Atoi(*InArgs[0]);
        if (Value == 1) { Module.OpenLauncher(); }
        else if (Value == 0) { Module.CloseLauncher(); }
        else { Module.ToggleLauncher(); }
    }));

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerLauncherModule::StartupModule() -> void
{
    FCkDebuggerLauncherStyle::Initialize();

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        LauncherTabName,
        FOnSpawnTab::CreateRaw(this, &FCkDebuggerLauncherModule::OnSpawnLauncherTab))
        .SetDisplayName(LOCTEXT("LauncherTabTitle", "CK Debugger Launcher"))
        .SetTooltipText(LOCTEXT("LauncherTabTooltip", "Open and focus the available CK debugger tools"))
        .SetIcon(FSlateIcon{
            FCkIconStyle::GetStyleSetName(),
            FCkIconStyle::Get_StyleKey(ECk_Icon::Diagnostics, ECk_Icon_BrushSize::Size_24x24)});

#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

}

auto FCkDebuggerLauncherModule::ShutdownModule() -> void
{
    CloseLauncher();

    if (FGlobalTabmanager::Get()->HasTabSpawner(LauncherTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LauncherTabName);
    }

    _LauncherWindow.Reset();
    _LauncherTab.Reset();
    FCkDebuggerLauncherStyle::Shutdown();
}

auto FCkDebuggerLauncherModule::Get() -> FCkDebuggerLauncherModule&
{
    return FModuleManager::GetModuleChecked<FCkDebuggerLauncherModule>(TEXT("CkDebuggerLauncher"));
}

auto FCkDebuggerLauncherModule::OpenLauncher() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(LauncherTabName);
}

auto FCkDebuggerLauncherModule::CloseLauncher() -> void
{
    if (_LauncherTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then
        // the tab's TSharedFromThis backing is gone and RequestCloseTab →
        // SharedThis(this) trips the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _LauncherTab->RequestCloseTab(); }
        _LauncherTab.Reset();
    }

    _LauncherWindow.Reset();
}

auto FCkDebuggerLauncherModule::ToggleLauncher() -> void
{
    if (IsLauncherOpen()) { CloseLauncher(); }
    else { OpenLauncher(); }
}

auto FCkDebuggerLauncherModule::IsLauncherOpen() const -> bool
{
    return _LauncherWindow.IsValid() && _LauncherTab.IsValid();
}

auto FCkDebuggerLauncherModule::OnSpawnLauncherTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _LauncherWindow = SNew(SCkDebuggerLauncher);

    _LauncherTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("LauncherTabLabel", "CK Debugger Launcher"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _LauncherWindow.Reset();
            _LauncherTab.Reset();
        })
        [
            _LauncherWindow.ToSharedRef()
        ];

    return _LauncherTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkDebuggerLauncherModule, CkDebuggerLauncher)
