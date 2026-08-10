#include "CkDialogDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDialogDebugger/Window/SCkDialogDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkDialogDebuggerModule"

const FName FCkDialogDebuggerModule::_DebuggerTabName = FName("CkDialogDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdDialogDebugger(
    TEXT("ck.DialogDebugger"),
    TEXT("Opens (1) or closes (0) the CK Dialog Debugger. Usage: ck.DialogDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkDialogDebuggerModule::Get();

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

auto FCkDialogDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkDialogDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Dialog Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Dialog Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkDialogDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK Dialog Debugger")),
        FText::FromString(TEXT("Inspect dialogue-line registry, emitters, cooldowns, and query history")),
        // Must be the basename of an SVG that actually ships under CkDebugger/Resources/Icons (or Icons/General).
        // An unknown id resolves to nullptr and the launcher silently falls back to the generic warning brush —
        // there is no Dialog.svg, and Speech.svg is the existing icon for this domain.
        TEXT("Speech"),
        // Core/40 — 20 is CkSmDebugger's and 30 is CkMapDebugger's. The launcher census spec asserts every
        // category/order slot is unique, so a duplicate here fails the catalog test rather than just mis-sorting.
        ECkDebuggerToolCategory::Core,
        40});
}

auto FCkDialogDebuggerModule::ShutdownModule() -> void
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

auto FCkDialogDebuggerModule::Get() -> FCkDialogDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkDialogDebuggerModule>("CkDialogDebugger");
}

auto FCkDialogDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto FCkDialogDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload; by then the tab's TSharedFromThis backing is
        // gone and RequestCloseTab would trip the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkDialogDebuggerModule::ToggleDebugger() -> void
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

auto FCkDialogDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkDialogDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkDialogDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Dialog Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    _DebuggerWindow->Set_OwningTab(_DebuggerTab);

    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkDialogDebuggerModule, CkDialogDebugger)
