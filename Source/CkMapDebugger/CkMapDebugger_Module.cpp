#include "CkMapDebugger_Module.h"

#include "CkMapDebugger/Window/SCkMapDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkMapDebuggerModule"

const FName FCkMapDebuggerModule::_DebuggerTabName = FName("CkMapDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdMapDebugger(
    TEXT("ck.MapDebugger"),
    TEXT("Opens (1) or closes (0) the CK Map Debugger. Usage: ck.MapDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkMapDebuggerModule::Get();

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

auto FCkMapDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkMapDebuggerModule::OnSpawnDebuggerTab))
        .SetReuseTabMethod(FOnFindTabToReuse::CreateLambda(
            [this](const FTabId&) { return _DebuggerTab; }))
        .SetDisplayName(FText::FromString(TEXT("CK Map Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Map Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkMapDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("[CK] Map Debugger")),
        FText::FromString(TEXT("POIs, compasses, minimaps, and fog-of-war — the CkPoi map stack at a glance")),
        ECk_Icon::Minimap,
        ECkDebuggerToolCategory::Core,
        30}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkMapDebuggerModule::HandleEnginePreExit);
}

auto FCkMapDebuggerModule::ShutdownModule() -> void
{
    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    if (_DebuggerWindow.IsValid()) { _DebuggerWindow->Release_Presentation(); }
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, false);
    _DebuggerWindow.Reset();
}

auto FCkMapDebuggerModule::Get() -> FCkMapDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkMapDebuggerModule>("CkMapDebugger");
}

auto FCkMapDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkMapDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerWindow.IsValid()) { _DebuggerWindow->Release_Presentation(); }
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, true);
    _DebuggerWindow.Reset();
}

auto FCkMapDebuggerModule::ToggleDebugger() -> void
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

auto FCkMapDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkMapDebuggerModule::HandleEnginePreExit() -> void
{
    // Slate may already have destroyed the tab's shared backing. Drop content and local refs only.
    if (_DebuggerWindow.IsValid()) { _DebuggerWindow->Release_Presentation(); }
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, false);
    _DebuggerWindow.Reset();
}

auto FCkMapDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkMapDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Map")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            if (_DebuggerWindow.IsValid()) { _DebuggerWindow->Release_Presentation(); }
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

IMPLEMENT_MODULE(FCkMapDebuggerModule, CkMapDebugger)
