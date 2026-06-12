#include "CkEcsDebugger_Module.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"
#include "CkEcsDebugger/Graph/CkEcsDebugGraphFactory.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "EdGraphUtilities.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkEcsDebuggerModule"

const FName FCkEcsDebuggerModule::DebuggerTabName = FName("CkEcsDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdEcsDebugger(
    TEXT("ck.EcsDebugger"),
    TEXT("Opens (1) or closes (0) the CK ECS Debugger window. Usage: ck.EcsDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkEcsDebuggerModule::Get();

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

auto FCkEcsDebuggerModule::StartupModule() -> void
{
    FCkDebuggerStyle::Initialize();

    _NodeFactory = MakeShared<FCkEcsDebugGraphFactory>();
    FEdGraphUtilities::RegisterVisualNodeFactory(_NodeFactory);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkEcsDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK ECS Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK ECS Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

    // Tear the debugger UI down BEFORE module shutdown / world cleanup, so that
    // any inspector-held FCk_Handle (and any captured handles in Slate delegates)
    // release their TSharedRef into the ECS registry while the registry is still
    // alive. Doing it during ShutdownModule is too late: by then the registry's
    // shared state has been freed and ~FCk_Handle hits an access violation
    // releasing the dangling shared reference.
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkEcsDebuggerModule::HandleEnginePreExit);

    // Install the cross-debugger entity-navigation hook so SCkDebug_EntityRef
    // (and any other widget in CkDebuggerCommon) can route "open this entity in
    // the ECS Debugger" without taking a hard dependency on this module.
    ck::DebugNav::Register_EntityNavigator([this](const FCk_Handle& InEntity)
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return; }

        // OpenDebugger is synchronous: TryInvokeTab spawns the tab via
        // OnSpawnDebuggerTab, which populates DebuggerWindow before returning.
        OpenDebugger();

        if (NOT DebuggerWindow.IsValid())
        { return; }

        const auto Selection = DebuggerWindow->Get_SelectionModel();
        if (NOT Selection.IsValid())
        { return; }

        Selection->Set_SelectedEntities({ InEntity });
    });

    // Cross-debugger selection sync (receive side). Unlike the navigator,
    // this NEVER opens the tab or steals focus — it only mirrors selection
    // into an already-open window. The ECS debugger lists every entity, so
    // the broadcast handle is selected verbatim (no lineage resolution).
    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddLambda(
        [this](const FCk_Handle& InEntity, FName InSource)
        {
            if (InSource == TEXT("EcsDebugger")) { return; }
            if (ck::Is_NOT_Valid(InEntity))      { return; }
            if (NOT DebuggerWindow.IsValid())    { return; }

            const auto Selection = DebuggerWindow->Get_SelectionModel();
            if (NOT Selection.IsValid())         { return; }

            const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
            Selection->Set_SelectedEntities({ InEntity });
        });
}

auto FCkEcsDebuggerModule::HandleEnginePreExit() -> void
{
    // We deliberately do NOT call DebuggerTab->RequestCloseTab() here. By the
    // time OnEnginePreExit fires, the tab's TSharedFromThis weak-self can
    // already be cleared (the editor's window teardown runs before
    // UEngine::PreExit), and RequestCloseTab -> SharedThis(this) would assert.
    //
    // Just drop our refs — that's enough to release the inspector widget tree
    // (and any FCk_Handles it owns) while the ECS registry is still alive,
    // which is the whole point of this hook.
    DebuggerTab.Reset();
    DebuggerWindow.Reset();
}

auto FCkEcsDebuggerModule::ShutdownModule() -> void
{
    // Drop the navigator first — any subsequent click on a stale SCkDebug_EntityRef
    // becomes a no-op instead of dereferencing a half-torn-down module.
    ck::DebugNav::Register_EntityNavigator(ck::DebugNav::FGotoEntityFn{});

    if (_SelectionSyncHandle.IsValid())
    {
        ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle);
        _SelectionSyncHandle.Reset();
    }

    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    if (FGlobalTabmanager::Get()->HasTabSpawner(DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebuggerTabName);
    }

    if (_NodeFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualNodeFactory(_NodeFactory);
        _NodeFactory.Reset();
    }

    // These should already have been released in HandleEnginePreExit during a
    // normal editor shutdown. Reset again here as a safety net (e.g. live module
    // reload, where OnEnginePreExit doesn't fire).
    DebuggerWindow.Reset();
    DebuggerTab.Reset();

    FCkDebuggerStyle::Shutdown();
}

auto FCkEcsDebuggerModule::Get() -> FCkEcsDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkEcsDebuggerModule>("CkEcsDebugger");
}

auto FCkEcsDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(DebuggerTabName);
}

auto FCkEcsDebuggerModule::CloseDebugger() -> void
{
    if (DebuggerTab.IsValid())
    {
        DebuggerTab->RequestCloseTab();
        DebuggerTab.Reset();
    }

    DebuggerWindow.Reset();
}

auto FCkEcsDebuggerModule::ToggleDebugger() -> void
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

auto FCkEcsDebuggerModule::IsDebuggerOpen() const -> bool
{
    return DebuggerWindow.IsValid() && DebuggerTab.IsValid();
}

auto FCkEcsDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    DebuggerWindow = SNew(SCkDebuggerWindow_Main);

    DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK ECS Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            DebuggerWindow.Reset();
            DebuggerTab.Reset();
        })
        [
            DebuggerWindow.ToSharedRef()
        ];

    // Hand the window a weak ref to its tab so the refresh gate can query visibility.
    DebuggerWindow->Set_OwningTab(DebuggerTab);

    return DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEcsDebuggerModule, CkEcsDebugger)
