#include "CkInputDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkInputDebugger/Window/SCkInputDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkInputDebuggerModule"

const FName FCkInputDebuggerModule::_DebuggerTabName = FName("CkInputDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdInputDebugger(
    TEXT("ck.InputDebugger"),
    TEXT("Opens (1) or closes (0) the CK Enhanced Input Debugger. Usage: ck.InputDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkInputDebuggerModule::Get();

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

auto FCkInputDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkInputDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Enhanced Input Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Enhanced Input Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkInputDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("[CK] Enhanced Input Debugger")),
        FText::FromString(TEXT("Inspect input contexts, actions, bindings, and live values")),
        ECk_Icon::Input,
        ECkDebuggerToolCategory::Interface,
        20}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(
        this, &FCkInputDebuggerModule::HandleEnginePreExit);
}

auto FCkInputDebuggerModule::ShutdownModule() -> void
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

    CloseDebugger();
}

auto FCkInputDebuggerModule::HandleEnginePreExit() -> void
{
    _DebuggerWindow.Reset();
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, false);
}

auto FCkInputDebuggerModule::Get() -> FCkInputDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkInputDebuggerModule>("CkInputDebugger");
}

auto FCkInputDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkInputDebuggerModule::CloseDebugger() -> void
{
    _DebuggerWindow.Reset();
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, true);
}

auto FCkInputDebuggerModule::ToggleDebugger() -> void
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

auto FCkInputDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkInputDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkInputDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Enhanced Input")))
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

IMPLEMENT_MODULE(FCkInputDebuggerModule, CkInputDebugger)
