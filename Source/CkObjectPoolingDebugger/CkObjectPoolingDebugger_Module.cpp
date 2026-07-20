#include "CkObjectPoolingDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkObjectPoolingDebugger/Window/SCkObjectPoolingDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkObjectPoolingDebuggerModule"

const FName FCkObjectPoolingDebuggerModule::_DebuggerTabName = FName("CkObjectPoolingDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdObjectPoolingDebugger(
    TEXT("ck.ObjectPoolingDebugger"),
    TEXT("Opens (1) or closes (0) the CK Object Pooling Debugger. Usage: ck.ObjectPoolingDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkObjectPoolingDebuggerModule::Get();

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

auto FCkObjectPoolingDebuggerModule::StartupModule() -> void
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkObjectPoolingDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Object Pooling Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Object Pooling Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkObjectPoolingDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK Object Pooling Debugger")),
        FText::FromString(TEXT("Inspect object pools, occupancy, allocations, and tuning data")),
        TEXT("Package"),
        ECkDebuggerToolCategory::Systems,
        20});
}

auto FCkObjectPoolingDebuggerModule::ShutdownModule() -> void
{
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkObjectPoolingDebuggerModule::Get() -> FCkObjectPoolingDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkObjectPoolingDebuggerModule>("CkObjectPoolingDebugger");
}

auto FCkObjectPoolingDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto FCkObjectPoolingDebuggerModule::CloseDebugger() -> void
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

auto FCkObjectPoolingDebuggerModule::ToggleDebugger() -> void
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

auto FCkObjectPoolingDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkObjectPoolingDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkObjectPoolingDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Object Pooling")))
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

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkObjectPoolingDebuggerModule, CkObjectPoolingDebugger)

#undef LOCTEXT_NAMESPACE
