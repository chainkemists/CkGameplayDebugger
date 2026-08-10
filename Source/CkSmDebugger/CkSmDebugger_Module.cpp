#include "CkSmDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Utils.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"

#include "CkSmDebugger/Window/SCkSmDebuggerWindow.h"
#include "CkSmDebugger/Graph/CkSmDebugGraphFactory.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "EdGraphUtilities.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkSmDebuggerModule"

const FName FCkSmDebuggerModule::_DebuggerTabName = FName("CkSmDebugger");

namespace
{
    auto ResolveSmTarget(const FCk_Handle& InSelected) -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InSelected,
            [](const FCk_Handle& InCandidate)
            {
                return ck::IsValid(InCandidate)
                    && InCandidate.Has_All<ck::FFragment_Sm_Current, ck::FFragment_Sm_Params>();
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdSmDebugger(
    TEXT("ck.SmDebugger"),
    TEXT("Opens (1) or closes (0) the CK State Machine Debugger. Usage: ck.SmDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkSmDebuggerModule::Get();

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

auto FCkSmDebuggerModule::StartupModule() -> void
{
    _NodeFactory = MakeShared<FCkSmDebugGraphFactory>();
    FEdGraphUtilities::RegisterVisualNodeFactory(_NodeFactory);

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkSmDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK State Machine Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK State Machine Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _TabForegroundedHandle = FGlobalTabmanager::Get()->OnTabForegrounded_Subscribe(
        FOnActiveTabChanged::FDelegate::CreateRaw(
            this, &FCkSmDebuggerModule::OnTabForegrounded));

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkSmDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK State Machine Debugger")),
        FText::FromString(TEXT("Inspect state-machine graphs, transitions, and history")),
        TEXT("StateMachine"),
        ECkDebuggerToolCategory::Core,
        20});

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkSmDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity) { return ck::IsValid(ResolveSmTarget(InEntity)); },
        [this](const FCk_Handle& InEntity)
        {
            const auto Target = ResolveSmTarget(InEntity);
            if (NOT ck::IsValid(Target)) { return; }
            OpenDebugger();
            if (_DebuggerWindow.IsValid()) { _DebuggerWindow->TargetEntity(Target); }
        }});

    // Selection sync only adopts into a live window. Unlike the entity-target
    // route above, it must not create a tab or take foreground focus.
    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddLambda(
        [this](const FCk_Handle& InEntity, FName InSource)
        {
            if (InSource == TEXT("SmDebugger")) { return; }
            if (ck::Is_NOT_Valid(InEntity))       { return; }
            if (NOT _DebuggerWindow.IsValid())    { return; }

            const auto Target = ResolveSmTarget(InEntity);
            if (NOT ck::IsValid(Target)) { return; }

            const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
            _DebuggerWindow->TargetEntity(Target);
        });
}

auto FCkSmDebuggerModule::ShutdownModule() -> void
{
    if (_SelectionSyncHandle.IsValid())
    {
        ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle);
        _SelectionSyncHandle.Reset();
    }

    UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(false);

    FCkDebug_EntityTargetRegistry::Get().Unregister(_DebuggerTabName, _EntityTargetRouteRegistrationId);
    _EntityTargetRouteRegistrationId = 0;

    if (_TabForegroundedHandle.IsValid())
    {
        FGlobalTabmanager::Get()->OnTabForegrounded_Unsubscribe(_TabForegroundedHandle);
        _TabForegroundedHandle.Reset();
    }

    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    if (_NodeFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualNodeFactory(_NodeFactory);
        _NodeFactory.Reset();
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkSmDebuggerModule::Get() -> FCkSmDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkSmDebuggerModule>("CkSmDebugger");
}

auto FCkSmDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto FCkSmDebuggerModule::CloseDebugger() -> void
{
    UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(false);

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

auto FCkSmDebuggerModule::ToggleDebugger() -> void
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

auto FCkSmDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkSmDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkSmDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK SM Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(false);
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

auto FCkSmDebuggerModule::OnTabForegrounded(
    TSharedPtr<SDockTab> InNewForegroundTab,
    TSharedPtr<SDockTab>) -> void
{
    UCk_Utils_StateMachineDebug_UE::Set_IsDebuggerCaptureVisible(
        _DebuggerTab.IsValid()
        && InNewForegroundTab == _DebuggerTab
        && _DebuggerTab->IsForeground());
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkSmDebuggerModule, CkSmDebugger)
