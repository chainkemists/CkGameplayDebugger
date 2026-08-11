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

namespace ck_save_debugger_model_spec
{
    // Fixtures go through the REAL read path — hand-built tables, serialized exactly as capture writes them, wrapped
    // in a save object and handed to Inspect_SaveGameObject — so the diagnostics and HasProblems flags the model
    // filters on are the ones the shipping analyzer produces, not a hand-written imitation.
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
        MakePayload(
            uint32 InOwnerSavedId,
            const FString& InTypePath,
            const TArray<uint8>& InBytes)
        -> FCk_Snapshot_V3_PayloadEntry
    {
        auto Entry = FCk_Snapshot_V3_PayloadEntry{};
        Entry.Set_OwnerSavedId(InOwnerSavedId);
        Entry.Set_TypePath(InTypePath);
        Entry.Set_PayloadBytes(InBytes);
        return Entry;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeDocument(
            const TArray<FCk_Snapshot_V3_EntityEntry>& InEntities,
            const TArray<FCk_Snapshot_V3_PayloadEntry>& InPayloads)
        -> FCk_SnapshotInspection_Document
    {
        auto Tables = FCk_Snapshot_V3_Tables{};
        Tables.Set_Entities(InEntities);
        Tables.Set_Payloads(InPayloads);

        constexpr auto Persistent = true;
        auto Writer = FBufferArchive{Persistent};
        FCk_Snapshot_V3_Tables::StaticStruct()->SerializeItem(Writer, &Tables, nullptr);

        // Census matches the tables so the fixture never trips HeaderCensusMismatch by accident — a test that wants
        // that diagnostic must ask for it explicitly.
        auto Header = FCk_Snapshot_HeaderV3{};
        Header.Set_EntityCount(InEntities.Num());
        Header.Set_PayloadCount(InPayloads.Num());

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
        Header.Set_EngineVersion(TEXT("spec"));

        auto SaveGame = TStrongObjectPtr<UCk_Snapshot_SaveGame>{NewObject<UCk_Snapshot_SaveGame>()};
        SaveGame->_HeaderV3 = Header;
        SaveGame->_SnapshotBytesV3 = static_cast<const TArray<uint8>&>(Writer);

        return ck::snapshot::Inspect_SaveGameObject(SaveGame.Get(), TEXT("CkSaveDebugger spec fixture"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_VisibleRootCount(
            const FCkSaveDebugger_Model& InModel)
        -> int32
    {
        auto Count = 0;
        for (const auto& Root : InModel.Get_TreeRoots())
        {
            if (Root.IsValid() && Root->IsVisible)
            { ++Count; }
        }
        return Count;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryFind_Node(
            const FCkSaveDebugger_Model& InModel,
            uint32 InSavedId)
        -> TSharedPtr<FCkSaveDebugger_TreeNode>
    {
        auto Stack = InModel.Get_TreeRoots();
        while (Stack.Num() > 0)
        {
            const auto Node = Stack.Pop();
            if (NOT Node.IsValid())
            { continue; }

            if (Node->Kind == ECkSaveDebugger_NodeKind::Entity && Node->SavedId == InSavedId)
            { return Node; }

            Stack.Append(Node->Children);
        }

        return {};
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_FilterAndHighlight,
    "Ck.SaveDebugger.Model.FilterAndHighlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_FilterAndHighlight::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Document = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Alpha")),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Bravo")),
            MakeEntity(3, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Charlie")),
        },
        {});

    TestEqual(TEXT("Fixture parsed"), static_cast<int32>(Document.Get_ReadStatus()),
        static_cast<int32>(ECk_SnapshotInspection_ReadStatus::Success));
    TestEqual(TEXT("Three rows"), Document.Get_Entities().Num(), 3);

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    TestEqual(TEXT("No filter shows every root"), Get_VisibleRootCount(Model), 3);

    Model.Set_FilterString(TEXT("alpha"));
    Model.Apply_Filters();
    TestEqual(TEXT("Filter is case-insensitive and narrows to one row"), Get_VisibleRootCount(Model), 1);

    Model.Set_FilterString(TEXT("nothing-matches-this"));
    Model.Apply_Filters();
    TestEqual(TEXT("A filter nothing matches hides every root"), Get_VisibleRootCount(Model), 0);

    Model.Set_FilterString(FString{});
    Model.Set_HighlightString(TEXT("brav"));
    Model.Apply_Filters();

    TestEqual(TEXT("Highlight never hides a row"), Get_VisibleRootCount(Model), 3);
    TestTrue(TEXT("Highlighted row matches"), TryFind_Node(Model, 2)->IsSearchMatch);
    TestFalse(TEXT("Non-highlighted row is dimmed"), TryFind_Node(Model, 1)->IsSearchMatch);

    Model.Set_HighlightString(FString{});
    Model.Apply_Filters();
    TestTrue(TEXT("An empty highlight matches everything"), TryFind_Node(Model, 1)->IsSearchMatch);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_ProvenanceAndProblemsOnly,
    "Ck.SaveDebugger.Model.ProvenanceAndProblemsOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_ProvenanceAndProblemsOnly::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    // Row 5 is a ConstructSpawned row naming an owner the table does not contain: its adopt key can never match,
    // so the analyzer flags it (Warning) — which is what ProblemsOnly must be able to isolate.
    const auto Document = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(3, ECk_Snapshot_V3_Provenance::RuntimeSpawned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(4, ECk_Snapshot_V3_Provenance::DefinitionBuilt, 1),
            MakeEntity(5, ECk_Snapshot_V3_Provenance::ConstructSpawned, 999),
        },
        {});

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    TestEqual(TEXT("Unfiltered forest has three entity roots plus the non-persisted-owner group"),
        Get_VisibleRootCount(Model), 4);

    Model.Set_ProvenanceMask(ck_save_debugger_model::Get_ProvenanceBit(ECk_Snapshot_V3_Provenance::RuntimeSpawned));
    Model.Apply_Filters();

    TestEqual(TEXT("A single-provenance mask leaves only that row's root"), Get_VisibleRootCount(Model), 1);
    TestTrue(TEXT("The surviving root is the RuntimeSpawned row"), TryFind_Node(Model, 3)->IsVisible);
    TestFalse(TEXT("An excluded provenance is hidden"), TryFind_Node(Model, 2)->IsVisible);

    Model.Set_ProvenanceMask(ck_save_debugger_model::Get_AllProvenanceMask());
    Model.Set_ProblemsOnly(true);
    Model.Apply_Filters();

    TestTrue(TEXT("The defective row survives ProblemsOnly"), TryFind_Node(Model, 5)->IsVisible);
    TestFalse(TEXT("A clean row does not"), TryFind_Node(Model, 2)->IsVisible);
    TestEqual(TEXT("Only the non-persisted-owner group remains visible"), Get_VisibleRootCount(Model), 1);

    Model.Set_ProblemsOnly(false);
    Model.Apply_Filters();
    TestTrue(TEXT("Clearing ProblemsOnly restores the clean rows"), TryFind_Node(Model, 2)->IsVisible);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_DiagnosticRowMapping,
    "Ck.SaveDebugger.Model.DiagnosticRowMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_DiagnosticRowMapping::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Document = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
        },
        {
            // Row 0: present owner, no bytes  -> PayloadBytesEmpty, targets BOTH the entity and the payload.
            MakePayload(1, TEXT("/Script/Spec.Spec_RepData"), {}),
            // Row 1: absent owner             -> PayloadOwnerMissing, targets the payload only.
            MakePayload(777, TEXT("/Script/Spec.Spec_RepData"), TArray<uint8>{1, 2, 3}),
        });

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    const auto Rows = Model.Build_DiagnosticRows();
    TestTrue(TEXT("The fixture produced diagnostics"), Rows.Num() > 0);

