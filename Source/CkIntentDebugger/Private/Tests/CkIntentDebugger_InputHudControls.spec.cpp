#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CkIntentDebugger/Window/SCkIntentDebugger_InputHudControls.h"

// --------------------------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkIntentDebugger_InputHudControlsConstruction,
    "Ck.IntentDebugger.InputHudControls.Construction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkIntentDebugger_InputHudControlsConstruction::RunTest(const FString&) -> bool
{
    const auto Controls = SNew(SCkIntentDebugger_InputHudControls);
    Controls->SlatePrepass();

    TestTrue(TEXT("Intent Debugger composes the operational Input HUD tuner popover"),
        Controls->GetDesiredSize().X > 0.0f && Controls->GetDesiredSize().Y > 0.0f);
    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
