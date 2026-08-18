#include "CkSaveDebugger/Window/SCkSaveDebuggerWindow.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Registration_TabSpawnerAndDescriptor,
    "Ck.SaveDebugger.Registration.TabSpawnerAndDescriptor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkSaveDebugger_Registration_TabSpawnerAndDescriptor::RunTest(const FString& Parameters)
{
    const auto TabId = FName{TEXT("CkSaveDebugger")};

    TestEqual(TEXT("The window id and the tab id are the same name"), SCkSaveDebuggerWindow::WindowId, TabId);

    TestTrue(TEXT("The nomad tab spawner is registered"),
        FGlobalTabmanager::Get()->HasTabSpawner(TabId));

    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    const auto* Descriptor = Tools.FindByPredicate([TabId](const FCkDebuggerToolDescriptor& InCandidate)
    {
        return InCandidate.Get_TabId() == TabId;
    });

    TestNotNull(TEXT("The launcher catalog carries a CkSaveDebugger descriptor"), Descriptor);

    if (Descriptor == nullptr)
    { return false; }

    // Pinned fields — the launcher census, saved editor layouts and the user's muscle memory all key off these.
    TestEqual(TEXT("Descriptor is owned by its debugger module"),
        Descriptor->Get_OwnerModule(), FName{TEXT("CkSaveDebugger")});
    TestEqual(TEXT("Descriptor keeps its display name"),
        Descriptor->Get_DisplayName().ToString(), FString{TEXT("[CK] Save Debugger")});
    TestEqual(TEXT("Descriptor keeps its tooltip"),
        Descriptor->Get_Tooltip().ToString(),
        FString{TEXT("Inspect CK .sav snapshot files offline — census, diagnostics, payloads")});
    TestTrue(TEXT("Descriptor keeps its icon"),
        Descriptor->Get_IconId() == ECk_Icon::SaveSlot);
    TestEqual(TEXT("Descriptor stays in the Tools category"),
        Descriptor->Get_Category(), ECkDebuggerToolCategory::Tools);
    TestEqual(TEXT("Descriptor keeps its launcher order"),
        Descriptor->Get_SortOrder(), 30);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
