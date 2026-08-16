#include "CkOptimizationDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkOptimizationDebugger/Window/SCkOptimizationDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkOptimizationDebuggerModule"

const FName FCkOptimizationDebuggerModule::_DebuggerTabName = FName("CkOptimizationDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdOptimizationDebugger(
    TEXT("ck.OptimizationDebugger"),
    TEXT("Opens (1) or closes (0) the CK Optimization Debugger. Usage: ck.OptimizationDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkOptimizationDebuggerModule::Get();

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

auto FCkOptimizationDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkOptimizationDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Optimization Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Optimization Debugger window")));

#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkOptimizationDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("[CK] Optimization Debugger")),
        FText::FromString(TEXT("Analyze levels and assets offline — findings, memory, profiling, cleanup")),
        // Must be the basename of an SVG that ships under CkDebugger/Resources/Icons (or Icons/General) — an unknown
        // id resolves to nullptr and the launcher silently falls back to the generic warning brush. Stopwatch.svg exists.
        TEXT("Stopwatch"),
        // Tools/40 — the Tools category already uses 10 (Insights Analyzer), 20 (Style Lab) and 30 (Save). The launcher
        // census spec asserts every category/order slot is unique, so this must stay distinct.
        ECkDebuggerToolCategory::Tools,
        40});
}

auto FCkOptimizationDebuggerModule::ShutdownModule() -> void
{
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    CloseDebugger();

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkOptimizationDebuggerModule::Get() -> FCkOptimizationDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkOptimizationDebuggerModule>("CkOptimizationDebugger");
}

auto FCkOptimizationDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto FCkOptimizationDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then the tab's TSharedFromThis backing is
        // gone and RequestCloseTab → SharedThis(this) trips the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkOptimizationDebuggerModule::ToggleDebugger() -> void
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

auto FCkOptimizationDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkOptimizationDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkOptimizationDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Optimization")))
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

IMPLEMENT_MODULE(FCkOptimizationDebuggerModule, CkOptimizationDebugger)

#undef LOCTEXT_NAMESPACE
