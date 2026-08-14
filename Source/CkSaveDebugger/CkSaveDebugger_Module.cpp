#include "CkSaveDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkSaveDebugger/Window/SCkSaveDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkSaveDebuggerModule"

const FName FCkSaveDebuggerModule::_DebuggerTabName = FName("CkSaveDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdSaveDebugger(
    TEXT("ck.SaveDebugger"),
    TEXT("Opens (1) or closes (0) the CK Save Debugger. Usage: ck.SaveDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkSaveDebuggerModule::Get();

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

auto FCkSaveDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkSaveDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Save Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Save Debugger window")));

#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkSaveDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK Save Debugger")),
        FText::FromString(TEXT("Inspect CK .sav snapshot files offline — census, diagnostics, payloads")),
        // Must be the basename of an SVG that ships under CkDebugger/Resources/Icons (or Icons/General) — an unknown
        // id resolves to nullptr and the launcher silently falls back to the generic warning brush. Cassette.svg exists.
        TEXT("Cassette"),
        // Tools/30 — the Tools category already uses 10 (Insights Analyzer) and 20 (Style Lab). The launcher census
        // spec asserts every category/order slot is unique, so this must stay distinct.
        ECkDebuggerToolCategory::Tools,
        30}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));
}

auto FCkSaveDebuggerModule::ShutdownModule() -> void
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

auto FCkSaveDebuggerModule::Get() -> FCkSaveDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkSaveDebuggerModule>("CkSaveDebugger");
}

auto FCkSaveDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkSaveDebuggerModule::CloseDebugger() -> void
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

auto FCkSaveDebuggerModule::ToggleDebugger() -> void
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

auto FCkSaveDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkSaveDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkSaveDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Save")))
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

IMPLEMENT_MODULE(FCkSaveDebuggerModule, CkSaveDebugger)

#undef LOCTEXT_NAMESPACE
