#include "Misc/AutomationTest.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_HistoryModel.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapDebuggerHistoryModel_Serialize,
    "CkGoapDebugger.History.Serialize.BasicLines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkGoapDebuggerHistoryModel_Serialize::RunTest(const FString&)
{
    using namespace ck_goap_debugger_history_model;
    using EK = ECkGoapDebugger_HistoryEventKind;

    const auto MakeEv = [](EK InKind, double InTime, const TCHAR* InName, const TCHAR* InMeta)
    {
        auto Ev = FCkGoapDebugger_HistoryEvent{};
        Ev.Kind = InKind;
        Ev.WorldTimeSeconds = InTime;
        Ev.ActionClassName = InName;
        Ev.Meta = InMeta;
        return Ev;
    };

    auto Events = TArray<FCkGoapDebugger_HistoryEvent>{
        MakeEv(EK::ActionActivated, 34.538, TEXT("ShoppingTrip"), TEXT("")),
        MakeEv(EK::PlanFound,       42.401, TEXT("RoamPOIs"),     TEXT("cost=3.00, steps=3")),
    };

    const auto Text = SerializeHistory(TEXT("GOAP history - 2 events"), Events,
        [](const FCk_Handle_Goap_Planner&) { return FString(TEXT("IntentPlanner")); });

    TestTrue(TEXT("has header"),       Text.Contains(TEXT("GOAP history - 2 events")));
    TestTrue(TEXT("has ACT row"),      Text.Contains(TEXT("ACT")) && Text.Contains(TEXT("ShoppingTrip")));
    TestTrue(TEXT("has PLAN + meta"),  Text.Contains(TEXT("PLAN")) && Text.Contains(TEXT("cost=3.00, steps=3")));
    TestTrue(TEXT("has planner col"),  Text.Contains(TEXT("IntentPlanner")));
    TestTrue(TEXT("has timestamp"),    Text.Contains(TEXT("00:34.538")));

    return true;
}
