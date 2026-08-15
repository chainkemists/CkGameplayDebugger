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

    TestEqual(TEXT("the filter hides the rows that do not match"), Outliner->Get_NumVisibleRows(), 1);

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

#endif
