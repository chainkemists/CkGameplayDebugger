#include "CkDebuggerCommon/Utils/CkDebug_WorldSpeed.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugWorldSpeed_AuthoritySelectionIsFailClosed,
    "Ck.DebuggerCommon.WorldSpeed.AuthoritySelectionIsFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugWorldSpeed_AuthoritySelectionIsFailClosed::RunTest(const FString& Parameters)
{
    const auto Choose = [](std::initializer_list<ENetMode> InModes, FText& OutReason)
    {
        return ck::DebugWorldSpeed::Choose_AuthorityIndex(TArray<ENetMode>{InModes}, OutReason);
    };

    auto Reason = FText::GetEmpty();
    TestEqual(TEXT("Standalone is authority"), Choose({NM_Standalone}, Reason), 0);
    TestTrue(TEXT("Standalone has no rejection reason"), Reason.IsEmpty());

    TestEqual(TEXT("Listen server is authority"), Choose({NM_ListenServer}, Reason), 0);
    TestEqual(TEXT("Dedicated server is authority"), Choose({NM_DedicatedServer}, Reason), 0);
    TestEqual(TEXT("Listen server is selected over its client"), Choose({NM_Client, NM_ListenServer}, Reason), 1);

    TestEqual(TEXT("Client-only session is rejected"), Choose({NM_Client}, Reason), INDEX_NONE);
    TestFalse(TEXT("Client-only rejection explains why"), Reason.IsEmpty());

    TestEqual(TEXT("Missing session is rejected"), Choose({}, Reason), INDEX_NONE);
    TestFalse(TEXT("Missing-session rejection explains why"), Reason.IsEmpty());

    TestEqual(TEXT("Multiple authority worlds are rejected"),
        Choose({NM_Standalone, NM_ListenServer}, Reason), INDEX_NONE);
    TestFalse(TEXT("Ambiguous-session rejection explains why"), Reason.IsEmpty());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
