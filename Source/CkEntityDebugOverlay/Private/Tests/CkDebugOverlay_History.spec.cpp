#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_History_Test,
    "Ck.DebugOverlay.History.RingAndChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebugOverlay_History_Test::RunTest(const FString&)
{
    FCk_DebugOverlay_History H;
    const FCk_DebugOverlay_HistoryKey K{ 1, FGameplayTag::RequestGameplayTag(TEXT("Ck.OnScreenDebugger.Provider")) };
    H.Observe(K, TEXT("Idle"),   0.0);
    H.Observe(K, TEXT("Idle"),   0.1);   // no change → no append, no new stamp
    H.Observe(K, TEXT("Turn"),   0.2);
    H.Observe(K, TEXT("Moving"), 0.3);
    const auto Trail = H.Get_Trail(K, 2);            // last two PRIOR values, oldest→newest
    TestEqual(TEXT("trail len"), Trail.Num(), 2);
    TestEqual(TEXT("trail[0]"), Trail[0], FString(TEXT("Idle")));
    TestEqual(TEXT("trail[1]"), Trail[1], FString(TEXT("Turn")));
    TestEqual(TEXT("changed at"), H.Get_LastChangedTime(K), 0.3);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
