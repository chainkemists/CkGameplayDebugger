#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"

#include "CkCore/Macros/CkMacros.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_tab_utils
{
    auto
        TryGet_LiveAnchorTabId(
            FName InTabIdToOpen)
        -> FName
    {
        for (const auto& Tool : FCkDebuggerToolRegistry::Get().Get_Tools())
        {
            if (Tool.Get_TabId() == InTabIdToOpen)
            { continue; }

            if (FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{Tool.Get_TabId()}).IsValid())
            { return Tool.Get_TabId(); }
        }

        return NAME_None;
    }

    auto
        TryGet_TabFactory(
            FName InTabId)
        -> FCkDebuggerToolTabFactory
    {
        for (const auto& Tool : FCkDebuggerToolRegistry::Get().Get_Tools())
        {
            if (Tool.Get_TabId() == InTabId)
            { return Tool.Get_TabFactory(); }
        }

        return {};
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debugger_tabs
{
    const FName LauncherTabId = FName{TEXT("CkDebuggerLauncher")};

    auto
        Invoke_DebuggerTab(
            FName InTabId)
        -> TSharedPtr<SDockTab>
    {
        const auto GlobalTabManager = FGlobalTabmanager::Get();

        // FTabId equality wildcard-matches the instance id, so this finds the nomad-spawned AND
        // the dock-inserted flavor alike — the id can never open twice.
        if (const auto ExistingTab = GlobalTabManager->FindExistingLiveTab(FTabId{InTabId});
            ExistingTab.IsValid())
        {
            ExistingTab->DrawAttention();
            return ExistingTab;
        }

        const auto DockIntoExisting = GetDefault<UCkDebuggerWindowSettings>()->DockNewDebuggersIntoExistingWindow;

        if (NOT DockIntoExisting)
        { return GlobalTabManager->TryInvokeTab(FTabId{InTabId}); }

        const auto AnchorTabId = ck_debugger_tab_utils::TryGet_LiveAnchorTabId(InTabId);
        const auto TabFactory = ck_debugger_tab_utils::TryGet_TabFactory(InTabId);

        if (AnchorTabId.IsNone() || NOT TabFactory.IsBound())
        { return GlobalTabManager->TryInvokeTab(FTabId{InTabId}); }

        const auto NewTab = TabFactory.Execute();
        GlobalTabManager->InsertNewDocumentTab(
            AnchorTabId, InTabId, FTabManager::FLiveTabSearch{AnchorTabId}, NewTab);

        return NewTab;
    }

    auto
        Release_DebuggerTab(
            TSharedPtr<SDockTab>& InOutTab,
            bool InRequestClose)
        -> void
    {
        auto Tab = MoveTemp(InOutTab);
        if (NOT Tab.IsValid())
        { return; }

        // The close delegate is commonly bound with CreateRaw/[this]. Clear it before either a synchronous close
        // or module unload can invoke code whose owner is disappearing.
        Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});

        if (InRequestClose && NOT IsEngineExitRequested())
        { Tab->RequestCloseTab(); }

        // A global docking tree may retain the tab after the module drops its reference. Do not let it retain a
        // module-defined widget tree (or ECS handles owned by that tree) across unload.
        Tab->SetContent(SNullWidget::NullWidget);
    }
}

// --------------------------------------------------------------------------------------------------------------------