    auto SawEmptyBytes = false;
    auto SawMissingOwner = false;

    for (const auto& Row : Rows)
    {
        if (Row.CodeText == TEXT("PayloadBytesEmpty"))
        {
            SawEmptyBytes = true;
            TestEqual(TEXT("Empty-payload diagnostic points at its payload row"), Row.Target.PayloadIndex, 0);
            TestEqual(TEXT("Empty-payload diagnostic points at its owning entity"),
                static_cast<int64>(Row.Target.EntitySavedId), static_cast<int64>(1));
        }

        if (Row.CodeText == TEXT("PayloadOwnerMissing"))
        {
            SawMissingOwner = true;
            TestEqual(TEXT("Orphan-payload diagnostic points at its payload row"), Row.Target.PayloadIndex, 1);
            TestFalse(TEXT("Orphan-payload diagnostic resolves no entity row"), Row.Target.Get_HasEntity());
        }
    }

    TestTrue(TEXT("PayloadBytesEmpty was emitted"), SawEmptyBytes);
    TestTrue(TEXT("PayloadOwnerMissing was emitted"), SawMissingOwner);

    // Errors sort ahead of Warnings ahead of Info, so the list's first screen explains the summary tile; the
    // DiagnosticIndex each row carries still addresses the document's diagnostics array for selection mapping.
    for (auto Index = 1; Index < Rows.Num(); ++Index)
    {
        TestTrue(TEXT("Rows are ordered by non-increasing severity"),
            static_cast<uint8>(Rows[Index - 1].Severity) >= static_cast<uint8>(Rows[Index].Severity));
    }

