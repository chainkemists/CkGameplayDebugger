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

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapDebuggerHistoryModel_Flap,
    "CkGoapDebugger.History.Group.FlapCollapse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkGoapDebuggerHistoryModel_Flap::RunTest(const FString&)
{
    using namespace ck_goap_debugger_history_model;
    using EK = ECkGoapDebugger_HistoryEventKind;

    const auto Ev = [](EK InKind, double InTime, const TCHAR* InName)
    {
        auto E = FCkGoapDebugger_HistoryEvent{};
        E.Kind = InKind; E.WorldTimeSeconds = InTime; E.ActionClassName = InName;
        return E;
    };

    // 6 alternating ACT/DEACT between Shop and Roam, then a PlanFound (breaks the run).
    auto Events = TArray<FCkGoapDebugger_HistoryEvent>{
        Ev(EK::ActionActivated,   1.0, TEXT("Shop")), Ev(EK::ActionDeactivated, 1.1, TEXT("Roam")),
        Ev(EK::ActionActivated,   1.2, TEXT("Roam")), Ev(EK::ActionDeactivated, 1.3, TEXT("Shop")),
        Ev(EK::ActionActivated,   1.4, TEXT("Shop")), Ev(EK::ActionDeactivated, 1.5, TEXT("Roam")),
        Ev(EK::PlanFound,         2.0, TEXT("Roam")),
    };

    const auto Groups = BuildPlannerGroups(Events,
        [](const FCk_Handle_Goap_Planner&) { return FString(TEXT("IntentPlanner")); });

    TestEqual(TEXT("one planner group"),      Groups.Num(), 1);
    TestEqual(TEXT("two rows (flap + plan)"), Groups[0].Rows.Num(), 2);
    TestTrue (TEXT("row0 is flap"),           Groups[0].Rows[0].IsFlap);
    TestEqual(TEXT("flap count 6"),           Groups[0].Rows[0].FlapCount, 6);
    TestEqual(TEXT("flap raw events 6"),      Groups[0].Rows[0].RawEvents.Num(), 6);
    TestEqual(TEXT("flap tStart 1.0"),        Groups[0].Rows[0].FlapTStart, 1.0);
    TestEqual(TEXT("flap tEnd 1.5"),          Groups[0].Rows[0].FlapTEnd, 1.5);
    TestFalse(TEXT("row1 is single"),         Groups[0].Rows[1].IsFlap);

    // Below the threshold (2 events) must NOT collapse.
    auto Short = TArray<FCkGoapDebugger_HistoryEvent>{
        Ev(EK::ActionActivated,   1.0, TEXT("Shop")), Ev(EK::ActionDeactivated, 1.1, TEXT("Roam")),
    };
    const auto ShortGroups = BuildPlannerGroups(Short,
        [](const FCk_Handle_Goap_Planner&) { return FString(TEXT("IntentPlanner")); });
    TestEqual(TEXT("short run stays as singles"), ShortGroups[0].Rows.Num(), 2);
    TestFalse(TEXT("short row0 not flap"),        ShortGroups[0].Rows[0].IsFlap);

    return true;
}
