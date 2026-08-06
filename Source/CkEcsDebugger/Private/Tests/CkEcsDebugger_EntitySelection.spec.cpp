#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerSelection_WorldChangeResetDropsHistory,
    "Ck.EcsDebugger.Selection.WorldChangeResetDropsHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerSelection_WorldChangeResetDropsHistory::RunTest(const FString&)
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);

    {
        const auto Entity = FCk_Entity{Registry.create()};
        const auto OldWorldHandle = FCk_Handle{Entity, RegistrySlot};

        auto Selection = FCkDebuggerModel_EntitySelection{};
        auto SelectionBroadcastCount = 0;
        auto LastBroadcastSelection = TArray<FCk_Handle>{};
        Selection.OnSelectionChanged.AddLambda(
            [&SelectionBroadcastCount, &LastBroadcastSelection](const TArray<FCk_Handle>& InSelection)
            {
                ++SelectionBroadcastCount;
                LastBroadcastSelection = InSelection;
            });

        Selection.Set_SelectedEntities({OldWorldHandle});
        Selection.Reset_ForWorldChange();

        TestEqual(TEXT("World reset broadcasts selected then empty"), SelectionBroadcastCount, 2);
        TestTrue(TEXT("World reset broadcasts an empty selection"), LastBroadcastSelection.IsEmpty());
        TestEqual(TEXT("World reset clears current selection"), Selection.Get_SelectionCount(), 0);
        TestFalse(TEXT("World reset drops Back history"), Selection.CanNavigateBack());
        TestFalse(TEXT("World reset drops Forward history"), Selection.CanNavigateForward());
        TestFalse(TEXT("Old-world selection cannot be restored"), Selection.NavigateBack());
    }

    Free(RegistrySlot);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
