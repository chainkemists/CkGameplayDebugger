#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Window/SCkJoltDebugger_OutlinerPanel.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"

namespace ck_jolt_debugger_outliner_spec
{
    auto Make_Snapshot(
        const FCk_Handle& InHandle,
        ECkJoltDebugger_Population InPopulation,
        uint64 InBodyKey,
        const FString& InDisplayName) -> FCkJoltDebugger_BodySnapshot
    {
        auto Snapshot = FCkJoltDebugger_BodySnapshot{};
        Snapshot.Handle      = InHandle;
        Snapshot.Population  = InPopulation;
        Snapshot.BodyKey     = InBodyKey;
        Snapshot.DisplayName = InDisplayName;
        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerOutliner_ConstructsWithoutEnsure,
    "Ck.JoltDebugger.Outliner.ConstructsWithoutEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerOutliner_ConstructsWithoutEnsure::RunTest(const FString&) -> bool
{
    const auto Outliner = SNew(SCkJoltDebugger_OutlinerPanel);
    Outliner->SlatePrepass();

    TestTrue(TEXT("Jolt debugger outliner has a non-empty layout"), Outliner->GetDesiredSize().Y > 0.0f);

    // An empty collector pass is the state the panel lives in outside PIE, and the state it returns to on
    // session invalidation — both must reconcile without a row and without an ensure.
    Outliner->Refresh(TArray<FCkJoltDebugger_BodySnapshot>{});
    Outliner->Clear();
    Outliner->SlatePrepass();

    TestEqual(TEXT("an empty pass leaves no visible row"), Outliner->Get_NumVisibleRows(), 0);
    TestFalse(TEXT("selecting an absent entity resolves to nothing"),
        Outliner->SelectByHandle(FCk_Handle{}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerOutliner_RowsSelectFilterAndSurviveRefresh,
    "Ck.JoltDebugger.Outliner.RowsSelectFilterAndSurviveRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerOutliner_RowsSelectFilterAndSurviveRefresh::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_outliner_spec;

    // Real handles out of a standalone ECS world: the panel keys row identity on the handle, so distinct
    // entities are the only fixture that can prove rows are addressed and not merely counted.
    auto EcsWorld = ck::FEcsWorld{};
    const auto BodyEntity      = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto BakedEntity     = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto CharacterEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());

    const auto Bodies = TArray<FCkJoltDebugger_BodySnapshot>
    {
        Make_Snapshot(BodyEntity,      ECkJoltDebugger_Population::JoltBody,    11, TEXT("Crate")),
        Make_Snapshot(BakedEntity,     ECkJoltDebugger_Population::BakedStatic, 22, TEXT("Floor")),
        Make_Snapshot(CharacterEntity, ECkJoltDebugger_Population::Character,   33, TEXT("Walker"))
    };

    const auto Outliner = SNew(SCkJoltDebugger_OutlinerPanel);
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("every collected body becomes a row"), Outliner->Get_NumVisibleRows(), 3);

    const auto Selected = Outliner->SelectByHandle(BakedEntity);

    if (NOT TestTrue(TEXT("selecting by handle finds the row"), Selected.IsSet()))
    { return false; }

    TestTrue(TEXT("the selected row is the one asked for"), Selected->Handle == BakedEntity);
    TestEqual(TEXT("the selected row carries that body's facts"), Selected->DisplayName, FString{TEXT("Floor")});

    // A refresh over the same entities must reuse the row items, or the view loses the selection with them.
    Outliner->Refresh(Bodies);

    const auto Survivor = Outliner->Get_Selection();
    TestTrue(TEXT("a refresh over the same entities keeps the selection"),
        Survivor.IsSet() && Survivor->Handle == BakedEntity);

    Outliner->Set_FilterQuery(TEXT("Crate"));

    // "Crate" matches one row, but the SELECTED row ("Floor") stays pinned — a selection the filter erases is
    // indistinguishable from no selection, while the detail panel beside it still shows that row's facts.
    TestEqual(TEXT("the filter hides non-matches but keeps the selected row pinned"),
        Outliner->Get_NumVisibleRows(), 2);

    const auto Pinned = Outliner->Get_Selection();
    TestTrue(TEXT("the pinned row is the selection itself"),
        Pinned.IsSet() && Pinned->Handle == BakedEntity);
    TestTrue(TEXT("the pinned row renders dimmed, because it is not what the filter asked for"),
        Outliner->Get_IsRowDimmed(*Pinned));
    TestFalse(TEXT("the row the filter DID match is not dimmed"),
        Outliner->Get_IsRowDimmed(Bodies[0]));

    // The pin follows the selection and nothing else: with no selection, the filter hides the row again.
    Outliner->ClearSelection();
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("with no selection to pin, the filter hides every non-match"),
        Outliner->Get_NumVisibleRows(), 1);

    // An external selector must reach a row the filter is hiding: it clears the filter and reveals it, rather
    // than silently selecting nothing.
    const auto Revealed = Outliner->SelectByHandle(CharacterEntity);

    TestTrue(TEXT("an external selection reaches a filtered-out row"),
        Revealed.IsSet() && Revealed->Handle == CharacterEntity);
    TestEqual(TEXT("reaching it reveals every row again"), Outliner->Get_NumVisibleRows(), 3);

    Outliner->Clear();

    TestEqual(TEXT("clearing drops every row"), Outliner->Get_NumVisibleRows(), 0);
    TestFalse(TEXT("clearing drops the selection with them"), Outliner->Get_Selection().IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerOutliner_MultiSelectKeepsPrimaryAndSurvivesRefresh,
    "Ck.JoltDebugger.Outliner.MultiSelectKeepsPrimaryAndSurvivesRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerOutliner_MultiSelectKeepsPrimaryAndSurvivesRefresh::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_outliner_spec;

    auto EcsWorld = ck::FEcsWorld{};
    const auto BodyEntity      = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto BakedEntity     = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto CharacterEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());

    const auto Bodies = TArray<FCkJoltDebugger_BodySnapshot>
    {
        Make_Snapshot(BodyEntity,      ECkJoltDebugger_Population::JoltBody,    11, TEXT("Crate")),
        Make_Snapshot(BakedEntity,     ECkJoltDebugger_Population::BakedStatic, 22, TEXT("Floor")),
        Make_Snapshot(CharacterEntity, ECkJoltDebugger_Population::Character,   33, TEXT("Walker"))
    };

    const auto Outliner = SNew(SCkJoltDebugger_OutlinerPanel);
    Outliner->Refresh(Bodies);

    Outliner->SelectByHandle(BodyEntity);

    TestEqual(TEXT("a plain select selects exactly one row"), Outliner->Get_NumSelectedRows(), 1);

    // Ctrl+click, whether it lands on a row here or on a body in the viewport, is the same act: the row
    // JOINS the selection and becomes the primary, because it is the one the user acted on LAST.
    const auto Added = Outliner->Add_ToSelection(BakedEntity);

    if (NOT TestTrue(TEXT("adding to the selection finds the row"), Added.IsSet()))
    { return false; }

    TestEqual(TEXT("the addition makes two rows selected"), Outliner->Get_NumSelectedRows(), 2);

    const auto Primary = Outliner->Get_Selection();
    TestTrue(TEXT("the LAST row acted on is the primary"),
        Primary.IsSet() && Primary->Handle == BakedEntity);

    // The facility samples the FIRST highlighted key and asks only that body for its contacts, so the whole
    // set has to arrive primary-first or the detail panel would describe a body the user did not click.
    const auto All = Outliner->Get_SelectedAll();
    TestEqual(TEXT("the whole selected set is reported"), All.Num(), 2);

    if (All.Num() == 2)
    {
        TestTrue(TEXT("the primary leads the set"), All[0].Handle == BakedEntity);
        TestTrue(TEXT("the earlier selection is still in it"), All[1].Handle == BodyEntity);
    }

    // A refresh over the same entities replaces nothing and must keep BOTH rows selected — the pointer-reuse
    // contract plus an identity-keyed selection model, which is what a pointer-keyed one could not do.
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("a refresh over the same entities keeps both rows selected"),
        Outliner->Get_NumSelectedRows(), 2);

