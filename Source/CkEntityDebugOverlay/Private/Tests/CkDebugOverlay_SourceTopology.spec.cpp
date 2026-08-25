#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"

#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay_source_topology_spec
{
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Topology_Provider, "Ck.OnScreenDebugger.Provider.SourceTopology")

    static auto MakeDependents() -> TMap<uint32, TArray<uint32>>
    {
        auto Dependents = TMap<uint32, TArray<uint32>>{};
        Dependents.Add(100, { 200, 300 });
        Dependents.Add(200, { 400 });
        Dependents.Add(300, { 500 });
        return Dependents;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SourceTopology_BreadthFirstAndCycleSafe,
    "Ck.DebugOverlay.Presentation.SourceTopology.BreadthFirstAndCycleSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SourceTopology_BreadthFirstAndCycleSafe::RunTest(const FString&)
{
    using namespace ck_debugoverlay;

    auto Dependents = ck_debugoverlay_source_topology_spec::MakeDependents();
    // A malformed revisit must not duplicate the root or move it behind a
    // descendant. The valid sibling order remains the input discovery order.
    // FindOrAdd, not FindChecked: 500 is a leaf here — it appears as a VALUE under 300 and was
    // never added as a key, so FindChecked fataled before this test reached its subject. A fixture
    // that asserts takes the whole editor (and every test batched into its lane) down with it;
    // a fixture that fails closed costs one red line.
    Dependents.FindOrAdd(500).Add(100);
    Dependents.FindOrAdd(300).Add(200);

    const auto Topology = Build_SourceTopology(100, Dependents);
    TestEqual(TEXT("root plus four unique descendants"), Topology.Num(), 5);

    TestEqual(TEXT("root source id"), static_cast<int32>(Topology[0].EntityId), 100);
    TestEqual(TEXT("root parent is sentinel zero"), static_cast<int32>(Topology[0].ParentSourceEntityId), 0);
    TestEqual(TEXT("root depth"), Topology[0].SourceDepth, 0);

    TestEqual(TEXT("first child preserves sibling discovery order"), static_cast<int32>(Topology[1].EntityId), 200);
    TestEqual(TEXT("first child parent"), static_cast<int32>(Topology[1].ParentSourceEntityId), 100);
    TestEqual(TEXT("first child depth"), Topology[1].SourceDepth, 1);

    TestEqual(TEXT("second child preserves sibling discovery order"), static_cast<int32>(Topology[2].EntityId), 300);
    TestEqual(TEXT("second child parent"), static_cast<int32>(Topology[2].ParentSourceEntityId), 100);
    TestEqual(TEXT("second child depth"), Topology[2].SourceDepth, 1);

    TestEqual(TEXT("grandchild comes after its breadth peers"), static_cast<int32>(Topology[3].EntityId), 400);
    TestEqual(TEXT("grandchild parent"), static_cast<int32>(Topology[3].ParentSourceEntityId), 200);
    TestEqual(TEXT("grandchild depth"), Topology[3].SourceDepth, 2);

    TestEqual(TEXT("second branch grandchild follows first"), static_cast<int32>(Topology[4].EntityId), 500);
    TestEqual(TEXT("second branch grandchild parent"), static_cast<int32>(Topology[4].ParentSourceEntityId), 300);
    TestEqual(TEXT("second branch grandchild depth"), Topology[4].SourceDepth, 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SourceTopology_DetailedBuildPreservesSections,
    "Ck.DebugOverlay.Presentation.SourceTopology.DetailedBuildPreservesSections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SourceTopology_DetailedBuildPreservesSections::RunTest(const FString&)
{
    using namespace ck_debugoverlay;

    auto First = FCk_DebugOverlay_Section{};
    First.ProviderTag          = ck_debugoverlay_source_topology_spec::Topology_Provider;
    First.SourceEntityId       = 200;
    First.ParentSourceEntityId = 100;
    First.SourceDepth          = 1;
    First.SourceOrder          = 1;
    First.MergeBehavior        = ECk_DebugOverlay_MergeBehavior::CondensePerSource;
    auto FirstRow = FCk_DebugOverlay_Row{};
    FirstRow.Value = FText::FromString(TEXT("Primary"));
    First.Rows.Add(MoveTemp(FirstRow));

    auto Second = FCk_DebugOverlay_Section{};
    Second.ProviderTag          = ck_debugoverlay_source_topology_spec::Topology_Provider;
    Second.SourceEntityId       = 400;
    Second.ParentSourceEntityId = 200;
    Second.SourceDepth          = 2;
    Second.SourceOrder          = 2;
    Second.MergeBehavior        = ECk_DebugOverlay_MergeBehavior::CondensePerSource;
    auto SecondRow = FCk_DebugOverlay_Row{};
    SecondRow.Value = FText::FromString(TEXT("Secondary"));
    Second.Rows.Add(MoveTemp(SecondRow));

    auto Empty = FCk_DebugOverlay_Section{};
    Empty.ProviderTag          = ck_debugoverlay_source_topology_spec::Topology_Provider;
    Empty.SourceEntityId       = 500;
    Empty.ParentSourceEntityId = 400;
    Empty.SourceDepth          = 3;
    Empty.SourceOrder          = 3;
    Empty.MergeBehavior        = ECk_DebugOverlay_MergeBehavior::CondensePerSource;

    const auto SourceSections = TArray<FCk_DebugOverlay_Section>{ First, Second };
    const auto DefaultOptions = FCk_DebugOverlay_EntityModelBuildOptions{};
    TestTrue(TEXT("default build keeps existing per-source condensation"),
        Should_CondensePerSourceSections(DefaultOptions, /*InMergeDuplicateRows=*/true));

    const auto Condensed = Condense_ProviderSections(
        SourceSections, /*InFocusEntityId=*/100,
        [](const FGameplayTag&, const FText&, const TArray<FCk_DebugOverlay_Row>&) -> FCk_DebugOverlay_Row
        {
            auto Row = FCk_DebugOverlay_Row{};
            Row.Value = FText::FromString(TEXT("Condensed"));
            return Row;
        });
    TestEqual(TEXT("default condensation emits primary plus summary section"), Condensed.Num(), 2);
    TestEqual(TEXT("default condenses the secondary source into its provider summary"),
        Condensed[1].Rows[0].Value.ToString(), FString(TEXT("Condensed")));
    TestEqual(TEXT("summary no longer represents the second source id"),
        static_cast<int32>(Condensed[1].SourceEntityId), 100);

    auto DetailedOptions = FCk_DebugOverlay_EntityModelBuildOptions{};
    DetailedOptions.bCondensePerSourceSections = false;
    TestFalse(TEXT("detailed build bypasses only per-source condensation"),
        Should_CondensePerSourceSections(DetailedOptions, /*InMergeDuplicateRows=*/true));

    // Build_EntityModel keeps this source array when its option gate is false;
    // preparation still runs afterwards and these non-empty unique rows survive it.
    const auto Detailed = Should_CondensePerSourceSections(DetailedOptions, true)
        ? Condense_ProviderSections(SourceSections, 100,
            [](const FGameplayTag&, const FText&, const TArray<FCk_DebugOverlay_Row>&) { return FCk_DebugOverlay_Row{}; })
        : SourceSections;
    TestEqual(TEXT("detailed mode retains both same-provider source sections"), Detailed.Num(), 2);
    TestEqual(TEXT("first section retains parent topology"),
        static_cast<int32>(Detailed[0].ParentSourceEntityId), 100);
    TestEqual(TEXT("first section retains depth"), Detailed[0].SourceDepth, 1);
    TestEqual(TEXT("second section retains source id"), static_cast<int32>(Detailed[1].SourceEntityId), 400);
    TestEqual(TEXT("second section retains parent topology"),
        static_cast<int32>(Detailed[1].ParentSourceEntityId), 200);
    TestEqual(TEXT("second section retains depth"), Detailed[1].SourceDepth, 2);

    auto WithEmpty = Detailed;
    WithEmpty.Add(Empty);
    auto ModelWithEmpty = FCk_DebugOverlay_EntityModel{};
    ModelWithEmpty.Sections = MoveTemp(WithEmpty);
    const auto FocusCardPrepared = Prepare_FocusCardModel(ModelWithEmpty, true);
    TestEqual(TEXT("focus-card cleanup still drops empty sections by default"), FocusCardPrepared.Sections.Num(), 2);
    const auto DetailedPrepared = Prepare_FocusCardModel(ModelWithEmpty, true, /*InRetainEmptySections=*/true);
    TestEqual(TEXT("detailed cleanup retains an empty source as topology"), DetailedPrepared.Sections.Num(), 3);
    TestEqual(TEXT("retained empty topology keeps its source id"),
        static_cast<int32>(DetailedPrepared.Sections[2].SourceEntityId), 500);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
