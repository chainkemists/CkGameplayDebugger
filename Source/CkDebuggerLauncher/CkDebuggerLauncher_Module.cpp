#include "CkDebuggerLauncher_Module.h"

#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Styles/CkDebuggerLauncherStyle.h"
#include "Window/SCkDebuggerLauncher.h"
#include "Window/SCkDebuggerSuiteWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkDebuggerLauncherModule"

const FName& FCkDebuggerLauncherModule::LauncherTabName = ck::debugger_tabs::LauncherTabId;
const FName& FCkDebuggerLauncherModule::SuiteTabName = ck::debugger_tabs::SuiteTabId;

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

static FAutoConsoleCommand CmdDebuggerSuite(
    TEXT("ck.DebuggerSuite"),
    TEXT("Opens (1) or closes (0) the CK Debugger Suite host window. Usage: ck.DebuggerSuite [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkDebuggerLauncherModule::Get();

        if (InArgs.IsEmpty())
        {
            Module.ToggleSuite();
            return;
        }

        const auto Value = FCString::Atoi(*InArgs[0]);
        if (Value == 1) { Module.OpenSuite(); }
        else if (Value == 0) { Module.CloseSuite(); }
        else { Module.ToggleSuite(); }
    }));

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerLauncherModule::StartupModule() -> void
{
    FCkDebuggerLauncherStyle::Initialize();

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        LauncherTabName,
        FOnSpawnTab::CreateRaw(this, &FCkDebuggerLauncherModule::OnSpawnLauncherTab))
        .SetReuseTabMethod(FOnFindTabToReuse::CreateLambda(
            [this](const FTabId&) { return _LauncherTab; }))
        .SetDisplayName(LOCTEXT("LauncherTabTitle", "CK Debugger Launcher"))
        .SetTooltipText(LOCTEXT("LauncherTabTooltip", "Open and focus the available CK debugger tools"))
        .SetIcon(FSlateIcon{
            FCkIconStyle::GetStyleSetName(),
            FCkIconStyle::Get_StyleKey(ECk_Icon::Diagnostics, ECk_Icon_BrushSize::Size_24x24)});

#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    auto& SuiteSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        SuiteTabName,
        FOnSpawnTab::CreateRaw(this, &FCkDebuggerLauncherModule::OnSpawnSuiteTab))
        .SetReuseTabMethod(FOnFindTabToReuse::CreateLambda(
            [this](const FTabId&) { return _SuiteTab; }))
        .SetDisplayName(LOCTEXT("SuiteTabTitle", "CK Debugger Suite"))
        .SetTooltipText(LOCTEXT("SuiteTabTooltip",
            "One window hosting the CK debugger tools, with the category rail on the left"))
        .SetIcon(FSlateIcon{
            FCkIconStyle::GetStyleSetName(),
            FCkIconStyle::Get_StyleKey(ECk_Icon::Catalog, ECk_Icon_BrushSize::Size_24x24)});

#if WITH_EDITOR
    SuiteSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    // The suite caches factory-built SDockTabs that own feature widget trees (and, through them,
    // ECS handles). Those must be released while the registry is still alive — ShutdownModule is
    // too late. Same contract as CkEcsDebugger_Module.
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(
        this, &FCkDebuggerLauncherModule::HandleEnginePreExit);
}

auto FCkDebuggerLauncherModule::ShutdownModule() -> void
{
    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    CloseSuite();
    CloseLauncher();

    if (FGlobalTabmanager::Get()->HasTabSpawner(SuiteTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SuiteTabName);
    }

    if (FGlobalTabmanager::Get()->HasTabSpawner(LauncherTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LauncherTabName);
    }

    _SuiteWindow.Reset();
    _SuiteTab.Reset();
    _LauncherWindow.Reset();
    _LauncherTab.Reset();
    FCkDebuggerLauncherStyle::Shutdown();
}

auto FCkDebuggerLauncherModule::HandleEnginePreExit() -> void
{
    // Deliberately no RequestCloseTab: by now the editor's window teardown has run and a tab's
    // TSharedFromThis weak-self can already be cleared, so SharedThis would assert. Dropping the
    // refs is all this hook needs to do.
    if (_SuiteWindow.IsValid())
    {
        _SuiteWindow->Release_Presentation();
        _SuiteWindow->Release_AllEmbeddedTools(false);
    }

    if (_LauncherWindow.IsValid())
    { _LauncherWindow->Release_Presentation(); }

    ck::debugger_tabs::Release_DebuggerTab(_SuiteTab, false);
    _SuiteWindow.Reset();
    ck::debugger_tabs::Release_DebuggerTab(_LauncherTab, false);
    _LauncherWindow.Reset();
}

