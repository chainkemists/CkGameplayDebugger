#include "CkNavDebugger_Module.h"

#include "CkNavDebugger/Data/CkNavDebugger_HealthCheck.h"
#include "CkNavDebugger/Data/CkNavDebugger_Types.h"
#include "CkNavDebugger/DebugDraw/CkNavDebugger_WorldDraw.h"
#include "CkNavDebugger/Window/SCkNavDebuggerWindow.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkNavDebuggerModule"

const FName FCkNavDebuggerModule::_DebuggerTabName = FName("CkNavDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdNavDebugger(
    TEXT("ck.NavDebugger"),
    TEXT("Opens (1) or closes (0) the CK Navigation Debugger. Usage: ck.NavDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkNavDebuggerModule::Get();

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

static FAutoConsoleCommand CmdNavDebuggerHealthCheck(
    TEXT("ck.NavDebugger.HealthCheck"),
    TEXT("Run the CK Navigation Debugger system-level diagnostic battery and print the report to the log."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        auto FoundWorld = static_cast<UWorld*>(nullptr);
        if (GEngine != nullptr)
        {
            for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
            {
                if (It->WorldType == EWorldType::PIE && IsValid(It->World()))
                { FoundWorld = It->World(); break; }
            }
        }

        const auto Report = FCkNavDebugger_HealthCheck::Run(FoundWorld);
        UE_LOG(LogTemp, Display, TEXT("[CkNavDebugger.HealthCheck] %d items"), Report.Items.Num());
        for (const auto& Item : Report.Items)
        {
            const TCHAR* Tag = Item.Status == ECkNavDebugger_HealthStatus::Pass ? TEXT("OK  ")
                             : Item.Status == ECkNavDebugger_HealthStatus::Warn ? TEXT("WARN")
                             :                                                    TEXT("FAIL");
            UE_LOG(LogTemp, Display, TEXT("  [%s] %s — %s"), Tag, *Item.Name, *Item.Detail);
        }
    })
);

auto
    FCkNavDebuggerModule::
    StartupModule()
    -> void
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkNavDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Navigation Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Navigation Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

    // In-world overlay runs whenever the module is loaded, regardless of whether the panel
    // window is open — the overlay is the killer feature for nav debugging.
    _WorldTickHandle = FWorldDelegates::OnWorldTickStart.AddLambda(
        [](UWorld* InWorld, ELevelTick, float)
        {
            FCkNavDebugger_WorldDraw::DrawAll(InWorld);
        });
}

auto
    FCkNavDebuggerModule::
    ShutdownModule()
    -> void
{
    if (_WorldTickHandle.IsValid())
    {
        FWorldDelegates::OnWorldTickStart.Remove(_WorldTickHandle);
        _WorldTickHandle.Reset();
    }

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto
    FCkNavDebuggerModule::
    Get()
    -> FCkNavDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkNavDebuggerModule>("CkNavDebugger");
}

auto
    FCkNavDebuggerModule::
    OpenDebugger()
    -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto
    FCkNavDebuggerModule::
    CloseDebugger()
    -> void
{
    if (_DebuggerTab.IsValid())
    {
        _DebuggerTab->RequestCloseTab();
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
    FCkNavDebugger_WorldDraw::Set_WindowOpen(false);
}

auto
    FCkNavDebuggerModule::
    ToggleDebugger()
    -> void
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

auto
    FCkNavDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto
    FCkNavDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs& InArgs)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkNavDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Navigation Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
            FCkNavDebugger_WorldDraw::Set_WindowOpen(false);
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    _DebuggerWindow->Set_OwningTab(_DebuggerTab);
    FCkNavDebugger_WorldDraw::Set_WindowOpen(true);

    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkNavDebuggerModule, CkNavDebugger)
