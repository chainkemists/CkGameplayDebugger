#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityHealthList.h"

#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

namespace ck_debug_entity_health_list_tests
{
    auto MakeItem(
        const FCk_Handle& InRowIdentity,
        const FCk_Handle& InSelectionTarget,
        const TCHAR* InName = TEXT("Agent")) -> FCkDebug_EntityHealthItem
    {
        auto Item = FCkDebug_EntityHealthItem{};
        Item.RowIdentity = InRowIdentity;
        Item.SelectionTarget = InSelectionTarget;
        Item.Name = FText::FromString(InName);
        Item.Summary = FText::FromString(TEXT("Current health summary"));
        Item.Context = FText::FromString(TEXT("Current health context"));
        Item.Status = FText::FromString(TEXT("ACTIVE"));
        Item.Tone = ECk_Tone::Ok;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugEntityHealthList_RejectsInvalidAndDuplicateRowsAtomically,
    "Ck.DebuggerCommon.EntityHealthList.RejectsInvalidAndDuplicateRowsAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEntityHealthList_RejectsInvalidAndDuplicateRowsAtomically::RunTest(const FString&)
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    {
        const auto Agent = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto Target = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        auto Existing = TArray<ck::debug_entity_health_list::FItemPtr>{
            MakeShared<FCkDebug_EntityHealthItem>(ck_debug_entity_health_list_tests::MakeItem(Agent, Target, TEXT("Original")))};
        const auto Retained = Existing[0];
        auto Output = Existing;
        auto RenderedContentChanged = false;
        auto Error = FString{};

        const auto DuplicateAccepted = ck::debug_entity_health_list::Try_Reconcile(
            Existing,
            {ck_debug_entity_health_list_tests::MakeItem(Agent, Target, TEXT("First")),
             ck_debug_entity_health_list_tests::MakeItem(Agent, Target, TEXT("Second"))},
            Output,
            RenderedContentChanged,
            Error);
        TestFalse(TEXT("Duplicate row identity is rejected"), DuplicateAccepted);
        TestTrue(TEXT("Duplicate rejection preserves the output pointer"), Output[0] == Retained);
        TestEqual(TEXT("Duplicate rejection preserves retained content"), Retained->Name.ToString(), FString(TEXT("Original")));
        TestFalse(TEXT("Duplicate rejection leaves change reporting untouched"), RenderedContentChanged);

        const auto InvalidIdentityAccepted = ck::debug_entity_health_list::Try_Reconcile(
            Existing,
            {ck_debug_entity_health_list_tests::MakeItem(FCk_Handle{}, Target)},
            Output,
            RenderedContentChanged,
            Error);
        TestFalse(TEXT("Invalid row identity is rejected"), InvalidIdentityAccepted);
        TestTrue(TEXT("Invalid identity rejection remains atomic"), Output[0] == Retained);

        const auto InvalidTargetAccepted = ck::debug_entity_health_list::Try_Reconcile(
            Existing,
            {ck_debug_entity_health_list_tests::MakeItem(Agent, FCk_Handle{})},
            Output,
            RenderedContentChanged,
            Error);
        TestFalse(TEXT("Invalid selection target is rejected"), InvalidTargetAccepted);
        TestTrue(TEXT("Invalid target rejection remains atomic"), Output[0] == Retained);
        TestEqual(TEXT("Invalid target rejection preserves retained content"), Retained->Name.ToString(), FString(TEXT("Original")));
    }
    Free(RegistrySlot);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugEntityHealthList_ReconcilesPhysicalRowsSeparateFromSelectionTarget,
    "Ck.DebuggerCommon.EntityHealthList.ReconcilesPhysicalRowsSeparateFromSelectionTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEntityHealthList_ReconcilesPhysicalRowsSeparateFromSelectionTarget::RunTest(const FString&)
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    {
        const auto AgentA = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto AgentB = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        const auto SharedTarget = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        auto FirstRefresh = TArray<ck::debug_entity_health_list::FItemPtr>{};
        auto RenderedContentChanged = false;
        auto Error = FString{};

        const auto FirstAccepted = ck::debug_entity_health_list::Try_Reconcile(
            {},
            {ck_debug_entity_health_list_tests::MakeItem(AgentA, SharedTarget, TEXT("Agent A")),
             ck_debug_entity_health_list_tests::MakeItem(AgentB, SharedTarget, TEXT("Agent B"))},
            FirstRefresh,
            RenderedContentChanged,
            Error);
        TestTrue(TEXT("Two physical rows may share one selection target"), FirstAccepted);
        TestTrue(TEXT("First refresh changes rendered content"), RenderedContentChanged);
        TestEqual(TEXT("Both physical rows survive first refresh"), FirstRefresh.Num(), 2);
        TestTrue(TEXT("Each physical row owns a distinct shared pointer"), FirstRefresh[0] != FirstRefresh[1]);
        const auto AgentAPointer = FirstRefresh[0];
        const auto AgentBPointer = FirstRefresh[1];

        auto SecondRefresh = TArray<ck::debug_entity_health_list::FItemPtr>{};
        const auto SecondAccepted = ck::debug_entity_health_list::Try_Reconcile(
            FirstRefresh,
            {ck_debug_entity_health_list_tests::MakeItem(AgentA, SharedTarget, TEXT("Agent A (updated)")),
             ck_debug_entity_health_list_tests::MakeItem(AgentB, SharedTarget, TEXT("Agent B (updated)"))},
            SecondRefresh,
            RenderedContentChanged,
            Error);
        TestTrue(TEXT("Second physical-row refresh is accepted"), SecondAccepted);
        TestTrue(TEXT("Changed text reports a rendered-content refresh"), RenderedContentChanged);
        TestTrue(TEXT("Agent A retains its row pointer"), SecondRefresh[0] == AgentAPointer);
        TestTrue(TEXT("Agent B retains its row pointer"), SecondRefresh[1] == AgentBPointer);
        TestTrue(TEXT("Rows still target the same conceptual entity"),
                 SecondRefresh[0]->SelectionTarget == SecondRefresh[1]->SelectionTarget);

        auto ThirdRefresh = TArray<ck::debug_entity_health_list::FItemPtr>{};
        const auto ThirdAccepted = ck::debug_entity_health_list::Try_Reconcile(
            SecondRefresh,
            {ck_debug_entity_health_list_tests::MakeItem(AgentA, SharedTarget, TEXT("Agent A (updated)")),
             ck_debug_entity_health_list_tests::MakeItem(AgentB, SharedTarget, TEXT("Agent B (updated)"))},
            ThirdRefresh,
            RenderedContentChanged,
            Error);
        TestTrue(TEXT("Unchanged input is accepted"), ThirdAccepted);
        TestFalse(TEXT("Unchanged input reports no rendered-content refresh"), RenderedContentChanged);
        TestTrue(TEXT("Unchanged Agent A retains its pointer"), ThirdRefresh[0] == AgentAPointer);
        TestTrue(TEXT("Unchanged Agent B retains its pointer"), ThirdRefresh[1] == AgentBPointer);
    }
    Free(RegistrySlot);
    return true;
}

#endif
