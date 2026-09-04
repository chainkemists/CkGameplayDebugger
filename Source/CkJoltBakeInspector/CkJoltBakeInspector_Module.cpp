#include "CkJoltBakeInspector_Module.h"

#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Misc/CoreDelegates.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

namespace ck_jolt_bake_inspector
{
    const FName TabId{TEXT("CkJoltBakeInspector")};
}

auto FCkJoltBakeInspectorModule::StartupModule() -> void
{
    auto& Spawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        ck_jolt_bake_inspector::TabId, FOnSpawnTab::CreateRaw(this, &FCkJoltBakeInspectorModule::OnSpawnTab))
        .SetDisplayName(FText::FromString(TEXT("CK Jolt Bake Inspector")))
        .SetTooltipText(FText::FromString(TEXT("Inspect Jolt mesh-bake readiness without cooking")));
    Spawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

    _ToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkJoltBakeInspector"), ck_jolt_bake_inspector::TabId,
        FText::FromString(TEXT("[CK] Jolt Bake Inspector")),
        FText::FromString(TEXT("Inspect mesh-shape bake readiness, winding repairs, and predicted failures")),
        ECk_Icon::Entity, ECkDebuggerToolCategory::Systems, 31}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{ck_jolt_bake_inspector::TabId}}); })));

    // The preview target owns components registered with its preview world. Release that UI while
    // engine-owned worlds are still valid; ShutdownModule is too late during process teardown.
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(
        this, &FCkJoltBakeInspectorModule::HandleEnginePreExit);
}

auto FCkJoltBakeInspectorModule::ShutdownModule() -> void
{
    FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
    _EnginePreExitHandle.Reset();
    FCkDebuggerToolRegistry::Get().Unregister(ck_jolt_bake_inspector::TabId, _ToolRegistrationId);
    _ToolRegistrationId = 0;
    ck::debugger_tabs::Release_DebuggerTab(_Tab, true);
    _Window.Reset();
    if (FGlobalTabmanager::Get()->HasTabSpawner(ck_jolt_bake_inspector::TabId))
    { FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ck_jolt_bake_inspector::TabId); }
}

auto FCkJoltBakeInspectorModule::HandleEnginePreExit() -> void
{
    _Tab.Reset();
    _Window.Reset();
}

auto FCkJoltBakeInspectorModule::Get() -> FCkJoltBakeInspectorModule&
{ return FModuleManager::GetModuleChecked<FCkJoltBakeInspectorModule>(TEXT("CkJoltBakeInspector")); }

auto FCkJoltBakeInspectorModule::OpenInspector() -> void
{ ck::debugger_tabs::Invoke_DebuggerTab(ck_jolt_bake_inspector::TabId); }

auto FCkJoltBakeInspectorModule::CloseInspector() -> void
{
    ck::debugger_tabs::Release_DebuggerTab(_Tab, true);
    _Window.Reset();
}

auto FCkJoltBakeInspectorModule::OnSpawnTab(const FSpawnTabArgs&) -> TSharedRef<SDockTab>
{
    _Window = SNew(SCkJoltBakeInspectorWindow);
    _Tab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Jolt Bake Inspector")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>) { _Window.Reset(); _Tab.Reset(); })
        [_Window.ToSharedRef()];
    _Window->Set_OwningTab(_Tab);
    return _Tab.ToSharedRef();
}

IMPLEMENT_MODULE(FCkJoltBakeInspectorModule, CkJoltBakeInspector)
