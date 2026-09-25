#include "CkProceduralAnimationDebugger/CkProceduralAnimationDebugger_Module.h"
#include "CkProceduralAnimationDebugger/Window/SCkProceduralAnimationDebuggerWindow.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkCore/Validation/CkIsValid.h"
#include <Framework/Docking/TabManager.h>
#include <HAL/IConsoleManager.h>
#include <Misc/CoreDelegates.h>
#include <Widgets/Docking/SDockTab.h>
#if WITH_EDITOR
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>
#endif

namespace ck_procedural_debug_module
{
    auto Resolve(const FCk_Handle& InEntity) -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InEntity,
            [](const auto& InCandidate) { return SCkProceduralAnimationDebuggerWindow::Is_ProceduralEntity(InCandidate); });
    }
    static FAutoConsoleCommand Command(TEXT("ck.ProceduralAnimationDebugger"),
        TEXT("Open (1), close (0), or toggle the procedural animation debugger."),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
        {
            auto& Module = FCkProceduralAnimationDebuggerModule::Get();
            if (InArgs.IsEmpty()) { Module.ToggleDebugger(); }
            else if (FCString::Atoi(*InArgs[0]) == 0) { Module.CloseDebugger(); }
            else { Module.OpenDebugger(); }
        }));
}

auto
    FCkProceduralAnimationDebuggerModule::
    StartupModule()
    -> void

{
    auto& Spawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(Get_TabName(),
        FOnSpawnTab::CreateRaw(this, &FCkProceduralAnimationDebuggerModule::SpawnTab))
        .SetReuseTabMethod(FOnFindTabToReuse::CreateLambda([this](const FTabId&) { return _Tab; }))
        .SetDisplayName(FText::FromString(TEXT("CK Procedural Animation")))
        .SetTooltipText(FText::FromString(TEXT("Inspect procedural gait, surface contact, rig poses and sampled history.")));
#if WITH_EDITOR
    Spawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif
    _ToolRegistration = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkProceduralAnimationDebugger"), Get_TabName(), FText::FromString(TEXT("[CK] Procedural Animation")),
        FText::FromString(TEXT("Gait entities, per-leg contact, recorded rig poses and sample history")),
        ECk_Icon::Aim, ECkDebuggerToolCategory::Systems, 60}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return SpawnTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{Get_TabName()}}); })));
    _RouteRegistration = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkProceduralAnimationDebugger"), Get_TabName(),
        [](const auto& InEntity) { return ck::IsValid(ck_procedural_debug_module::Resolve(InEntity)); },
        [](const auto& InEntity) { SCkProceduralAnimationDebuggerWindow::OpenForEntity(ck_procedural_debug_module::Resolve(InEntity)); }});
    _PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkProceduralAnimationDebuggerModule::HandleEnginePreExit);
}

auto
    FCkProceduralAnimationDebuggerModule::
    ShutdownModule()
    -> void

{
    FCoreDelegates::OnEnginePreExit.Remove(_PreExitHandle);
    FCkDebug_EntityTargetRegistry::Get().Unregister(Get_TabName(), _RouteRegistration);
    FCkDebuggerToolRegistry::Get().Unregister(Get_TabName(), _ToolRegistration);
    if (FGlobalTabmanager::Get()->HasTabSpawner(Get_TabName()))
    { FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Get_TabName()); }
    HandleEnginePreExit();
}
auto
    FCkProceduralAnimationDebuggerModule::
    Get()
    -> FCkProceduralAnimationDebuggerModule&

{ return FModuleManager::GetModuleChecked<FCkProceduralAnimationDebuggerModule>(TEXT("CkProceduralAnimationDebugger")); }
auto
    FCkProceduralAnimationDebuggerModule::
    OpenDebugger()
    -> void

{ ck::debugger_tabs::Invoke_DebuggerTab(Get_TabName()); }
auto
    FCkProceduralAnimationDebuggerModule::
    CloseDebugger()
    -> void

{
    if (_Window.IsValid()) { _Window->ReleaseSession(); }
    ck::debugger_tabs::Release_DebuggerTab(_Tab, true);
    _Window.Reset();
}
auto
    FCkProceduralAnimationDebuggerModule::
    ToggleDebugger()
    -> void

{ if (IsDebuggerOpen()) { CloseDebugger(); } else { OpenDebugger(); } }
auto
    FCkProceduralAnimationDebuggerModule::
    IsDebuggerOpen() const
    -> bool

{ return _Window.IsValid() && _Tab.IsValid(); }
auto
    FCkProceduralAnimationDebuggerModule::
    HandleEnginePreExit()
    -> void

{
    if (_Window.IsValid()) { _Window->ReleaseSession(); }
    // The tab's Slate weak-self may already be gone during engine shutdown. Never RequestCloseTab here.
    ck::debugger_tabs::Release_DebuggerTab(_Tab, false);
    _Window.Reset();
}
auto
    FCkProceduralAnimationDebuggerModule::
    SpawnTab(
        const FSpawnTabArgs&)
    -> TSharedRef<SDockTab>

{
    _Window = SNew(SCkProceduralAnimationDebuggerWindow);
    _Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab).Label(FText::FromString(TEXT("CK Procedural Animation")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            if (_Window.IsValid()) { _Window->ReleaseSession(); }
            _Window.Reset();
            _Tab.Reset();
        })[_Window.ToSharedRef()];
    _Window->Set_OwningTab(_Tab);
    return _Tab.ToSharedRef();
}

IMPLEMENT_MODULE(FCkProceduralAnimationDebuggerModule, CkProceduralAnimationDebugger)
