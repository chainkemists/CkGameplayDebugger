#include "CkOptimizationDebugger/Window/SCkOptimizationDebuggerWindow.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Registration_TabSpawnerAndDescriptor,
    "Ck.OptimizationDebugger.Registration.TabSpawnerAndDescriptor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkOptimizationDebugger_Registration_TabSpawnerAndDescriptor::RunTest(const FString& Parameters)
{
    const auto TabId = FName{TEXT("CkOptimizationDebugger")};

    TestEqual(TEXT("The window id and the tab id are the same name"), SCkOptimizationDebuggerWindow::WindowId, TabId);

    TestTrue(TEXT("The nomad tab spawner is registered"),
        FGlobalTabmanager::Get()->HasTabSpawner(TabId));

    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    const auto* Descriptor = Tools.FindByPredicate([TabId](const FCkDebuggerToolDescriptor& InCandidate)
    {
        return InCandidate.Get_TabId() == TabId;
    });

    TestNotNull(TEXT("The launcher catalog carries a CkOptimizationDebugger descriptor"), Descriptor);

    if (Descriptor == nullptr)
    { return false; }

    // Pinned fields — the launcher census, saved editor layouts and the user's muscle memory all key off these.
    TestEqual(TEXT("Descriptor is owned by its debugger module"),
        Descriptor->Get_OwnerModule(), FName{TEXT("CkOptimizationDebugger")});
    TestEqual(TEXT("Descriptor keeps its display name"),
        Descriptor->Get_DisplayName().ToString(), FString{TEXT("CK Optimization Debugger")});
    TestEqual(TEXT("Descriptor keeps its tooltip"),
        Descriptor->Get_Tooltip().ToString(),
        FString{TEXT("Analyze levels and assets offline — findings, memory, profiling, cleanup")});
    TestEqual(TEXT("Descriptor keeps its icon"),
        Descriptor->Get_IconId(), FName{TEXT("Stopwatch")});
    TestEqual(TEXT("Descriptor stays in the Tools category"),
        Descriptor->Get_Category(), ECkDebuggerToolCategory::Tools);
    TestEqual(TEXT("Descriptor keeps its launcher order"),
        Descriptor->Get_SortOrder(), 40);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