    const auto PrimaryAfterRefresh = Outliner->Get_Selection();
    TestTrue(TEXT("and keeps the same primary"),
        PrimaryAfterRefresh.IsSet() && PrimaryAfterRefresh->Handle == BakedEntity);

    // The pin covers EVERY selected row, not just the primary: a filter that erased half a multi-selection
    // would leave the user isolating or highlighting rows they can no longer see.
    Outliner->Set_FilterQuery(TEXT("Walker"));

    TestEqual(TEXT("the filter hides no selected row — both stay pinned beside the one match"),
        Outliner->Get_NumVisibleRows(), 3);
    TestEqual(TEXT("and the selection itself is untouched"), Outliner->Get_NumSelectedRows(), 2);
    TestTrue(TEXT("a pinned selected row renders dimmed"), Outliner->Get_IsRowDimmed(Bodies[0]));
    TestTrue(TEXT("both pinned rows do"), Outliner->Get_IsRowDimmed(Bodies[1]));
    TestFalse(TEXT("the row the filter DID match is not dimmed"), Outliner->Get_IsRowDimmed(Bodies[2]));

    // A selected row whose entity leaves the world is not a selection any more.
    const auto Survivors = TArray<FCkJoltDebugger_BodySnapshot>{Bodies[0], Bodies[2]};
    Outliner->Refresh(Survivors);