    TestEqual(TEXT("The Error row reads first"),
        static_cast<int32>(Rows[0].Severity), static_cast<int32>(ECk_SnapshotInspection_Severity::Error));

    for (const auto& Row : Rows)
    {
        TestEqual(TEXT("Each row's DiagnosticIndex addresses its document diagnostic"),
            Row.CodeText,
            ck::snapshot::Get_DiagnosticCodeText(Document.Get_Diagnostics()[Row.DiagnosticIndex].Get_Code()));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_StableRowIdentity,
    "Ck.SaveDebugger.Model.StableRowIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_StableRowIdentity::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Document = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, 1),
        },
        {});

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    const auto FirstRoot = Model.Get_TreeRoots()[0];
    const auto FirstChild = TryFind_Node(Model, 2);

    TestTrue(TEXT("The child hangs under its lifetime owner"), FirstChild.IsValid());

    const auto SetChanged = Model.Rebuild_Tree();
    TestFalse(TEXT("Rebuilding an unchanged document reports no set change"), SetChanged);

    TestTrue(TEXT("Root row identity survives a rebuild"), Model.Get_TreeRoots()[0] == FirstRoot);
    TestTrue(TEXT("Child row identity survives a rebuild"), TryFind_Node(Model, 2) == FirstChild);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_SyntheticRoots,
    "Ck.SaveDebugger.Model.SyntheticRoots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_SyntheticRoots::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    // Two rows owning each other. A naive owner-chain walk would never terminate here; the layout must classify
    // both as cycle members and hang them off one synthetic root.
    const auto CyclicDocument = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, 2),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::EngineOwned, 1),
        },
        {});

    const auto CyclicLayout = ck_save_debugger_model::Build_OwnershipLayout(CyclicDocument);

    TestEqual(TEXT("A cyclic save produces exactly one root"), CyclicLayout.RootLayoutIndices.Num(), 1);

    const auto& CycleRoot = CyclicLayout.Nodes[CyclicLayout.RootLayoutIndices[0]];
    TestEqual(TEXT("That root is the synthetic Cycle root"),
        static_cast<int32>(CycleRoot.Kind), static_cast<int32>(ECkSaveDebugger_NodeKind::CycleRoot));
    TestEqual(TEXT("Both cycle members hang off it"), CycleRoot.ChildLayoutIndices.Num(), 2);

    // A self-owning row is the degenerate cycle and must be classified identically.
    const auto SelfCyclicDocument = MakeDocument(
        {
            MakeEntity(7, ECk_Snapshot_V3_Provenance::EngineOwned, 7),
        },
        {});

    const auto SelfCyclicLayout = ck_save_debugger_model::Build_OwnershipLayout(SelfCyclicDocument);
    TestEqual(TEXT("A self-owning row produces one root"), SelfCyclicLayout.RootLayoutIndices.Num(), 1);
    TestEqual(TEXT("A self-owning row is a cycle member"),
        static_cast<int32>(SelfCyclicLayout.Nodes[SelfCyclicLayout.RootLayoutIndices[0]].Kind),
        static_cast<int32>(ECkSaveDebugger_NodeKind::CycleRoot));

    // A row owned by a non-persisted entity groups under one synthetic root per absent owner id — a normal
    // top-level shape (transient-owned roots), not an error styling.
    const auto NonPersistedOwnerDocument = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::EngineOwned, 4242),
            MakeEntity(3, ECk_Snapshot_V3_Provenance::EngineOwned, 4242),
            MakeEntity(4, ECk_Snapshot_V3_Provenance::EngineOwned, 7777),
        },
        {});

    const auto NonPersistedOwnerLayout = ck_save_debugger_model::Build_OwnershipLayout(NonPersistedOwnerDocument);
    TestEqual(TEXT("One group per absent owner id, plus the healthy root"),
        NonPersistedOwnerLayout.RootLayoutIndices.Num(), 3);

    const auto& FirstGroup = NonPersistedOwnerLayout.Nodes[NonPersistedOwnerLayout.RootLayoutIndices[0]];
    const auto& SecondGroup = NonPersistedOwnerLayout.Nodes[NonPersistedOwnerLayout.RootLayoutIndices[1]];
    TestEqual(TEXT("The groups read first"),
        static_cast<int32>(FirstGroup.Kind), static_cast<int32>(ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot));
    TestEqual(TEXT("Groups sort by absent owner id"),
        static_cast<int64>(FirstGroup.SavedId), static_cast<int64>(4242));
    TestEqual(TEXT("Both rows naming owner 4242 share one group"), FirstGroup.ChildLayoutIndices.Num(), 2);
    TestEqual(TEXT("The second group carries the other absent owner id"),
        static_cast<int64>(SecondGroup.SavedId), static_cast<int64>(7777));
    TestEqual(TEXT("The lone row naming owner 7777 hangs off it"), SecondGroup.ChildLayoutIndices.Num(), 1);

    // A healthy save produces no synthetic roots at all.
    const auto HealthyDocument = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, 1),
        },
        {});

    const auto HealthyLayout = ck_save_debugger_model::Build_OwnershipLayout(HealthyDocument);
    TestEqual(TEXT("A healthy save has one root"), HealthyLayout.RootLayoutIndices.Num(), 1);
    TestEqual(TEXT("The healthy root is a real entity row"),
        static_cast<int32>(HealthyLayout.Nodes[HealthyLayout.RootLayoutIndices[0]].Kind),
        static_cast<int32>(ECkSaveDebugger_NodeKind::Entity));

    // And the model materializes the same shape without hanging on the cyclic fixture.
    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(CyclicDocument);
    TestEqual(TEXT("The model renders the cyclic save as one synthetic root"), Model.Get_TreeRoots().Num(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_RowPresentation,
    "Ck.SaveDebugger.Model.RowPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_RowPresentation::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model;
    using namespace ck_save_debugger_model_spec;

    const auto ProvenanceIcons = TSet<FName>
    {
        Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::EngineOwned),
        Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::ConstructSpawned),
        Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::RuntimeSpawned),
        Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::DefinitionBuilt),
    };

    // A shared glyph turns the tree's provenance column into decoration, so the four must stay distinct.
    TestEqual(TEXT("Every provenance has its own glyph"), ProvenanceIcons.Num(), 4);

    TestTrue(TEXT("A synthetic root does not borrow a provenance glyph"),
        NOT ProvenanceIcons.Contains(Get_NodeKindIconId(ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot)) &&
        NOT ProvenanceIcons.Contains(Get_NodeKindIconId(ECkSaveDebugger_NodeKind::CycleRoot)));

    TestTrue(TEXT("The two synthetic roots are told apart by glyph"),
        Get_NodeKindIconId(ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot) !=
        Get_NodeKindIconId(ECkSaveDebugger_NodeKind::CycleRoot));

    // The reclassification that matters: a transient-owned group is normal shape, only a cycle is a defect.
    TestEqual(TEXT("A Non-Persisted Owner group renders neutral"),
        static_cast<int32>(Get_NodeKindTone(ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot)),
        static_cast<int32>(ECk_Tone::Neutral));
    TestEqual(TEXT("An entity row renders neutral"),
        static_cast<int32>(Get_NodeKindTone(ECkSaveDebugger_NodeKind::Entity)),
        static_cast<int32>(ECk_Tone::Neutral));
    TestEqual(TEXT("An owner cycle renders in the error tone"),
        static_cast<int32>(Get_NodeKindTone(ECkSaveDebugger_NodeKind::CycleRoot)),
        static_cast<int32>(ECk_Tone::Err));

    // And the rows the model materializes carry the glyph, so the window never re-derives it from the document.
    const auto Document = MakeDocument(
        {
            MakeEntity(1, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::RuntimeSpawned, 1),
        },
        {});

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    TestEqual(TEXT("The materialized save has one root"), Model.Get_TreeRoots().Num(), 1);

    const auto& Root = Model.Get_TreeRoots()[0];
    TestEqual(TEXT("The root row carries its provenance glyph"),
        Root->IconId.ToString(), Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::EngineOwned).ToString());
    TestEqual(TEXT("The child row carries its own provenance glyph"),
        Root->Children[0]->IconId.ToString(),
        Get_ProvenanceIconId(ECk_Snapshot_V3_Provenance::RuntimeSpawned).ToString());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Model_HexPreview,
    "Ck.SaveDebugger.Model.HexPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Model_HexPreview::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model;

    TestEqual(TEXT("An empty blob says so"), Format_HexPreview({}, k_MaxHexPreviewBytes), FString{TEXT("(empty)")});

    const auto Short = Format_HexPreview(TArray<uint8>{0xde, 0xad, 0xbe}, k_MaxHexPreviewBytes);
    TestEqual(TEXT("A short blob is one offset-prefixed line"), Short, FString{TEXT("0000: de ad be")});

    auto Long = TArray<uint8>{};
    Long.SetNumZeroed(300);

    const auto Preview = Format_HexPreview(Long, k_MaxHexPreviewBytes);
    TestTrue(TEXT("An over-long blob states what was cut"), Preview.Contains(TEXT("more byte(s) not shown")));
    TestTrue(TEXT("The cut count is exact"), Preview.Contains(TEXT("44 more byte(s)")));
    TestTrue(TEXT("The preview stops at the last full line offset"), Preview.Contains(TEXT("00f0:")));
    TestFalse(TEXT("The preview never reaches past the limit"), Preview.Contains(TEXT("0100:")));

    // The formatter is pure — same input, same output, every time.
    TestEqual(TEXT("Formatting is deterministic"), Format_HexPreview(Long, k_MaxHexPreviewBytes), Preview);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Export_JsonDeterminism,
    "Ck.SaveDebugger.Export.JsonDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Export_JsonDeterminism::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Document = MakeDocument(
        {
            MakeEntity(3, ECk_Snapshot_V3_Provenance::EngineOwned, ck::snapshot::k_NoSavedEntity),
            MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, 3),
            MakeEntity(2, ECk_Snapshot_V3_Provenance::RuntimeSpawned, 3),
        },
        {
            MakePayload(3, TEXT("/Script/Spec.Spec_RepData_B"), TArray<uint8>{9, 9, 9}),
            MakePayload(1, TEXT("/Script/Spec.Spec_RepData_A"), TArray<uint8>{1}),
        });

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Document);

    const auto First = ck_save_debugger_model::Build_JsonExport(Model);
    const auto Second = ck_save_debugger_model::Build_JsonExport(Model);

    TestTrue(TEXT("The export is non-empty"), First.Len() > 0);
    TestEqual(TEXT("Two exports of the same model are byte-identical"), First, Second);

    TestTrue(TEXT("The file hash is always present"), First.Contains(TEXT("\"sha256\"")));
    TestTrue(TEXT("The census is always present"), First.Contains(TEXT("\"census\"")));
    TestTrue(TEXT("Diagnostics are always present"), First.Contains(TEXT("\"diagnostics\"")));
    TestTrue(TEXT("Blobs appear as bounded previews"), First.Contains(TEXT("\"preview\"")));

    // A model built independently from the same bytes exports identically — nothing in the document carries a
    // pointer, a timestamp of its own, or an iteration-order artefact.
    auto Twin = FCkSaveDebugger_Model{};
    Twin.Set_Document(Document);
    TestEqual(TEXT("An independently built model exports identically"),
        ck_save_debugger_model::Build_JsonExport(Twin), First);

    const auto Report = ck_save_debugger_model::Build_TextReport(Model);
    TestTrue(TEXT("The text report is non-empty"), Report.Len() > 0);
    TestEqual(TEXT("The text report is deterministic"), ck_save_debugger_model::Build_TextReport(Model), Report);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_model_spec
{
    // Baseline vs current pair exercising all four moving group kinds at once: Alpha keeps its count but changes
    // payload bytes, Beta grows, Gamma appears, Delta disappears.
    auto
        MakeDiffBaselineDocument()
        -> FCk_SnapshotInspection_Document
    {
        return MakeDocument(
            {
                MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Alpha")),
                MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Beta")),
                MakeEntity(5, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Delta")),
            },
            {
                MakePayload(1, TEXT("/Script/Spec.Spec_RepData_A"), TArray<uint8>{1}),
            });
    }

    auto
        MakeDiffCurrentDocument()
        -> FCk_SnapshotInspection_Document
    {
        return MakeDocument(
            {
                MakeEntity(1, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Alpha")),
                MakeEntity(2, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Beta")),
                MakeEntity(3, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Beta")),
                MakeEntity(4, ECk_Snapshot_V3_Provenance::ConstructSpawned, ck::snapshot::k_NoSavedEntity, TEXT("Gamma")),
            },
            {
                MakePayload(1, TEXT("/Script/Spec.Spec_RepData_A"), TArray<uint8>{1, 2}),
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Diff_DiffReportDeterminism,
    "Ck.SaveDebugger.Diff.DiffReportDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Diff_DiffReportDeterminism::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Baseline = MakeDiffBaselineDocument();
    const auto Current = MakeDiffCurrentDocument();

    const auto Diff = ck::snapshot::Diff_Documents(Baseline, Current);
    TestTrue(TEXT("Two readable fixtures produce a valid diff"), Diff.Get_Valid());
    TestTrue(TEXT("The diff found groups"), Diff.Get_EntityGroups().Num() > 0);

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Current);
    Model.Set_Diff(Diff, TEXT("baseline fixture"));

    TestTrue(TEXT("The model reports it holds a diff"), Model.Get_HasDiff());

    const auto First = ck_save_debugger_model::Build_DiffReportText(Model);
    const auto Second = ck_save_debugger_model::Build_DiffReportText(Model);

    TestTrue(TEXT("The diff report is non-empty"), First.Len() > 0);
    TestEqual(TEXT("Two reports off the same model are byte-identical"), First, Second);

    // A model rebuilt from scratch off an independently computed diff reports identically — nothing in the report
    // is time-, pointer- or iteration-order-derived.
    const auto DiffAgain = ck::snapshot::Diff_Documents(Baseline, Current);

    auto Twin = FCkSaveDebugger_Model{};
    Twin.Set_Document(MakeDiffCurrentDocument());
    Twin.Set_Diff(DiffAgain, TEXT("baseline fixture"));

    TestEqual(TEXT("An independently rebuilt model reports identically"),
        ck_save_debugger_model::Build_DiffReportText(Twin), First);

    // Unchanged groups are the diff's to document and the UI's to hide — the toggle is the only thing that decides.
    const auto MovedRows = Model.Build_DiffGroupRows();

    Model.Set_DiffShowUnchanged(true);
    const auto AllRows = Model.Build_DiffGroupRows();

    TestEqual(TEXT("Showing unchanged groups lists every group the diff carries"),
        AllRows.Num(), Diff.Get_EntityGroups().Num());
    TestEqual(TEXT("Hiding them drops exactly the Unchanged ones"),
        MovedRows.Num(),
        AllRows.Num() - Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::Unchanged));

    for (const auto& Row : MovedRows)
    {
        TestTrue(TEXT("No Unchanged group survives the default filter"),
            Row.Kind != ECk_SnapshotInspection_DiffGroupKind::Unchanged);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Diff_DiffLifecycle,
    "Ck.SaveDebugger.Diff.DiffLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Diff_DiffLifecycle::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model_spec;

    const auto Baseline = MakeDiffBaselineDocument();
    const auto Current = MakeDiffCurrentDocument();

    auto Model = FCkSaveDebugger_Model{};
    Model.Set_Document(Current);
    Model.Set_Diff(ck::snapshot::Diff_Documents(Baseline, Current), TEXT("baseline fixture"));

    TestTrue(TEXT("The diff is live"), Model.Get_HasDiff());
    TestTrue(TEXT("The baseline description is retained"),
        Model.Get_DiffBaselineSourceDescription() == TEXT("baseline fixture"));

    // Replacing the document — a reload counts — leaves a diff describing a CURRENT side that is no longer on
    // screen, so the model drops it rather than showing a comparison against nothing.
    Model.Set_Document(Current);

    TestFalse(TEXT("Replacing the document drops the diff"), Model.Get_HasDiff());
    TestFalse(TEXT("The dropped diff is not left valid"), Model.Get_Diff().Get_Valid());
    TestTrue(TEXT("The baseline description goes with it"),
        Model.Get_DiffBaselineSourceDescription().IsEmpty());
    TestEqual(TEXT("A dropped diff lists no groups"), Model.Build_DiffGroupRows().Num(), 0);

    Model.Set_Diff(ck::snapshot::Diff_Documents(Baseline, Current), TEXT("baseline fixture"));
    Model.Reset();

    TestFalse(TEXT("Reset drops the diff too"), Model.Get_HasDiff());

    Model.Set_Document(Current);
    Model.Set_Diff(ck::snapshot::Diff_Documents(Baseline, Current), TEXT("baseline fixture"));
    Model.Clear_Diff();

    TestFalse(TEXT("Closing the diff explicitly drops it"), Model.Get_HasDiff());

    // An invalid diff still reaches the window — it carries the reason the panel prints — but it lists nothing.
    auto WithInvalid = FCkSaveDebugger_Model{};
    WithInvalid.Set_Document(Current);
    WithInvalid.Set_Diff(FCk_SnapshotInspection_Diff{}, TEXT("unreadable fixture"));

    TestTrue(TEXT("An invalid diff is still HELD"), WithInvalid.Get_HasDiff());
    TestFalse(TEXT("An invalid diff is not valid"), WithInvalid.Get_Diff().Get_Valid());
    TestEqual(TEXT("An invalid diff lists no groups"), WithInvalid.Build_DiffGroupRows().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSaveDebugger_Diff_KindTones,
    "Ck.SaveDebugger.Diff.KindTones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkSaveDebugger_Diff_KindTones::RunTest(const FString& Parameters)
{
    using namespace ck_save_debugger_model;

    // Growth is the suspect direction — a leak reads as a population that only ever went up.
    TestEqual(TEXT("Added is Warn"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::Added, 3)),
        static_cast<int32>(ECk_Tone::Warn));
    TestEqual(TEXT("Removed is Info"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::Removed, -3)),
        static_cast<int32>(ECk_Tone::Info));
    TestEqual(TEXT("A grown count is Warn"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::CountChanged, 77)),
        static_cast<int32>(ECk_Tone::Warn));
    TestEqual(TEXT("A shrunk count is Info"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::CountChanged, -1)),
        static_cast<int32>(ECk_Tone::Info));
    TestEqual(TEXT("PayloadsChanged is Accent"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::PayloadsChanged, 0)),
        static_cast<int32>(ECk_Tone::Accent));
    TestEqual(TEXT("Unchanged is Neutral"),
        static_cast<int32>(Get_DiffGroupTone(ECk_SnapshotInspection_DiffGroupKind::Unchanged, 0)),
        static_cast<int32>(ECk_Tone::Neutral));

    TestEqual(TEXT("A positive census delta is Warn"),
        static_cast<int32>(Get_DiffDeltaTone(12)), static_cast<int32>(ECk_Tone::Warn));
    TestEqual(TEXT("A negative census delta is Info"),
        static_cast<int32>(Get_DiffDeltaTone(-12)), static_cast<int32>(ECk_Tone::Info));
    TestEqual(TEXT("A flat census delta is Neutral"),
        static_cast<int32>(Get_DiffDeltaTone(0)), static_cast<int32>(ECk_Tone::Neutral));

    // A row that did not move carries no delta at all rather than a "+0" the eye has to discount.
    TestTrue(TEXT("A zero delta renders as nothing"), Build_DiffDeltaText(0).IsEmpty());
    TestEqual(TEXT("Growth is signed"), Build_DiffDeltaText(5), FString{TEXT("+5")});
    TestEqual(TEXT("Shrinkage is signed"), Build_DiffDeltaText(-5), FString{TEXT("-5")});

    TestEqual(TEXT("An unmoved count omits the delta"),
        Build_DiffCountText(3, 3), FString{TEXT("3 -> 3")});
    TestEqual(TEXT("A moved count carries it"),
        Build_DiffCountText(3, 80), FString{TEXT("3 -> 80 (+77)")});
    TestEqual(TEXT("Bytes read the same way"),
        Build_DiffBytesText(10, 4), FString{TEXT("10 -> 4 B (-6)")});

    auto ContentRow = FCk_SnapshotInspection_DiffPayloadTypeRow{};
    ContentRow.Set_CountBaseline(2);
    ContentRow.Set_CountCurrent(2);
    ContentRow.Set_BytesBaseline(64);
    ContentRow.Set_BytesCurrent(64);
    ContentRow.Set_ContentDiffers(true);

    // Same counts, same bytes — the hash verdict is the only thing that says this type moved at all.
    TestTrue(TEXT("A same-size value change is called out"),
        Build_DiffPayloadTypeText(ContentRow).Contains(TEXT("(content differs)")));

    ContentRow.Set_ContentDiffers(false);
    TestFalse(TEXT("An unchanged type is not"),
        Build_DiffPayloadTypeText(ContentRow).Contains(TEXT("(content differs)")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
