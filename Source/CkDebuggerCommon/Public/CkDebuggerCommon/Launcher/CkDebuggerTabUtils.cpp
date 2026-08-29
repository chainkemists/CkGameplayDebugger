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
    // A tab id that is not in the catalog (the launcher's own tab, the suite host) has no category,
    // which is exactly the answer that keeps it out of every anchoring decision below.
    auto
        Get_ToolCategory(
            const TArray<FCkDebuggerToolDescriptor>& InTools,
            FName InTabId)
        -> ECkDebuggerToolCategory
    {
        for (const auto& Tool : InTools)
        {
            if (Tool.Get_TabId() == InTabId)
            { return Tool.Get_Category(); }
        }

        return ECkDebuggerToolCategory::Invalid;
    }

    // Anchor ONLY next to a live tab of the SAME category. There is deliberately no cross-category
    // fallback: piling every tool into whichever well happened to be open first is what turns one
    // SDockingTabWell into a horizontally scrolling strip of 22 tabs.
    auto
        TryGet_LiveAnchorTabId(
            const TArray<FCkDebuggerToolDescriptor>& InTools,
            FName InTabIdToOpen,
            ECkDebuggerToolCategory InCategory)
        -> FName
    {
        if (InCategory == ECkDebuggerToolCategory::Invalid)
        { return NAME_None; }

        for (const auto& Tool : InTools)
        {
            if (Tool.Get_TabId() == InTabIdToOpen)
            { continue; }

            if (Tool.Get_Category() != InCategory)
            { continue; }

            if (FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{Tool.Get_TabId()}).IsValid())
            { return Tool.Get_TabId(); }
        }

        return NAME_None;
    }

    auto
        TryGet_TabFactory(
            const TArray<FCkDebuggerToolDescriptor>& InTools,
            FName InTabId)
        -> FCkDebuggerToolTabFactory
    {
        for (const auto& Tool : InTools)
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
    const FName SuiteTabId = FName{TEXT("CkDebuggerSuite")};

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

        const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
        const auto Category = ck_debugger_tab_utils::Get_ToolCategory(Tools, InTabId);
        const auto AnchorTabId = ck_debugger_tab_utils::TryGet_LiveAnchorTabId(Tools, InTabId, Category);
        const auto TabFactory = ck_debugger_tab_utils::TryGet_TabFactory(Tools, InTabId);

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
