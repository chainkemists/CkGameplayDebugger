#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Misc/AutomationTest.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"

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