    TestEqual(TEXT("a row that left the world leaves the selection with it"),
        Outliner->Get_NumSelectedRows(), 1);

    const auto Promoted = Outliner->Get_Selection();
    TestTrue(TEXT("and the surviving row becomes the primary"),
        Promoted.IsSet() && Promoted->Handle == BodyEntity);

    Outliner->ClearSelection();

    TestEqual(TEXT("clearing drops the whole set"), Outliner->Get_NumSelectedRows(), 0);
    TestFalse(TEXT("and the primary with it"), Outliner->Get_Selection().IsSet());

    /*
     * Everything above drives the panel's MODEL. A real click drives the VIEW, and `SListView` reports it by
     * handing `OnSelectionChanged` an ARBITRARY member of its selection set — so the primary can only come
     * from the set DELTA, and none of the assertions above exercise that derivation (P7-D71/F9).
     */
    Outliner->Refresh(Bodies);
    Outliner->Set_FilterQuery(FString{});

    Outliner->Simulate_RowClick({BodyEntity}, true);

    {
        TestEqual(TEXT("a click selects exactly the row clicked"), Outliner->Get_NumSelectedRows(), 1);

        const auto Clicked = Outliner->Get_Selection();
        TestTrue(TEXT("and makes it the primary"), Clicked.IsSet() && Clicked->Handle == BodyEntity);
    }

    Outliner->Simulate_RowClick({BakedEntity}, true);

    {
        TestEqual(TEXT("a Ctrl+click ADDS rather than replaces"), Outliner->Get_NumSelectedRows(), 2);

        const auto ClickAdded = Outliner->Get_Selection();
        TestTrue(TEXT("and the row it added is the new primary"),
            ClickAdded.IsSet() && ClickAdded->Handle == BakedEntity);
    }

    // A Shift RANGE arrives as ONE signal carrying several additions, which is the case a "primary = the
    // delegate's item" implementation gets wrong most often — the item is not the row the user released on.
    Outliner->ClearSelection();
    Outliner->Simulate_RowClick({BodyEntity, BakedEntity, CharacterEntity}, true);

