#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerTabUtils_LauncherTabIdIsStable,
    "Ck.DebuggerCommon.TabUtils.LauncherTabIdIsStable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerTabUtils_LauncherTabIdIsStable::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Common owns the launcher tab identity"),
        ck::debugger_tabs::LauncherTabId, FName{TEXT("CkDebuggerLauncher")});
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerTabUtils_ReleaseDetachesModuleContent,
    "Ck.DebuggerCommon.TabUtils.ReleaseDetachesModuleContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerTabUtils_ReleaseDetachesModuleContent::RunTest(const FString& Parameters)
{
    TSharedPtr<SWidget> Content = SNew(SBox);
    auto WeakContent = TWeakPtr<SWidget>{Content};
    TSharedPtr<SDockTab> Tab = SNew(SDockTab)[Content.ToSharedRef()];
    Content.Reset();

    ck::debugger_tabs::Release_DebuggerTab(Tab, false);

    TestFalse(TEXT("release clears the module-owned tab reference"), Tab.IsValid());
    TestFalse(TEXT("release detaches module-defined content from the tab"), WeakContent.IsValid());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerTabUtils_TerminalReleaseRemovesParentedTab,
    "Ck.DebuggerCommon.TabUtils.TerminalReleaseRemovesParentedTab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerTabUtils_TerminalReleaseRemovesParentedTab::RunTest(const FString& Parameters)
{
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Terminal tab release test requires Slate."));
        return false;
    }

    const FName TestTabId{TEXT("CkDebuggerCommon.TabUtils.TerminalRelease")};
    const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();
    if (NOT TestFalse(TEXT("Terminal-release fixture tab spawner is not already registered"), TabManager->HasTabSpawner(TestTabId)))
    { return false; }

    int32 ClosedCallbackCount = 0;
    TSharedPtr<SDockTab> HeldTab;
    const TSharedPtr<SWidget> Content = SNew(SBox);
    TabManager->RegisterNomadTabSpawner(TestTabId, FOnSpawnTab::CreateLambda(
        [&ClosedCallbackCount, &HeldTab, Content](const FSpawnTabArgs&)
        {
            HeldTab = SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                .OnTabClosed_Lambda([&ClosedCallbackCount](TSharedRef<SDockTab>) { ++ClosedCallbackCount; })
                [Content.ToSharedRef()];
            return HeldTab.ToSharedRef();
        }));
    ON_SCOPE_EXIT
    {
        if (HeldTab.IsValid() && HeldTab->GetParent().IsValid())
        {
            HeldTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});
            HeldTab->RemoveTabFromParent();
        }
        TabManager->UnregisterNomadTabSpawner(TestTabId);
    };

    TSharedPtr<SDockTab> Tab = TabManager->TryInvokeTab(TestTabId);
    if (NOT TestTrue(TEXT("Fixture opens a live tab"), Tab.IsValid() && HeldTab.IsValid())) { return false; }
    if (NOT TestTrue(TEXT("Fixture tab is parented before terminal release"), HeldTab->GetParent().IsValid())) { return false; }

    ck::debugger_tabs::Release_DebuggerTab(Tab, true);

    TestFalse(TEXT("Terminal release clears the caller-owned tab reference"), Tab.IsValid());
    TestEqual(TEXT("Terminal release suppresses the tab-closed callback"), ClosedCallbackCount, 0);
    TestFalse(TEXT("Terminal release synchronously removes the held tab from its parent"), HeldTab->GetParent().IsValid());
    TestTrue(TEXT("Terminal release replaces held tab content with the null widget"), HeldTab->GetContent() == SNullWidget::NullWidget);
    TestFalse(TEXT("Terminal release synchronously removes the live tab manager entry"), TabManager->FindExistingLiveTab(FTabId{TestTabId}).IsValid());

    ck::debugger_tabs::Release_DebuggerTab(Tab, true);

    TestFalse(TEXT("Repeated terminal release keeps the caller tab reference clear"), Tab.IsValid());
    TestEqual(TEXT("Repeated terminal release remains callback-free"), ClosedCallbackCount, 0);
    TestFalse(TEXT("Repeated terminal release keeps the held tab detached"), HeldTab->GetParent().IsValid());
    TestTrue(TEXT("Repeated terminal release keeps the held tab content null"), HeldTab->GetContent() == SNullWidget::NullWidget);
    TestFalse(TEXT("Repeated terminal release does not restore a tab-manager entry"), TabManager->FindExistingLiveTab(FTabId{TestTabId}).IsValid());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
