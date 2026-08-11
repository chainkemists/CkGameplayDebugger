#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkSnapshot/Inspection/CkSnapshot_Inspection.h"
#include "CkSnapshot/SaveGame/CkSnapshot_SaveGame.h"

#include "Misc/AutomationTest.h"
#include "Serialization/BufferArchive.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_viz_spec
{
    auto
        MakeEntity(
            uint32 InSavedId,
            ECk_Snapshot_V3_Provenance InProvenance,
            uint32 InLifetimeOwnerSavedId,
            const FString& InLabel = FString{})
        -> FCk_Snapshot_V3_EntityEntry
    {
        auto Entry = FCk_Snapshot_V3_EntityEntry{};
        Entry.Set_SavedId(InSavedId);
        Entry.Set_Provenance(InProvenance);
        Entry.Set_LifetimeOwnerSavedId(InLifetimeOwnerSavedId);

        const auto Name = InLabel.IsEmpty() ? ck::Format_UE(TEXT("Row_{}"), static_cast<int64>(InSavedId)) : InLabel;

        switch (InProvenance)
        {
            case ECk_Snapshot_V3_Provenance::EngineOwned:
            {
                Entry.Set_PlayerId(Name);
                break;
            }
            case ECk_Snapshot_V3_Provenance::ConstructSpawned:
            {
                Entry.Set_Label(Name);
                break;
            }
            case ECk_Snapshot_V3_Provenance::RuntimeSpawned:
            {
                Entry.Set_ScriptClassPath(ck::Format_UE(TEXT("/Game/Spec/{}.{}_C"), Name, Name));
                break;
            }
            case ECk_Snapshot_V3_Provenance::DefinitionBuilt:
            {
                auto Step = FCk_Snapshot_V3_BuildStep{};
                Step.Set_ScriptClassPath(ck::Format_UE(TEXT("/Game/Spec/{}.{}_C"), Name, Name));

                auto Recipe = TArray<FCk_Snapshot_V3_BuildStep>{};
                Recipe.Add(MoveTemp(Step));
                Entry.Set_BuildRecipe(Recipe);
                break;
            }
            default:
                break;
        }

        return Entry;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeDocument(
            const TArray<FCk_Snapshot_V3_EntityEntry>& InEntities,
            const FSoftObjectPath& InWorldAssetPath = FSoftObjectPath{})
        -> FCk_SnapshotInspection_Document
    {
        auto Tables = FCk_Snapshot_V3_Tables{};
        Tables.Set_Entities(InEntities);

        constexpr auto Persistent = true;
        auto Writer = FBufferArchive{Persistent};
        FCk_Snapshot_V3_Tables::StaticStruct()->SerializeItem(Writer, &Tables, nullptr);

        auto Header = FCk_Snapshot_HeaderV3{};
        Header.Set_EntityCount(InEntities.Num());
        Header.Set_EngineVersion(TEXT("spec"));
        Header.Set_WorldAssetPath(InWorldAssetPath);

        auto EngineOwned = 0;
        auto ConstructSpawned = 0;
        auto RuntimeSpawned = 0;
        auto DefinitionBuilt = 0;

        for (const auto& Entry : InEntities)
        {
            switch (Entry.Get_Provenance())
            {
                case ECk_Snapshot_V3_Provenance::EngineOwned:      ++EngineOwned; break;
                case ECk_Snapshot_V3_Provenance::ConstructSpawned: ++ConstructSpawned; break;
                case ECk_Snapshot_V3_Provenance::RuntimeSpawned:   ++RuntimeSpawned; break;
                case ECk_Snapshot_V3_Provenance::DefinitionBuilt:  ++DefinitionBuilt; break;
                default: break;
            }
        }

        Header.Set_EngineOwnedCount(EngineOwned);
        Header.Set_ConstructSpawnedCount(ConstructSpawned);
        Header.Set_RuntimeSpawnedCount(RuntimeSpawned);
        Header.Set_DefinitionBuiltCount(DefinitionBuilt);

        auto SaveGame = TStrongObjectPtr<UCk_Snapshot_SaveGame>{NewObject<UCk_Snapshot_SaveGame>()};
        SaveGame->_HeaderV3 = Header;
        SaveGame->_SnapshotBytesV3 = static_cast<const TArray<uint8>&>(Writer);

        return ck::snapshot::Inspect_SaveGameObject(SaveGame.Get(), TEXT("CkSaveDebugger viz spec fixture"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryFind_Row(
            const TArray<FCkSaveDebugger_VisualizationRow>& InRows,
            uint32 InSavedId)
        -> int32
    {
        for (auto Index = 0; Index < InRows.Num(); ++Index)
        {
            if (InRows[Index].SavedId == InSavedId)
            { return Index; }
        }
        return INDEX_NONE;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Viz_PlacementRule,
    "Ck.SaveDebugger.Viz.PlacementRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Viz_PlacementRule::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_viz_spec;
    namespace model = ck_save_debugger_model;

    const auto PlacedTransform = FTransform{FRotator{0, 45, 0}, FVector{100, 200, 300}};
    const auto SeedTransform   = FTransform{FVector{5, 5, 5}};
    const auto ChildTransform  = FTransform{FVector{100, 200, 400}};
    const auto StrayTransform  = FTransform{FVector{-50, 0, 0}};

    auto Placed = MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Placed"));
    Placed.Set_SavedWorldTransform(PlacedTransform);

    // A bridged actor row: the general column is identity, so the spawn seed is the placement.
    auto Bridged = MakeEntity(2, ECk_Snapshot_V3_Provenance::RuntimeSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Bridged"));
    Bridged.Set_ActorClassPath(TEXT("/Script/Engine.StaticMeshActor"));
    Bridged.Set_ActorSpawnTransform(SeedTransform);

    const auto Unplaced = MakeEntity(3, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Unplaced"));

    auto ChildOfPlaced = MakeEntity(4, ECk_Snapshot_V3_Provenance::ConstructSpawned, 1, TEXT("ChildOfPlaced"));
    ChildOfPlaced.Set_SavedWorldTransform(ChildTransform);

    // Placed itself, but its owner has no placement — the chain line has no other endpoint to reach.
    auto ChildOfUnplaced = MakeEntity(5, ECk_Snapshot_V3_Provenance::ConstructSpawned, 3, TEXT("ChildOfUnplaced"));
    ChildOfUnplaced.Set_SavedWorldTransform(StrayTransform);

    const auto Document = MakeDocument({Placed, Bridged, Unplaced, ChildOfPlaced, ChildOfUnplaced});

    TestEqual(TEXT("Fixture parsed"), static_cast<int32>(Document.Get_ReadStatus()),
        static_cast<int32>(ECk_SnapshotInspection_ReadStatus::Success));

    // The transform helper alone: the general column wins over the seed when both are set.
    {
        auto Both = FCk_Snapshot_V3_EntityEntry{};
        Both.Set_SavedWorldTransform(PlacedTransform);
        Both.Set_ActorSpawnTransform(SeedTransform);

        const auto Preferred = model::TryGet_VisualizationTransform(Both);
        TestTrue(TEXT("Both set -> placed"), Preferred.IsSet());
        if (Preferred.IsSet())
        { TestTrue(TEXT("Both set -> the general column wins"), Preferred->Equals(PlacedTransform)); }

        TestFalse(TEXT("Identity everywhere -> unset"),
            model::TryGet_VisualizationTransform(FCk_Snapshot_V3_EntityEntry{}).IsSet());
    }

    const auto Rows = model::Build_VisualizationRows(Document);

    TestEqual(TEXT("Only placeable rows appear"), Rows.Num(), 4);
    TestEqual(TEXT("The identity-transform row is absent"), TryFind_Row(Rows, 3), INDEX_NONE);

    const auto PlacedIndex = TryFind_Row(Rows, 1);
    const auto BridgedIndex = TryFind_Row(Rows, 2);
    const auto ChildIndex = TryFind_Row(Rows, 4);
    const auto StrayIndex = TryFind_Row(Rows, 5);

    TestTrue(TEXT("Placed row present"), PlacedIndex != INDEX_NONE);
    TestTrue(TEXT("Bridged row present"), BridgedIndex != INDEX_NONE);
    TestTrue(TEXT("Child row present"), ChildIndex != INDEX_NONE);
    TestTrue(TEXT("Stray row present"), StrayIndex != INDEX_NONE);

    if (PlacedIndex != INDEX_NONE)
    { TestTrue(TEXT("Placed at the saved transform"), Rows[PlacedIndex].WorldTransform.Equals(PlacedTransform)); }

    if (BridgedIndex != INDEX_NONE)
    {
        TestTrue(TEXT("Bridged at the spawn seed"), Rows[BridgedIndex].WorldTransform.Equals(SeedTransform));
        TestEqual(TEXT("Bridged row carries its actor class"),
            Rows[BridgedIndex].ActorClassPath, FString{TEXT("/Script/Engine.StaticMeshActor")});
    }

    if (PlacedIndex != INDEX_NONE)
    { TestTrue(TEXT("Non-bridged row carries no actor class"), Rows[PlacedIndex].ActorClassPath.IsEmpty()); }

    // Visual-kind classification: a bridged actor row ghosts (its Construct cannot run without an actor), a plain
    // RuntimeSpawned script row previews (the editor world runs its real Construct), everything else keeps just
    // its diamond.
    {
        const auto KindOf = [](const FCk_Snapshot_V3_EntityEntry& InEntry) -> int32
        { return static_cast<int32>(model::Get_VisualKind(InEntry)); };

        auto BridgedScript = MakeEntity(21, ECk_Snapshot_V3_Provenance::RuntimeSpawned, ck::snapshot::k_NoSavedEntity, TEXT("B"));
        BridgedScript.Set_ActorClassPath(TEXT("/Script/Engine.StaticMeshActor"));
        TestEqual(TEXT("Bridged -> MeshGhost"), KindOf(BridgedScript),
            static_cast<int32>(ECkSaveDebugger_VisualKind::MeshGhost));

        TestEqual(TEXT("Plain script -> ConstructionPreview"),
            KindOf(MakeEntity(22, ECk_Snapshot_V3_Provenance::RuntimeSpawned, ck::snapshot::k_NoSavedEntity, TEXT("S"))),
            static_cast<int32>(ECkSaveDebugger_VisualKind::ConstructionPreview));

        TestEqual(TEXT("ConstructSpawned -> DiamondOnly"),
            KindOf(MakeEntity(23, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("C"))),
            static_cast<int32>(ECkSaveDebugger_VisualKind::DiamondOnly));

        TestEqual(TEXT("EngineOwned -> DiamondOnly"),
            KindOf(MakeEntity(24, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity, TEXT("E"))),
            static_cast<int32>(ECkSaveDebugger_VisualKind::DiamondOnly));

        TestEqual(TEXT("DefinitionBuilt -> DiamondOnly (recipe replay not built)"),
            KindOf(MakeEntity(25, ECk_Snapshot_V3_Provenance::DefinitionBuilt, ck::snapshot::k_NoSavedEntity, TEXT("D"))),
            static_cast<int32>(ECkSaveDebugger_VisualKind::DiamondOnly));
    }

    if (ChildIndex != INDEX_NONE)
    { TestEqual(TEXT("Child links to its placed owner's row"), Rows[ChildIndex].OwnerRowIndex, PlacedIndex); }

    if (StrayIndex != INDEX_NONE)
    { TestEqual(TEXT("Unplaced owner -> no chain link"), Rows[StrayIndex].OwnerRowIndex, INDEX_NONE); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Viz_ProvenanceColors,
    "Ck.SaveDebugger.Viz.ProvenanceColors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Viz_ProvenanceColors::RunTest(const FString& Parameters)
{
    namespace model = ck_save_debugger_model;

    const auto AllProvenances = TArray<ECk_Snapshot_V3_Provenance>{
        ECk_Snapshot_V3_Provenance::EngineOwned,
        ECk_Snapshot_V3_Provenance::ConstructSpawned,
        ECk_Snapshot_V3_Provenance::RuntimeSpawned,
        ECk_Snapshot_V3_Provenance::DefinitionBuilt,
    };

    for (auto IndexA = 0; IndexA < AllProvenances.Num(); ++IndexA)
    {
        const auto ColorA = model::Get_ProvenanceVisualizationColor(AllProvenances[IndexA]);

        TestFalse(
            ck::Format_UE(TEXT("Provenance [{}] tint is distinct from the problem tint"), IndexA),
            ColorA.Equals(CkStyle::Err()));

        for (auto IndexB = IndexA + 1; IndexB < AllProvenances.Num(); ++IndexB)
        {
            TestFalse(
                ck::Format_UE(TEXT("Provenance tints [{}] and [{}] are distinct"), IndexA, IndexB),
                ColorA.Equals(model::Get_ProvenanceVisualizationColor(AllProvenances[IndexB])));
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Viz_SaveKeyAnnotations,
    "Ck.SaveDebugger.Viz.SaveKeyAnnotations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Viz_SaveKeyAnnotations::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_viz_spec;
    namespace model = ck_save_debugger_model;

    const auto KnownKey = FGuid::NewDeterministicGuid(TEXT("/Game/Spec/L_Spec/Shelf_3"));
    const auto UnknownKey = FGuid::NewDeterministicGuid(TEXT("/Game/Spec/L_Spec/NotInTheSave"));

    auto Keyed = MakeEntity(11, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity);
    Keyed.Set_PlayerId(FString{});
    Keyed.Set_SaveKey(KnownKey);
    Keyed.Set_SavedWorldTransform(FTransform{FVector{10, 20, 30}});

    const auto Unkeyed = MakeEntity(12, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Plain"));

    const auto Document = MakeDocument({Keyed, Unkeyed});

    TestEqual(TEXT("Fixture parsed"), static_cast<int32>(Document.Get_ReadStatus()),
        static_cast<int32>(ECk_SnapshotInspection_ReadStatus::Success));

    auto Annotations = TMap<FGuid, FString>{};
    Annotations.Add(KnownKey, TEXT("Shelf_3 (BP_Shelf_C)"));

    // Row lookup by SaveKey — the editor-selection sync path.
    TestEqual(TEXT("Known key resolves its row"),
        static_cast<int64>(model::TryGet_SavedIdForSaveKey(Document, KnownKey)), static_cast<int64>(11));
    TestEqual(TEXT("Unknown key resolves nothing"),
        static_cast<int64>(model::TryGet_SavedIdForSaveKey(Document, UnknownKey)),
        static_cast<int64>(ck::snapshot::k_NoSavedEntity));
    TestEqual(TEXT("Invalid key resolves nothing"),
        static_cast<int64>(model::TryGet_SavedIdForSaveKey(Document, FGuid{})),
        static_cast<int64>(ck::snapshot::k_NoSavedEntity));

    // Display/search enrichment — annotated rows carry the editor-actor name; unannotated rows are untouched.
    const auto& Entities = Document.Get_Entities();
    for (const auto& Summary : Entities)
    {
        const auto Display = model::Build_EntityDisplayText(Summary, Annotations);
        const auto Search = model::Build_EntitySearchText(Summary, Annotations);

        if (Summary.Get_Entry().Get_SavedId() == 11)
        {
            TestTrue(TEXT("Keyed display carries the actor name"), Display.Contains(TEXT("Shelf_3")));
            TestTrue(TEXT("Keyed search matches the actor name"), Search.Contains(TEXT("Shelf_3")));
        }
        else
        {
            TestFalse(TEXT("Unkeyed display is untouched"), Display.Contains(TEXT("Shelf_3")));
            TestEqual(TEXT("Unkeyed display equals the unannotated build"),
                Display, model::Build_EntityDisplayText(Summary));
        }
    }

    // The visualizer rows carry the same enrichment.
    const auto Rows = model::Build_VisualizationRows(Document, Annotations);
    const auto KeyedRowIndex = TryFind_Row(Rows, 11);
    TestTrue(TEXT("Keyed row placed"), KeyedRowIndex != INDEX_NONE);
    if (KeyedRowIndex != INDEX_NONE)
    { TestTrue(TEXT("Keyed viz row carries the actor name"), Rows[KeyedRowIndex].DisplayText.Contains(TEXT("Shelf_3"))); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Viz_LevelMatch,
    "Ck.SaveDebugger.Viz.LevelMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Viz_LevelMatch::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_viz_spec;
    namespace model = ck_save_debugger_model;

    const auto Entities = TArray<FCk_Snapshot_V3_EntityEntry>{
        MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Alpha"))};

    const auto WithWorld = MakeDocument(Entities, FSoftObjectPath{TEXT("/Game/Spec/L_Spec.L_Spec")});
    const auto PieWorld  = MakeDocument(Entities, FSoftObjectPath{TEXT("/Game/Spec/UEDPIE_0_L_Spec.L_Spec")});
    const auto NoWorld   = MakeDocument(Entities);

    const auto ExpectMatch = [&](const TCHAR* InWhat, const FCk_SnapshotInspection_Document& InDocument,
        const FString& InCurrentPackage, ECkSaveDebugger_LevelMatch InExpected) -> void
    {
        TestEqual(InWhat,
            static_cast<int32>(model::Get_LevelMatch(InDocument, InCurrentPackage)),
            static_cast<int32>(InExpected));
    };

    ExpectMatch(TEXT("Same package -> Match"), WithWorld, TEXT("/Game/Spec/L_Spec"), ECkSaveDebugger_LevelMatch::Match);
    ExpectMatch(TEXT("Case difference -> still Match"), WithWorld, TEXT("/Game/Spec/l_spec"), ECkSaveDebugger_LevelMatch::Match);
    ExpectMatch(TEXT("PIE-prefixed capture -> still Match"), PieWorld, TEXT("/Game/Spec/L_Spec"), ECkSaveDebugger_LevelMatch::Match);
    ExpectMatch(TEXT("Different package -> Mismatch"), WithWorld, TEXT("/Game/Spec/L_Other"), ECkSaveDebugger_LevelMatch::Mismatch);
    ExpectMatch(TEXT("Header without a world -> SaveHasNoWorld"), NoWorld, TEXT("/Game/Spec/L_Spec"), ECkSaveDebugger_LevelMatch::SaveHasNoWorld);
    ExpectMatch(TEXT("No editor level -> NoCurrentWorld"), WithWorld, FString{}, ECkSaveDebugger_LevelMatch::NoCurrentWorld);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