    {
        TestEqual(TEXT("a range selects every row in it"), Outliner->Get_NumSelectedRows(), 3);

        const auto RangePrimary = Outliner->Get_Selection();

        if (TestTrue(TEXT("a range leaves a primary behind"), RangePrimary.IsSet()))
        {
            TestTrue(TEXT("and it is one of the rows the range added"),
                RangePrimary->Handle == BodyEntity ||
                RangePrimary->Handle == BakedEntity ||
                RangePrimary->Handle == CharacterEntity);
        }

        // A Ctrl+click that REMOVES the primary: the last survivor takes over, because a multi-selection with
        // no primary has nothing to show in the detail panel and nothing for the facility to sample.
        const auto RemovedHandle = RangePrimary.IsSet() ? RangePrimary->Handle : FCk_Handle{};

        Outliner->Simulate_RowClick({RemovedHandle}, false);

        TestEqual(TEXT("ctrl-removing a row drops exactly that row"), Outliner->Get_NumSelectedRows(), 2);

        const auto PromotedAfterRemoval = Outliner->Get_Selection();
        TestTrue(TEXT("and a survivor is promoted to primary"),
            PromotedAfterRemoval.IsSet() && PromotedAfterRemoval->Handle != RemovedHandle);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerOutliner_ListsConstraintRows,
    "Ck.JoltDebugger.Outliner.ListsConstraintRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerOutliner_ListsConstraintRows::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_outliner_spec;

    auto EcsWorld = ck::FEcsWorld{};
    const auto BodyAEntity     = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto BodyBEntity     = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto ConstraintEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());

    auto ConstraintRow = Make_Snapshot(ConstraintEntity, ECkJoltDebugger_Population::Constraint, 0, TEXT("Rope Link"));

    // A constraint row carries NO body key of its own — it draws nothing. What it carries is the pair it joins,
    // which is what the window turns into a two-body highlight.
    ConstraintRow.BodyKey.Reset();
    ConstraintRow.ConstraintBodyKeys = TArray<uint64>{11, 22};
    ConstraintRow.ConstraintType     = ECk_JoltConstraint_Type::Hinge;
    ConstraintRow.NumBodies          = 2;

    const auto Bodies = TArray<FCkJoltDebugger_BodySnapshot>
    {
        Make_Snapshot(BodyAEntity, ECkJoltDebugger_Population::JoltBody, 11, TEXT("Crate")),
        Make_Snapshot(BodyBEntity, ECkJoltDebugger_Population::JoltBody, 22, TEXT("Hook")),
        ConstraintRow
    };

