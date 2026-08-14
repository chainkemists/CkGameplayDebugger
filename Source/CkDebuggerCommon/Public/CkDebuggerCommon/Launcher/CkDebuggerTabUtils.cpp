#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"

#include "CkCore/Macros/CkMacros.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

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
}

// --------------------------------------------------------------------------------------------------------------------
