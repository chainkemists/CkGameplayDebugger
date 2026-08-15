#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Window/SCkJoltDebugger_DetailPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerDetail_ConstructsWithoutEnsure,
    "Ck.JoltDebugger.Detail.ConstructsWithoutEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerDetail_ConstructsWithoutEnsure::RunTest(const FString&) -> bool
{
    // No GetSelection binding at all is the harshest case: every value lambda must survive an unbound
    // delegate, which is exactly what a window mid-teardown hands it.
    const auto EmptyPanel = SNew(SCkJoltDebugger_DetailPanel);
    EmptyPanel->SlatePrepass();

    TestTrue(TEXT("Jolt debugger detail panel has a non-empty layout"), EmptyPanel->GetDesiredSize().Y > 0.0f);
    TestEqual(TEXT("an unbound panel reads every row as unset"),
        EmptyPanel->Get_RowValueText(TEXT("Body Key")).ToString(), FString{TEXT("--")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerDetail_RowsReflectTheSelection,
    "Ck.JoltDebugger.Detail.RowsReflectTheSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerDetail_RowsReflectTheSelection::RunTest(const FString&) -> bool
{
    auto Snapshot = FCkJoltDebugger_BodySnapshot{};
    Snapshot.BodyKey            = 42;
    Snapshot.Population         = ECkJoltDebugger_Population::JoltBody;
    Snapshot.MotionType         = ECk_MotionType::Dynamic;
    Snapshot.SleepState         = ECk_Jolt_SleepState::Asleep;
    Snapshot.HasSimulationState = true;
    Snapshot.DisplayName        = TEXT("TestBody");

    // The rows read the selection through the delegate on every evaluation, so the fixture can move underneath
    // them — which is what a live selection change does.
    auto Selection = TOptional<FCkJoltDebugger_BodySnapshot>{Snapshot};

    const auto Panel = SNew(SCkJoltDebugger_DetailPanel)
        .GetSelection(FOnCkJoltDebugger_GetSelection::CreateLambda(
            [&Selection]() -> const TOptional<FCkJoltDebugger_BodySnapshot>&
            { return Selection; }));
    Panel->SlatePrepass();

    TestTrue(TEXT("Jolt debugger detail panel lays out with a selection"), Panel->GetDesiredSize().Y > 0.0f);

    TestEqual(TEXT("the body-key row renders the selection's key"),
        Panel->Get_RowValueText(TEXT("Body Key")).ToString(), FString{TEXT("42")});
    TestEqual(TEXT("the population row renders the selection's population"),
        Panel->Get_RowValueText(TEXT("Population")).ToString(), FString{TEXT("Jolt Body")});
    TestEqual(TEXT("the motion-type row renders the selection's motion type"),
        Panel->Get_RowValueText(TEXT("Motion Type")).ToString(), FString{TEXT("Dynamic")});
    TestEqual(TEXT("the sleep-state row renders the selection's sleep state"),
        Panel->Get_RowValueText(TEXT("Sleep State")).ToString(), FString{TEXT("Asleep")});

    // Velocity is sampled by the facility's capture, so a selection without one must read unset rather than
    // rendering a zero the user would take for a measurement.
    TestEqual(TEXT("an unsampled velocity reads as unset"),
        Panel->Get_RowValueText(TEXT("Linear Velocity")).ToString(), FString{TEXT("--")});

    Selection->LinearVelocity    = FVector{100.0, -50.0, 0.0};
    Selection->HasLinearVelocity = true;

    TestTrue(TEXT("a sampled velocity reaches the row"),
        Panel->Get_RowValueText(TEXT("Linear Velocity")).ToString().Contains(TEXT("100.0")));

    // A baked-static row has no body key of its own when its bodies are gone; the row must degrade, not lie.
    Selection->BodyKey.Reset();

    TestEqual(TEXT("a row with no body behind it reads as unset"),
        Panel->Get_RowValueText(TEXT("Body Key")).ToString(), FString{TEXT("--")});

    Selection.Reset();

    TestEqual(TEXT("clearing the selection empties every row"),
        Panel->Get_RowValueText(TEXT("Population")).ToString(), FString{TEXT("--")});

    return true;
}

#endif