    const auto Outliner = SNew(SCkJoltDebugger_OutlinerPanel);
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("the constraint is listed beside the bodies it joins"), Outliner->Get_NumVisibleRows(), 3);

    const auto Selected = Outliner->SelectByHandle(ConstraintEntity);

    if (NOT TestTrue(TEXT("a constraint row is selectable"), Selected.IsSet()))
    { return false; }

    TestEqual(TEXT("and it reports the population it belongs to"),
        static_cast<int32>(Selected->Population),
        static_cast<int32>(ECkJoltDebugger_Population::Constraint));

    TestFalse(TEXT("a constraint row has no drawn body of its own"), Selected->BodyKey.IsSet());

    TestEqual(TEXT("it names BOTH bodies it joins"), Selected->ConstraintBodyKeys.Num(), 2);

    if (Selected->ConstraintBodyKeys.Num() == 2)
    {
        TestEqual(TEXT("body A leads, because the facility samples the FIRST highlighted key"),
            Selected->ConstraintBodyKeys[0], static_cast<uint64>(11));
        TestEqual(TEXT("body B follows"), Selected->ConstraintBodyKeys[1], static_cast<uint64>(22));
    }

    TestEqual(TEXT("and it reports its constraint flavour"),
        static_cast<int32>(Selected->ConstraintType),
        static_cast<int32>(ECk_JoltConstraint_Type::Hinge));

    // Row identity is (handle, population), so a constraint whose entity is ALSO a body would still be two
    // rows. Here the plainer proof: a refresh over the same set keeps the constraint selected.
    Outliner->Refresh(Bodies);

    const auto Survivor = Outliner->Get_Selection();
    TestTrue(TEXT("a refresh keeps the constraint selected"),
        Survivor.IsSet() && Survivor->Handle == ConstraintEntity);

    // The row renders through the shared filter path like any other, so its own text is searchable.
    Outliner->ClearSelection();
    Outliner->Set_FilterQuery(TEXT("Rope"));

    TestEqual(TEXT("the constraint row answers the text filter"), Outliner->Get_NumVisibleRows(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerOutliner_ProblemsChipNarrowsToFlaggedRows,
    "Ck.JoltDebugger.Outliner.ProblemsChipNarrowsToFlaggedRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerOutliner_ProblemsChipNarrowsToFlaggedRows::RunTest(const FString&) -> bool
{
    using namespace ck_jolt_debugger_outliner_spec;

    auto EcsWorld = ck::FEcsWorld{};
    const auto HealthyEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto BrokenEntity  = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    const auto OtherEntity   = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());

    auto Broken = Make_Snapshot(BrokenEntity, ECkJoltDebugger_Population::JoltBody, 22, TEXT("Runaway"));
    Broken.ProblemFlags = ECk_Jolt_DebugDraw_ProblemFlags::RunawayVelocity;

    const auto Bodies = TArray<FCkJoltDebugger_BodySnapshot>
    {
        Make_Snapshot(HealthyEntity, ECkJoltDebugger_Population::JoltBody, 11, TEXT("Crate")),
        Broken,
        Make_Snapshot(OtherEntity, ECkJoltDebugger_Population::Character, 33, TEXT("Walker"))
    };

    const auto Outliner = SNew(SCkJoltDebugger_OutlinerPanel);
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("all three rows are listed to begin with"), Outliner->Get_NumVisibleRows(), 3);
    TestEqual(TEXT("exactly one of them is flagged"), Outliner->Get_NumProblemRows(), 1);
    TestFalse(TEXT("the chip starts off"), Outliner->Get_IsProblemsFilterActive());

    Outliner->Set_ProblemsFilter(true);

    TestTrue(TEXT("the chip is on"), Outliner->Get_IsProblemsFilterActive());
    TestEqual(TEXT("and it leaves exactly the flagged row visible"), Outliner->Get_NumVisibleRows(), 1);

    // The pin outranks the chip for the same reason it outranks the text filter: a selection the user cannot
    // see is indistinguishable from no selection, and the detail panel beside it is still showing that row.
    Outliner->Set_ProblemsFilter(false);
    Outliner->SelectByHandle(HealthyEntity);
    Outliner->Set_ProblemsFilter(true);

    TestEqual(TEXT("a selected healthy row stays pinned beside the flagged one"),
        Outliner->Get_NumVisibleRows(), 2);
    TestTrue(TEXT("and it renders dimmed, because it is not what the chip asked for"),
        Outliner->Get_IsRowDimmed(Bodies[0]));

    Outliner->ClearSelection();
    Outliner->Refresh(Bodies);

    TestEqual(TEXT("with nothing pinned the chip narrows to one again"), Outliner->Get_NumVisibleRows(), 1);

    Outliner->Set_ProblemsFilter(false);

    TestEqual(TEXT("clearing the chip restores all three"), Outliner->Get_NumVisibleRows(), 3);

    // The flags are LIVE state pushed in by the collector, so a body that stopped being broken stops being
    // listed by the chip without anyone touching the chip.
    auto Healed = Bodies;
    Healed[1].ProblemFlags = ECk_Jolt_DebugDraw_ProblemFlags::None;
    Outliner->Refresh(Healed);
    Outliner->Set_ProblemsFilter(true);

    TestEqual(TEXT("a healed body leaves the chip with nothing to show"), Outliner->Get_NumProblemRows(), 0);
    TestEqual(TEXT("and no row survives it"), Outliner->Get_NumVisibleRows(), 0);

    return true;
}

#endif
