#include "CkJoltDebugger_Module.h"

#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkJoltDebuggerModule"

auto FCkJoltDebuggerModule::Get_DebuggerTabName() -> FName
{
    return SCkJoltDebuggerWindow::TabId;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_module
{
    auto Resolve_JoltTarget(
        const FCk_Handle& InSelected) -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InSelected,
            [](const FCk_Handle& InCandidate)
            { return SCkJoltDebuggerWindow::Is_JoltDebuggerEntity(InCandidate); });
    }
}

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdJoltDebugger(
    TEXT("ck.JoltDebugger"),
    TEXT("Opens (1) or closes (0) the CK Jolt Physics Debugger. Usage: ck.JoltDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkJoltDebuggerModule::Get();

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

auto FCkJoltDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        Get_DebuggerTabName(),
        FOnSpawnTab::CreateRaw(this, &FCkJoltDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Jolt Physics Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Jolt Physics Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkJoltDebugger"),
        Get_DebuggerTabName(),
        FText::FromString(TEXT("CK Jolt Physics Debugger")),
        FText::FromString(TEXT("World-level Jolt physics stats: bodies, characters, and the baked static world")),
        TEXT("Cube"),
        ECkDebuggerToolCategory::Systems,
        30}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{Get_DebuggerTabName()}}); })));

    // Tear the debugger UI down BEFORE module shutdown / world cleanup, so the window's debug-draw target
    // unregisters from a live Jolt subsystem and releases its preview-world components while the world is
    // still alive. Doing it during ShutdownModule is too late.
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkJoltDebuggerModule::HandleEnginePreExit);

    // Registered AFTER the tab spawner so the route can never resolve to a tab that cannot be opened.
    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkJoltDebugger"), Get_DebuggerTabName(),
        [](const FCk_Handle& InEntity)
        { return ck::IsValid(ck_jolt_debugger_module::Resolve_JoltTarget(InEntity)); },
        [](const FCk_Handle& InEntity)
        {
            const auto Target = ck_jolt_debugger_module::Resolve_JoltTarget(InEntity);

            if (ck::Is_NOT_Valid(Target))
            { return; }

            auto& Module = FCkJoltDebuggerModule::Get();
            Module.OpenDebugger();

            if (Module._DebuggerWindow.IsValid())
            { Module._DebuggerWindow->TargetEntity(Target); }
        }});
}

auto FCkJoltDebuggerModule::HandleEnginePreExit() -> void
{
    _DebuggerTab.Reset();
    _DebuggerWindow.Reset();
}

auto FCkJoltDebuggerModule::ShutdownModule() -> void
{
    // Unregistered BEFORE the spawner: a route outliving its tab spawner offers an "Open In" that opens
    // nothing.
    FCkDebug_EntityTargetRegistry::Get().Unregister(Get_DebuggerTabName(), _EntityTargetRouteRegistrationId);
    _EntityTargetRouteRegistrationId = 0;

    FCkDebuggerToolRegistry::Get().Unregister(Get_DebuggerTabName(), _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    if (FGlobalTabmanager::Get()->HasTabSpawner(Get_DebuggerTabName()))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Get_DebuggerTabName());
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkJoltDebuggerModule::Get() -> FCkJoltDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkJoltDebuggerModule>("CkJoltDebugger");
}

auto FCkJoltDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(Get_DebuggerTabName());
}

auto FCkJoltDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        _DebuggerTab->RequestCloseTab();
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkJoltDebuggerModule::ToggleDebugger() -> void
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

auto FCkJoltDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkJoltDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkJoltDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Jolt Physics")))
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

IMPLEMENT_MODULE(FCkJoltDebuggerModule, CkJoltDebugger)
