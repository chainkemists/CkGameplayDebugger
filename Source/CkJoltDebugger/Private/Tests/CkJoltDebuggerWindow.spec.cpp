#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerWindow_ConstructsWithoutSlotAttributeEnsure,
    "Ck.JoltDebugger.Window.ConstructsWithoutSlotAttributeEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerWindow_ConstructsWithoutSlotAttributeEnsure::RunTest(const FString&) -> bool
{
    const auto Window = SNew(SCkJoltDebuggerWindow);
    Window->SlatePrepass();

    TestTrue(TEXT("Jolt debugger window has a non-empty layout"), Window->GetDesiredSize().Y > 0.0f);
    return true;
}

#endif
