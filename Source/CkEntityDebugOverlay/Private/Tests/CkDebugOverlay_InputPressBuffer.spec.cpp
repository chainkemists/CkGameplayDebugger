#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/Input/CkDebugOverlay_InputProcessor.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_InputPressBuffer_Test,
    "Ck.DebugOverlay.Input.PressBuffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_InputPressBuffer_Test::RunTest(const FString&)
{
    FCkDebugOverlay_InputPressBuffer Buffer;

    TestFalse(TEXT("invalid key is ignored"), Buffer.Record_KeyDown(FKey{}, false, 1.0));
    TestFalse(TEXT("repeat is ignored"), Buffer.Record_KeyDown(EKeys::LeftShift, true, 2.0));

    TestTrue(TEXT("first Shift press is recorded"), Buffer.Record_KeyDown(EKeys::LeftShift, false, 3.0));
    TestTrue(TEXT("other-key press is recorded"), Buffer.Record_KeyDown(EKeys::V, false, 4.0));
    TestTrue(TEXT("second Shift press is recorded"), Buffer.Record_KeyDown(EKeys::LeftShift, false, 5.0));

    const auto ShiftPresses = Buffer.Consume_PressTimes(EKeys::LeftShift);
    if (NOT TestEqual(TEXT("same-key presses survive one poll"), ShiftPresses.Num(), 2))
    { return false; }
    TestEqual(TEXT("same-key press order is preserved (first)"), ShiftPresses[0], 3.0);
    TestEqual(TEXT("same-key press order is preserved (second)"), ShiftPresses[1], 5.0);

    const auto VPresses = Buffer.Consume_PressTimes(EKeys::V);
    if (NOT TestEqual(TEXT("per-key consumption preserves other keys"), VPresses.Num(), 1))
    { return false; }
    TestEqual(TEXT("other-key timestamp is preserved"), VPresses[0], 4.0);
    TestEqual(TEXT("invalid-key consumption is empty"), Buffer.Consume_PressTimes(FKey{}).Num(), 0);

    TestTrue(TEXT("press before clear is recorded"), Buffer.Record_KeyDown(EKeys::LeftControl, false, 6.0));
    Buffer.Clear();
    TestEqual(TEXT("clear drops unconsumed presses"), Buffer.Consume_PressTimes(EKeys::LeftControl).Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
