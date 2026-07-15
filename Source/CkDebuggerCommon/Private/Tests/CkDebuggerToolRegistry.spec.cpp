#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerToolRegistry_RegisterReplaceSortAndUnregister,
    "Ck.DebuggerLauncher.Registry.RegisterReplaceSortAndUnregister",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_tool_registry_tests
{
auto MakeDescriptor(FName InTabId, int32 InSortOrder, const TCHAR* InDisplayName) -> FCkDebuggerToolDescriptor
{
    return FCkDebuggerToolDescriptor{
        TEXT("CkDebuggerToolRegistryTests"),
        InTabId,
        FText::FromString(InDisplayName),
        FText::FromString(TEXT("Test debugger tool")),
        TEXT("Bug"),
        ECkDebuggerToolCategory::Core,
        InSortOrder};
}
} // namespace ck_debugger_tool_registry_tests

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerToolRegistry_RegisterReplaceSortAndUnregister::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_tool_registry_tests;

    auto Registry = FCkDebuggerToolRegistry{};
    auto ChangeCount = 0;
    const auto ChangedHandle = Registry.Get_OnChanged().AddLambda([&ChangeCount]()
    {
        ++ChangeCount;
    });

    const auto CharlieId = Registry.Register(MakeDescriptor(TEXT("CkTestCharlie"), 20, TEXT("Charlie")));
    const auto BravoId = Registry.Register(MakeDescriptor(TEXT("CkTestBravo"), 10, TEXT("Bravo")));
    const auto AlphaId = Registry.Register(MakeDescriptor(TEXT("CkTestAlpha"), 10, TEXT("Alpha")));

    auto Tools = Registry.Get_Tools();
    TestEqual(TEXT("Three tools registered"), Tools.Num(), 3);
    TestEqual(TEXT("Sort order tie breaks lexically (Alpha)"), Tools[0].Get_TabId(), FName{TEXT("CkTestAlpha")});
    TestEqual(TEXT("Sort order tie breaks lexically (Bravo)"), Tools[1].Get_TabId(), FName{TEXT("CkTestBravo")});
    TestEqual(TEXT("Higher sort order follows"), Tools[2].Get_TabId(), FName{TEXT("CkTestCharlie")});

    const auto ReplacementAlphaId = Registry.Register(
        MakeDescriptor(TEXT("CkTestAlpha"), 5, TEXT("Alpha Replacement")));

    Tools = Registry.Get_Tools();
    TestEqual(TEXT("Replacement keeps one entry per tab id"), Tools.Num(), 3);
    TestEqual(TEXT("Replacement updates sort order"), Tools[0].Get_TabId(), FName{TEXT("CkTestAlpha")});
    TestEqual(TEXT("Replacement updates metadata"), Tools[0].Get_DisplayName().ToString(), FString{TEXT("Alpha Replacement")});
    TestNotEqual(TEXT("Replacement receives a new generation token"), ReplacementAlphaId, AlphaId);

    Registry.Unregister(TEXT("CkTestAlpha"), AlphaId);
    TestEqual(TEXT("Stale generation cannot remove replacement"), Registry.Get_Tools().Num(), 3);

    Registry.Unregister(TEXT("CkTestAlpha"), ReplacementAlphaId);
    Registry.Unregister(TEXT("CkTestBravo"), BravoId);
    Registry.Unregister(TEXT("CkTestCharlie"), CharlieId);
    TestEqual(TEXT("Matching generations remove all entries"), Registry.Get_Tools().Num(), 0);
    TestEqual(TEXT("Only structural changes broadcast"), ChangeCount, 7);

    Registry.Get_OnChanged().Remove(ChangedHandle);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