auto FCkDebuggerLauncherModule::Get() -> FCkDebuggerLauncherModule&
{
    return FModuleManager::GetModuleChecked<FCkDebuggerLauncherModule>(TEXT("CkDebuggerLauncher"));
}

auto FCkDebuggerLauncherModule::OpenLauncher() -> void
{
    // A close detaches content before the docking tree processes removal. Never focus that
    // retained empty tab: it has no module-owned presentation to service lifecycle calls.
    if (NOT _LauncherWindow.IsValid())
    {
        if (auto ExistingTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{LauncherTabName});
            ExistingTab.IsValid() && ExistingTab->GetContent() == SNullWidget::NullWidget)
        { ck::debugger_tabs::Release_DebuggerTab(ExistingTab, true); }
    }

    FGlobalTabmanager::Get()->TryInvokeTab(LauncherTabName);
}

auto FCkDebuggerLauncherModule::CloseLauncher() -> void
{
    if (_LauncherWindow.IsValid())
    { _LauncherWindow->Release_Presentation(); }

    ck::debugger_tabs::Release_DebuggerTab(_LauncherTab, NOT IsEngineExitRequested());

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

auto FCkDebuggerLauncherModule::OpenSuite() -> void
{
    // Same detached-tab recovery as the launcher. A live suite presentation is deliberately
    // preserved; only a tab that CloseSuite explicitly emptied is evicted.
    if (NOT _SuiteWindow.IsValid())
    {
        if (auto ExistingTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{SuiteTabName});
            ExistingTab.IsValid() && ExistingTab->GetContent() == SNullWidget::NullWidget)
        { ck::debugger_tabs::Release_DebuggerTab(ExistingTab, true); }
    }

    FGlobalTabmanager::Get()->TryInvokeTab(SuiteTabName);
}

auto FCkDebuggerLauncherModule::CloseSuite() -> void
{
    const bool RunCloseCallbacks = NOT IsEngineExitRequested();

    if (_SuiteWindow.IsValid())
    {
        _SuiteWindow->Release_Presentation();
        _SuiteWindow->Release_AllEmbeddedTools(RunCloseCallbacks);
    }

    // Clear the raw [this] close callback before requesting the synchronous close, and always
    // empty the tab so a docking tree or adversarial held reference cannot retain module widgets.
    ck::debugger_tabs::Release_DebuggerTab(_SuiteTab, RunCloseCallbacks);
    _SuiteWindow.Reset();
}

auto FCkDebuggerLauncherModule::ToggleSuite() -> void
{
    if (IsSuiteOpen()) { CloseSuite(); }
    else { OpenSuite(); }
}

auto FCkDebuggerLauncherModule::IsSuiteOpen() const -> bool
{
    return _SuiteWindow.IsValid() && _SuiteTab.IsValid();
}

auto FCkDebuggerLauncherModule::OnSpawnSuiteTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _SuiteWindow = SNew(SCkDebuggerSuiteWindow);

    _SuiteTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("SuiteTabLabel", "CK Debugger Suite"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab> Tab)
        {
            // Closing the host closes the tools it embedded — run their close callbacks so each
            // owning module resets its own window pointers.
            if (_SuiteWindow.IsValid())
            { _SuiteWindow->Release_AllEmbeddedTools(true); }

            Tab->SetContent(SNullWidget::NullWidget);
            _SuiteWindow.Reset();
            _SuiteTab.Reset();
        })
        [
            _SuiteWindow.ToSharedRef()
        ];

    return _SuiteTab.ToSharedRef();
}

auto FCkDebuggerLauncherModule::OnSpawnLauncherTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _LauncherWindow = SNew(SCkDebuggerLauncher);

    _LauncherTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("LauncherTabLabel", "CK Debugger Launcher"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab> Tab)
        {
            if (_LauncherWindow.IsValid()) { _LauncherWindow->Release_Presentation(); }
            Tab->SetContent(SNullWidget::NullWidget);
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
