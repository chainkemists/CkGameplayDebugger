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

    // The facility rows go through a SECOND unbound delegate, and one of them dereferences a TOptional
    // sample — an unbound facts delegate must land on the same "--" as an unset sample, not on a crash.
    TestEqual(TEXT("an unbound facts delegate reads the sample rows as unset"),
        EmptyPanel->Get_RowValueText(TEXT("Friction")).ToString(), FString{TEXT("--")});
    TestEqual(TEXT("an unbound facts delegate reads the character rows as unset"),
        EmptyPanel->Get_RowValueText(TEXT("Ground State")).ToString(), FString{TEXT("--")});

    EmptyPanel->Refresh_Contacts();
    TestEqual(TEXT("an unbound panel has no contacts to list"), EmptyPanel->Get_NumContactRows(), 0);

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

    // The rows read the selection through the delegates on every evaluation, so both fixtures can move
    // underneath them — which is what a live selection change and a fresh capture do.
    auto Selection = TOptional<FCkJoltDebugger_BodySnapshot>{Snapshot};
    auto Facts     = FCkJoltDebugger_SelectionFacts{};

    const auto Panel = SNew(SCkJoltDebugger_DetailPanel)
        .GetSelection(FOnCkJoltDebugger_GetSelection::CreateLambda(
            [&Selection]() -> const TOptional<FCkJoltDebugger_BodySnapshot>&
            { return Selection; }))
        .GetSelectionFacts(FOnCkJoltDebugger_GetSelectionFacts::CreateLambda(
            [&Facts]() -> const FCkJoltDebugger_SelectionFacts&
            { return Facts; }));
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

    // A selection whose sample has not landed yet is the NORMAL state for the frame after a click, and for
    // a sleeping or static body between scene-revision passes — every facility row must degrade, not lie.
    TestEqual(TEXT("with no sample, the mass row degrades"),
        Panel->Get_RowValueText(TEXT("Mass")).ToString(), FString{TEXT("--")});
    TestEqual(TEXT("with no sample, the friction row degrades"),
        Panel->Get_RowValueText(TEXT("Friction")).ToString(), FString{TEXT("--")});
    TestEqual(TEXT("with no sample, the object-layer row degrades"),
        Panel->Get_RowValueText(TEXT("Object Layer")).ToString(), FString{TEXT("--")});
    TestEqual(TEXT("with no sample, the shape-type row degrades"),
        Panel->Get_RowValueText(TEXT("Shape Type")).ToString(), FString{TEXT("--")});

    Selection->LinearVelocity    = FVector{100.0, -50.0, 0.0};
    Selection->HasLinearVelocity = true;

    TestTrue(TEXT("a sampled velocity reaches the row"),
        Panel->Get_RowValueText(TEXT("Linear Velocity")).ToString().Contains(TEXT("100.0")));

    // ---- The facility's rigid-body sample, row by row ----

    auto Sample = FCk_Jolt_DebugDraw_BodySample{};
    Sample.Set_AngularVelocity(FVector{1.0, 0.0, 0.0})
        .Set_Mass(12.5f)
        .Set_Friction(0.5f)
        .Set_Restitution(0.25f)
        .Set_MotionQuality(ECk_MotionQuality::LinearCast)
        .Set_ObjectLayer(static_cast<uint16>(3))
        .Set_IsSensor(true)
        .Set_ShapeType(FString{TEXT("Convex")})
        .Set_AllowsSleeping(TOptional<bool>{false});

    Facts.BodySample = Sample;

    TestEqual(TEXT("the angular-velocity row renders the sample"),
        Panel->Get_RowValueText(TEXT("Angular Velocity")).ToString(), FString{TEXT("1.0, 0.0, 0.0")});
    TestEqual(TEXT("the mass row renders the sample"),
        Panel->Get_RowValueText(TEXT("Mass")).ToString(), FString{TEXT("12.50 kg")});
    TestEqual(TEXT("the friction row renders the sample"),
        Panel->Get_RowValueText(TEXT("Friction")).ToString(), FString{TEXT("0.500")});
    TestEqual(TEXT("the restitution row renders the sample"),
        Panel->Get_RowValueText(TEXT("Restitution")).ToString(), FString{TEXT("0.250")});
    TestEqual(TEXT("the motion-quality row renders the sample"),
        Panel->Get_RowValueText(TEXT("Motion Quality")).ToString(), FString{TEXT("LinearCast (CCD)")});
    TestEqual(TEXT("the object-layer row renders the sample"),
        Panel->Get_RowValueText(TEXT("Object Layer")).ToString(), FString{TEXT("3")});
    TestEqual(TEXT("the sensor row renders the sample"),
        Panel->Get_RowValueText(TEXT("Sensor")).ToString(), FString{TEXT("Yes")});
    TestEqual(TEXT("the shape-type row renders the sample"),
        Panel->Get_RowValueText(TEXT("Shape Type")).ToString(), FString{TEXT("Convex")});
    TestEqual(TEXT("an allowed-sleeping flag renders as a word, not a number"),
        Panel->Get_RowValueText(TEXT("Allows Sleeping")).ToString(), FString{TEXT("No")});

    // ZERO MEANS INFINITE on the facility's side (Jolt stores an inverse mass); rendering "0.00 kg" would
    // say the exact opposite of what a static body's mass is.
    Facts.BodySample->Set_Mass(0.0f);
    TestEqual(TEXT("a zero mass reads as infinite, not as zero"),
        Panel->Get_RowValueText(TEXT("Mass")).ToString(), FString{TEXT("Infinite")});

    // A static body is never asked whether it may sleep — that TOptional is unset, and the row degrades.
    Facts.BodySample->Set_AllowsSleeping(TOptional<bool>{});
    TestEqual(TEXT("an unasked sleeping flag degrades"),
        Panel->Get_RowValueText(TEXT("Allows Sleeping")).ToString(), FString{TEXT("--")});

    // ---- Character group: collapsed for a rigid body, visible for a character ----

    TestFalse(TEXT("the character group is collapsed while a rigid body is selected"),
        Panel->Get_IsCharacterGroupVisible());
    TestEqual(TEXT("and its rows read unset with no character sample"),
        Panel->Get_RowValueText(TEXT("Ground State")).ToString(), FString{TEXT("--")});

    Selection->Population = ECkJoltDebugger_Population::Character;

    TestTrue(TEXT("selecting a character reveals the character group"),
        Panel->Get_IsCharacterGroupVisible());

    auto CharacterSample = FCk_Jolt_DebugDraw_CharacterSample{};
    CharacterSample.Set_GroundState(ECk_JoltCharacter_GroundState::OnGround)
        .Set_Velocity(FVector{0.0, 2.0, 0.0})
        .Set_GroundBodyKey(TOptional<uint64>{7});

    Facts.BodySample.Reset();
    Facts.CharacterSample = CharacterSample;

    TestEqual(TEXT("the ground-state row renders the character sample"),
        Panel->Get_RowValueText(TEXT("Ground State")).ToString(), FString{TEXT("On Ground")});
    TestEqual(TEXT("the character-velocity row renders the character sample"),
        Panel->Get_RowValueText(TEXT("Character Velocity")).ToString(), FString{TEXT("0.0, 2.0, 0.0")});
    TestEqual(TEXT("the ground-body row renders the touched body's key"),
        Panel->Get_RowValueText(TEXT("Ground Body")).ToString(), FString{TEXT("7")});

    // Unsupported: 0 is a VALID body key, so the facility hands back an unset optional rather than a
    // sentinel, and the row has to say so.
    Facts.CharacterSample->Set_GroundBodyKey(TOptional<uint64>{});
    TestEqual(TEXT("an unsupported character has no ground body"),
        Panel->Get_RowValueText(TEXT("Ground Body")).ToString(), FString{TEXT("--")});

    // A character has no rigid-body sample at all — the two are mutually exclusive on the facility's side.
    TestEqual(TEXT("a character selection degrades every rigid-body row"),
        Panel->Get_RowValueText(TEXT("Friction")).ToString(), FString{TEXT("--")});

    // ---- Contacts ----

    auto FirstContact = FCk_Jolt_DebugDraw_ContactEntry{};
    FirstContact.Set_OtherBodyKey(static_cast<uint64>(11)).Set_NumContactPoints(4).Set_PenetrationDepth(0.5f);

    auto SecondContact = FCk_Jolt_DebugDraw_ContactEntry{};
    SecondContact.Set_OtherBodyKey(static_cast<uint64>(12)).Set_NumContactPoints(1).Set_PenetrationDepth(-0.2f);

    Facts.Contacts = {FirstContact, SecondContact};
    Panel->Refresh_Contacts();

    TestEqual(TEXT("every contact the facility reported becomes a row"), Panel->Get_NumContactRows(), 2);

    Facts.Contacts.Reset();
    Panel->Refresh_Contacts();

    TestEqual(TEXT("a selection touching nothing lists no contacts"), Panel->Get_NumContactRows(), 0);

    Selection.Reset();

    TestEqual(TEXT("clearing the selection empties every row"),
        Panel->Get_RowValueText(TEXT("Population")).ToString(), FString{TEXT("--")});
    TestFalse(TEXT("clearing the selection collapses the character group too"),
        Panel->Get_IsCharacterGroupVisible());

    return true;
}

#endif
