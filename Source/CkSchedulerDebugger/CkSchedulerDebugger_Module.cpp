#include "CkSchedulerDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkSchedulerDebugger/Window/SCkSchedulerDebuggerWindow.h"
#if WITH_EDITOR
    #include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphFactory.h"
#endif

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "EdGraphUtilities.h"
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkSchedulerDebuggerModule"

const FName FCkSchedulerDebuggerModule::_DebuggerTabName = FName("CkSchedulerDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdSchedulerDebugger(
    TEXT("ck.SchedulerDebugger"),
    TEXT("Opens (1) or closes (0) the CK Scheduler Debugger. Usage: ck.SchedulerDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkSchedulerDebuggerModule::Get();

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

auto FCkSchedulerDebuggerModule::StartupModule() -> void
{
#if WITH_EDITOR
    _NodeFactory = MakeShared<FCkSchedulerDebugGraphFactory>();
    FEdGraphUtilities::RegisterVisualNodeFactory(_NodeFactory);
#endif

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkSchedulerDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Scheduler Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Scheduler Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkSchedulerDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK Scheduler Debugger")),
        FText::FromString(TEXT("Inspect processor scheduling, phases, and frame execution")),
        TEXT("Stopwatch"),
        ECkDebuggerToolCategory::Systems,
        10}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));
}

auto FCkSchedulerDebuggerModule::ShutdownModule() -> void
{
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

#if WITH_EDITOR
    if (_NodeFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualNodeFactory(_NodeFactory);
        _NodeFactory.Reset();
    }
#endif

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkSchedulerDebuggerModule::Get() -> FCkSchedulerDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkSchedulerDebuggerModule>("CkSchedulerDebugger");
}

auto FCkSchedulerDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkSchedulerDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then
        // the tab's TSharedFromThis backing is gone and RequestCloseTab →
        // SharedThis(this) trips the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkSchedulerDebuggerModule::ToggleDebugger() -> void
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

auto FCkSchedulerDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkSchedulerDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkSchedulerDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Scheduler Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    // Hand the window a weak ref to its tab so the refresh gate can query visibility.
    _DebuggerWindow->Set_OwningTab(_DebuggerTab);

    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkSchedulerDebuggerModule, CkSchedulerDebugger)
