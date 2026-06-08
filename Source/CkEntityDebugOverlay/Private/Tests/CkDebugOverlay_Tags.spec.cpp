#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Test-only tags — natively registered so RequestGameplayTag resolves them without erroring.
// UE_DEFINE_GAMEPLAY_TAG_STATIC registers at DLL load (before the test runner fires).

UE_DEFINE_GAMEPLAY_TAG_STATIC(TestTag_GoalLeaf, "Ck.OnScreenDebugger.Provider.GOAP.Goal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TestTag_Goap,     "Ck.OnScreenDebugger.Provider.GOAP");

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_LeafName_Test,
    "Ck.DebugOverlay.Tags.LeafName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_LeafName_Test::RunTest(const FString&)
{
    TestEqual(TEXT("leaf"),      ck_debugoverlay::Get_LeafName(TestTag_GoalLeaf), FString(TEXT("Goal")));
    TestEqual(TEXT("root-leaf"), ck_debugoverlay::Get_LeafName(TestTag_Goap),     FString(TEXT("GOAP")));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
