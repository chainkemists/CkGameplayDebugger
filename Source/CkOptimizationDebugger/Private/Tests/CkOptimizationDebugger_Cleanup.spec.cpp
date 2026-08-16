#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_CleanupScan.h"
#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_CleanupCommands.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup_spec
{
    /** A duplicate row built through the REAL group-key projection, so what the spec groups by is the same string the
     *  scan would have written. A fixture that hand-wrote the key would pass while the projection was broken. */
    auto
        MakeDuplicateRow(
            const FString& InName,
            const FString& InFolder,
            const FString& InClassName,
            int64 InBytes)
        -> FCkOptimizationDebugger_CleanupRow
    {
        auto Row = FCkOptimizationDebugger_CleanupRow{};

        Row.Category = ECkOptimizationDebugger_CleanupCategory::Duplicates;
        Row.AssetPath = ck::Format_UE(TEXT("/Game/{}/{}.{}"), InFolder, InName, InName);
        Row.DisplayName = InName;
        Row.ClassName = InClassName;
        Row.DiskSizeBytes = InBytes;
        Row.GroupKey =
            ck_optimization_debugger_model::Build_DuplicateGroupKey(InName, InClassName, InBytes);
        Row.Detail = FString{TEXT("Same name, class and size as a sibling")};

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeRow(
            ECkOptimizationDebugger_CleanupCategory InCategory,
            const FString& InName,
            const FString& InClassName,
            int64 InBytes,
            const FString& InDetail)
        -> FCkOptimizationDebugger_CleanupRow
    {
        auto Row = FCkOptimizationDebugger_CleanupRow{};

        Row.Category = InCategory;
        Row.AssetPath = ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InName, InName);
        Row.DisplayName = InName;
        Row.ClassName = InClassName;
        Row.DiskSizeBytes = InBytes;
        Row.Detail = InDetail;

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Two duplicate groups of different weights, one lone would-be duplicate, plus one row of every other category.
     *
     *  `Rock` appears three times at 2,048 bytes; `Brick` twice at 4,096; `Rock` once more at a DIFFERENT size, which
     *  must not join the first group — the size is a third of the match and dropping it would be the tool claiming
     *  two different assets are copies because they share a name. */
    auto
        MakeFixture()
        -> TArray<FCkOptimizationDebugger_CleanupRow>
    {
        auto Rows = TArray<FCkOptimizationDebugger_CleanupRow>{};

        Rows.Add(MakeDuplicateRow(TEXT("Rock"), TEXT("Env"), TEXT("StaticMesh"), 2048));
        Rows.Add(MakeDuplicateRow(TEXT("Rock"), TEXT("Props"), TEXT("StaticMesh"), 2048));
        Rows.Add(MakeDuplicateRow(TEXT("Rock"), TEXT("Legacy"), TEXT("StaticMesh"), 2048));

        Rows.Add(MakeDuplicateRow(TEXT("Brick"), TEXT("Env"), TEXT("Texture2D"), 4096));
        Rows.Add(MakeDuplicateRow(TEXT("Brick"), TEXT("Props"), TEXT("Texture2D"), 4096));

        // Same name and class as the first group, different size — a group of one, which is not a group.
        Rows.Add(MakeDuplicateRow(TEXT("Rock"), TEXT("Hero"), TEXT("StaticMesh"), 999));

        Rows.Add(MakeRow(ECkOptimizationDebugger_CleanupCategory::Unreferenced,
            TEXT("OrphanMesh"), TEXT("StaticMesh"), 8192, TEXT("No package references it")));
        Rows.Add(MakeRow(ECkOptimizationDebugger_CleanupCategory::Unreferenced,
            TEXT("OrphanTex"), TEXT("Texture2D"), 1024, TEXT("No package references it")));

        Rows.Add(MakeRow(ECkOptimizationDebugger_CleanupCategory::Redirectors,
            TEXT("OldName"), TEXT("ObjectRedirector"), 64, TEXT("2 package(s) still point at it")));

        Rows.Add(MakeRow(ECkOptimizationDebugger_CleanupCategory::DirtyPackages,
            TEXT("WorkInProgress"), TEXT("Package"), 0, TEXT("Never saved — nothing on disk yet")));

        return Rows;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Cleanup_DuplicateGrouping,
    "Ck.OptimizationDebugger.Cleanup.DuplicateGrouping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Cleanup_DuplicateGrouping::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_cleanup_spec;
    using namespace ck_optimization_debugger_model;

    // ---- The key is name + class + size, and nothing else ----
    TestEqual(TEXT("Two assets with the same name, class and size share a key"),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048));

    TestNotEqual(TEXT("A different SIZE is a different key — the size is a third of the match"),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2049));

    TestNotEqual(TEXT("A different CLASS is a different key"),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("Texture2D"), 2048));

    TestNotEqual(TEXT("A different NAME is a different key"),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048),
        Build_DuplicateGroupKey(TEXT("Stone"), TEXT("StaticMesh"), 2048));

    TestEqual(TEXT("Case is not part of the match — the Content Browser does not distinguish it either"),
        Build_DuplicateGroupKey(TEXT("Rock"), TEXT("StaticMesh"), 2048),
        Build_DuplicateGroupKey(TEXT("ROCK"), TEXT("staticmesh"), 2048));

    // ---- Grouping ----
    const auto Rows = MakeFixture();
    const auto Groups = Get_CleanupGroups(Rows,
        ECkOptimizationDebugger_CleanupCategory::Duplicates, FString{});

    TestEqual(TEXT("Two groups, and the lone same-name-different-size row is not one of them"), Groups.Num(), 2);

    if (Groups.Num() != 2)
    { return false; }

    // Rock: 3 copies at 2,048 → keeping one frees 4,096. Brick: 2 copies at 4,096 → keeping one frees 4,096. Equal
    // weights, so the group KEY breaks the tie — and it does so deterministically, which is the point.
    TestEqual(TEXT("Every member of a group is present"), Groups[0].Rows.Num() + Groups[1].Rows.Num(), 5);

    const auto FindGroup = [&Groups](const FString& InName) -> const FCkOptimizationDebugger_CleanupGroup*
    {
        for (const auto& Group : Groups)
        {
            if (Group.DisplayName.Equals(InName, ESearchCase::CaseSensitive))
            { return &Group; }
        }

        return nullptr;
    };

    const auto* RockGroup = FindGroup(TEXT("Rock"));
    const auto* BrickGroup = FindGroup(TEXT("Brick"));

    TestNotNull(TEXT("The three-copy group is there"), RockGroup);
    TestNotNull(TEXT("The two-copy group is there"), BrickGroup);

    if (RockGroup == nullptr || BrickGroup == nullptr)
    { return false; }

    TestEqual(TEXT("The three same-sized Rocks group together"), RockGroup->Rows.Num(), 3);
    TestEqual(TEXT("...and the odd-sized one is not among them"), static_cast<int32>(RockGroup->DiskSizeBytes), 2048);
    TestEqual(TEXT("The two Bricks group together"), BrickGroup->Rows.Num(), 2);

    // ---- Determinism ----
    const auto Again = Get_CleanupGroups(Rows,
        ECkOptimizationDebugger_CleanupCategory::Duplicates, FString{});

    TestEqual(TEXT("Two calls produce the same number of groups"), Again.Num(), Groups.Num());

    for (auto Index = 0; Index < Groups.Num(); ++Index)
    {
        TestEqual(ck::Format_UE(TEXT("Group {} is in the same place both times"), Index),
            Again[Index].GroupKey, Groups[Index].GroupKey);
    }

    // A group's members are path-ordered, so the sibling list one member prints is the list every other member does.
    for (const auto& Group : Groups)
    {
        for (auto Index = 1; Index < Group.Rows.Num(); ++Index)
        {
            TestTrue(TEXT("Members of a group are ordered by path"),
                Group.Rows[Index - 1].AssetPath.Compare(Group.Rows[Index].AssetPath, ESearchCase::CaseSensitive) < 0);
        }
    }

    // ---- A filter that narrows a pair to one drops the group entirely ----
    const auto Narrowed = Get_CleanupGroups(Rows,
        ECkOptimizationDebugger_CleanupCategory::Duplicates, FString{TEXT("Env/Brick")});

    TestEqual(TEXT("One asset is not a duplicate of anything — a filtered-down group is dropped, not shown alone"),
        Narrowed.Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Cleanup_ExclusionList,
    "Ck.OptimizationDebugger.Cleanup.ExclusionList",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Cleanup_ExclusionList::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_cleanup;

    const auto Excluded = Get_AlwaysRootedClassNames();

    TestFalse(TEXT("The exclusion list is not empty"), Excluded.IsEmpty());

    // The four the on-disk reference graph structurally cannot see a reference to. A regression that dropped any of
    // them would make the unreferenced tab ask the reader to delete every level in the project.
    TestTrue(TEXT("A world is never reported as unreferenced"), Is_AlwaysRootedClassName(TEXT("World")));
    TestTrue(TEXT("A level is never reported as unreferenced"), Is_AlwaysRootedClassName(TEXT("Level")));
    TestTrue(TEXT("A data asset is never reported as unreferenced"), Is_AlwaysRootedClassName(TEXT("DataAsset")));
    TestTrue(TEXT("A primary data asset is never reported as unreferenced"),
        Is_AlwaysRootedClassName(TEXT("PrimaryDataAsset")));

    TestTrue(TEXT("Matching ignores case — a class name is not a case-sensitive fact here"),
        Is_AlwaysRootedClassName(TEXT("world")));

    // The list is deliberately SHORT. Anything the reference graph can see is fair game, or the tab reports nothing.
    TestFalse(TEXT("An ordinary mesh is fair game"), Is_AlwaysRootedClassName(TEXT("StaticMesh")));
    TestFalse(TEXT("An ordinary texture is fair game"), Is_AlwaysRootedClassName(TEXT("Texture2D")));
    TestFalse(TEXT("A material is fair game"), Is_AlwaysRootedClassName(TEXT("MaterialInstanceConstant")));
    TestFalse(TEXT("A blueprint is fair game"), Is_AlwaysRootedClassName(TEXT("Blueprint")));
    TestFalse(TEXT("An empty class name matches nothing"), Is_AlwaysRootedClassName(FString{}));

    // Stable order, so two readers comparing the list are comparing the same list.
    const auto Again = Get_AlwaysRootedClassNames();

    TestEqual(TEXT("The list is the same length both times"), Again.Num(), Excluded.Num());

    for (auto Index = 0; Index < Excluded.Num(); ++Index)
    { TestEqual(TEXT("...and in the same order"), Again[Index], Excluded[Index]); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Cleanup_Totals,
    "Ck.OptimizationDebugger.Cleanup.Totals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Cleanup_Totals::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_cleanup_spec;
    using namespace ck_optimization_debugger_model;

    // ---- An empty census still describes all four categories ----
    const auto Empty = Get_CleanupTotals(TArray<FCkOptimizationDebugger_CleanupRow>{});

    TestEqual(TEXT("Every category has an entry even with nothing in it"),
        Empty.Categories.Num(), k_CleanupCategoryCount);
    TestEqual(TEXT("An empty census counts nothing"), Empty.RowCount, 0);
    TestEqual(TEXT("...and reclaims nothing"), static_cast<int32>(Empty.ReclaimableBytes), 0);

    for (const auto Category : Get_AllCleanupCategories())
    {
        const auto Index = static_cast<int32>(Category);

        TestEqual(TEXT("Category entries are in declaration order"),
            static_cast<int32>(Empty.Categories[Index].Category), Index);
    }

    // ---- The fixture ----
    const auto Rows = MakeFixture();
    const auto Totals = Get_CleanupTotals(Rows);

    TestEqual(TEXT("Every row is counted once"), Totals.RowCount, Rows.Num());

    const auto CountOf = [&Totals](ECkOptimizationDebugger_CleanupCategory InCategory) -> int32
    {
        return Totals.Categories[static_cast<int32>(InCategory)].RowCount;
    };

    TestEqual(TEXT("Two unreferenced"),
        CountOf(ECkOptimizationDebugger_CleanupCategory::Unreferenced), 2);
    TestEqual(TEXT("Six duplicate rows — including the lone one the GROUPING drops"),
        CountOf(ECkOptimizationDebugger_CleanupCategory::Duplicates), 6);
    TestEqual(TEXT("One redirector"),
        CountOf(ECkOptimizationDebugger_CleanupCategory::Redirectors), 1);
    TestEqual(TEXT("One dirty package"),
        CountOf(ECkOptimizationDebugger_CleanupCategory::DirtyPackages), 1);

    // 8,192 + 1,024. Deliberately NOT the sum of every category: a duplicate's bytes are reclaimable only once
    // somebody decides which copy is redundant, and a never-saved package is not on disk at all.
    TestEqual(TEXT("Reclaimable is the unreferenced bytes and only those"),
        static_cast<int32>(Totals.ReclaimableBytes), 9216);

    TestEqual(TEXT("A category's byte total is still its own"),
        static_cast<int32>(Totals.Categories[static_cast<int32>(
            ECkOptimizationDebugger_CleanupCategory::Redirectors)].TotalBytes), 64);

    // ---- Through the model ----
    auto Model = FCkOptimizationDebugger_Model{};

    TestFalse(TEXT("Nothing is scanned to begin with"), Model.Get_HasCleanupScan());

    Model.Set_CleanupRows(Rows, FDateTime{2026, 8, 11, 12, 0, 0});

    TestTrue(TEXT("A pass ran"), Model.Get_HasCleanupScan());
    TestEqual(TEXT("The model's totals are the projection's"), Model.Get_CleanupTotals().RowCount, Rows.Num());
    TestEqual(TEXT("The scan time is the one handed in, never the clock's"),
        Model.Get_LastCleanupScanTime().GetYear(), 2026);

    // ---- A play session does not make an unreferenced asset referenced ----
    Model.Reset();

    TestTrue(TEXT("The cleanup census SURVIVES a session boundary — it is asset-scoped, not world-scoped"),
        Model.Get_HasCleanupScan());
    TestEqual(TEXT("...with every row still in it"), Model.Get_CleanupTotals().RowCount, Rows.Num());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Cleanup_FilterNarrowing,
    "Ck.OptimizationDebugger.Cleanup.FilterNarrowing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Cleanup_FilterNarrowing::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_cleanup_spec;
    using namespace ck_optimization_debugger_model;

    const auto Rows = MakeFixture();

    // ---- Category identity round-trips ----
    for (const auto Category : Get_AllCleanupCategories())
    {
        const auto RoundTripped = TryGet_CleanupCategoryFromId(Get_CleanupCategoryId(Category));

        TestTrue(TEXT("Every category id resolves back to its category"), RoundTripped.IsSet());

        if (RoundTripped.IsSet())
        {
            TestEqual(TEXT("...to the SAME category"),
                static_cast<int32>(RoundTripped.GetValue()), static_cast<int32>(Category));
        }

        TestFalse(TEXT("Every category has a label"), Get_CleanupCategoryLabel(Category).IsEmpty());
        TestFalse(TEXT("Every category explains itself"), Get_CleanupCategoryHint(Category).IsEmpty());
    }

    TestFalse(TEXT("An unknown id resolves to nothing"),
        TryGet_CleanupCategoryFromId(FName{TEXT("Cleanup.Nonsense")}).IsSet());

    // The conservative claim, where the reader sees it. A label or hint that promised byte-identity would be this
    // tool asserting something its match cannot support.
    TestTrue(TEXT("The duplicates tab says POSSIBLE"),
        Get_CleanupCategoryLabel(ECkOptimizationDebugger_CleanupCategory::Duplicates)
            .Contains(TEXT("Possible"), ESearchCase::IgnoreCase));
    TestTrue(TEXT("...and the hint under it says what the match actually is"),
        Get_CleanupCategoryHint(ECkOptimizationDebugger_CleanupCategory::Duplicates)
            .Contains(TEXT("byte-identical"), ESearchCase::IgnoreCase));

    // ---- The filter narrows the view ----
    const auto AllUnreferenced =
        Get_SortedCleanupRows(Rows, ECkOptimizationDebugger_CleanupCategory::Unreferenced, FString{});

    TestEqual(TEXT("No filter admits everything in the category"), AllUnreferenced.Num(), 2);

    // Biggest first, because "what would this free" is the question the tab exists to answer.
    TestEqual(TEXT("The heaviest row is first"), AllUnreferenced[0].DisplayName, FString{TEXT("OrphanMesh")});

    const auto ByName = Get_SortedCleanupRows(Rows,
        ECkOptimizationDebugger_CleanupCategory::Unreferenced, FString{TEXT("OrphanTex")});

    TestEqual(TEXT("A name narrows it"), ByName.Num(), 1);

    const auto ByClass = Get_SortedCleanupRows(Rows,
        ECkOptimizationDebugger_CleanupCategory::Duplicates, FString{TEXT("Texture2D")});

    TestEqual(TEXT("The class is part of the haystack"), ByClass.Num(), 2);

    const auto ByDetail = Get_SortedCleanupRows(Rows,
        ECkOptimizationDebugger_CleanupCategory::Redirectors, FString{TEXT("still point at it")});

    TestEqual(TEXT("So is the reason the row is there"), ByDetail.Num(), 1);

    TestEqual(TEXT("A filter that matches nothing empties the list rather than falling back to everything"),
        Get_SortedCleanupRows(Rows,
            ECkOptimizationDebugger_CleanupCategory::Unreferenced, FString{TEXT("zzz-nothing")}).Num(), 0);

    // A filter never leaks across categories — a query that names a duplicate must not pull it into the
    // unreferenced tab.
    TestEqual(TEXT("The category gate comes first"),
        Get_SortedCleanupRows(Rows,
            ECkOptimizationDebugger_CleanupCategory::Unreferenced, FString{TEXT("Rock")}).Num(), 0);

    // ---- Totals ignore the filter, the sub-tab counts do not ----
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_CleanupRows(Rows, FDateTime{});
    Model.Set_CleanupFilterString(FString{TEXT("Orphan")});

    TestEqual(TEXT("The filter narrows the view"),
        Model.Get_VisibleCleanupRowCount(ECkOptimizationDebugger_CleanupCategory::Unreferenced), 2);
    TestEqual(TEXT("...and empties a category it does not name"),
        Model.Get_VisibleCleanupRowCount(ECkOptimizationDebugger_CleanupCategory::Redirectors), 0);
    TestEqual(TEXT("...but the totals ignore it — the header is the census, not the view"),
        Model.Get_CleanupTotals().RowCount, Rows.Num());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Cleanup_ActionCatalog,
    "Ck.OptimizationDebugger.Cleanup.ActionCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Cleanup_ActionCatalog::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_cleanup_spec;
    using namespace ck_optimization_debugger_cleanup_commands;
    using namespace ck_optimization_debugger_model;

    // **No spec runs an action.** Every one of them mutates real assets, real packages or a real ini file, and a spec
    // that ran one would be a spec that changes the branch it runs on. The catalog and the selection rules are the
    // testable parts, and they are pure.
    const auto& Actions = Get_AllActions();

    TestEqual(TEXT("Three actions, one per acting category"), Actions.Num(), 3);

    auto SeenIds = TSet<FName>{};

    for (const auto& Action : Actions)
    {
        TestFalse(TEXT("Every action has an id"), Action.Id.IsNone());
        TestFalse(TEXT("Every action has a verb"), Action.DisplayVerb.IsEmpty());
        TestFalse(TEXT("Every action says what it does"), Action.Description.IsEmpty());
        TestFalse(TEXT("Every action claims at least one category"), Action.Categories.IsEmpty());

        TestFalse(TEXT("Action ids are unique"), SeenIds.Contains(Action.Id));
        SeenIds.Add(Action.Id);

        TestNotNull(TEXT("Every action is reachable by id"), TryGet_ActionById(Action.Id));
        TestNotNull(TEXT("...and by enum"), TryGet_Action(Action.Action));
    }

    // AT MOST one action per category, which is what lets the page show ONE button rather than three that disable.
    // Not "exactly one": `NameCollisions` deliberately has none, because resolving a collision means renaming an
    // asset and no batch action should make that content decision for the reader. Asserting it explicitly below is
    // what keeps "deliberately actionless" distinguishable from "somebody forgot to add the action".
    for (const auto Category : Get_AllCleanupCategories())
    {
        auto ClaimCount = 0;

        for (const auto& Action : Actions)
        {
            if (Accepts_Category(Action, Category))
            { ++ClaimCount; }
        }

        const auto IsActionless =
            Category == ECkOptimizationDebugger_CleanupCategory::NameCollisions;

        TestEqual(TEXT("At most one action claims each category"), ClaimCount, IsActionless ? 0 : 1);

        if (IsActionless)
        {
            TestNull(TEXT("The name-collision category has no action, on purpose"),
                TryGet_ActionForCategory(Category));
        }
        else
        {
            TestNotNull(TEXT("...and the lookup finds it"), TryGet_ActionForCategory(Category));
        }
    }

    const auto* Delete = TryGet_Action(ECkOptimizationDebugger_CleanupAction::DeleteAssets);
    const auto* Redirect = TryGet_Action(ECkOptimizationDebugger_CleanupAction::FixUpRedirectors);
    const auto* Save = TryGet_Action(ECkOptimizationDebugger_CleanupAction::SaveDirtyPackages);

    if (Delete == nullptr || Redirect == nullptr || Save == nullptr)
    { return false; }

    TestTrue(TEXT("Deletion is flagged destructive — nothing else is"), Delete->IsDestructive);
    TestFalse(TEXT("A redirector fix-up is not destructive"), Redirect->IsDestructive);
    TestFalse(TEXT("Saving is not destructive"), Save->IsDestructive);

    TestTrue(TEXT("Deletion accepts unreferenced rows"),
        Accepts_Category(*Delete, ECkOptimizationDebugger_CleanupCategory::Unreferenced));
    TestTrue(TEXT("...and duplicate rows, which is the whole reason that tab has an action"),
        Accepts_Category(*Delete, ECkOptimizationDebugger_CleanupCategory::Duplicates));
    TestFalse(TEXT("...and never a dirty package, which is not a thing to delete"),
        Accepts_Category(*Delete, ECkOptimizationDebugger_CleanupCategory::DirtyPackages));

    // The one action that ignores the selection, because the editor's save dialog is a checklist over every dirty
    // package and there is no supported narrowing of it.
    TestTrue(TEXT("Deletion reads the selection"), Delete->OperatesOnSelection);
    TestTrue(TEXT("So does the redirector fix-up"), Redirect->OperatesOnSelection);
    TestFalse(TEXT("The save action does not — the editor's own dialog is the checklist"),
        Save->OperatesOnSelection);

    // ---- Selection rules ----
    const auto Rows = MakeFixture();

    auto Unreferenced = TArray<FCkOptimizationDebugger_CleanupRow>{};
    auto Redirectors = TArray<FCkOptimizationDebugger_CleanupRow>{};

    for (const auto& Row : Rows)
    {
        if (Row.Category == ECkOptimizationDebugger_CleanupCategory::Unreferenced)
        { Unreferenced.Add(Row); }

        if (Row.Category == ECkOptimizationDebugger_CleanupCategory::Redirectors)
        { Redirectors.Add(Row); }
    }

    TestEqual(TEXT("An empty selection is applicable to nothing"),
        Get_ApplicableRows(*Delete, TArray<FCkOptimizationDebugger_CleanupRow>{}).Num(), 0);
    TestFalse(TEXT("...and the button is dead"),
        Can_RunAction(*Delete, TArray<FCkOptimizationDebugger_CleanupRow>{}));
    TestFalse(TEXT("...with a reason that says so"),
        Get_ActionDisabledReason(*Delete, TArray<FCkOptimizationDebugger_CleanupRow>{}).IsEmpty());

    // A mixed selection is NARROWED, never refused: the reader who rubber-banded a list is asking about the rows the
    // button understands.
    TestEqual(TEXT("A whole-fixture selection narrows to the rows deletion understands"),
        Get_ApplicableRows(*Delete, Rows).Num(), 8);
    TestTrue(TEXT("...and the button is live"), Can_RunAction(*Delete, Rows));
    TestTrue(TEXT("...with nothing to explain"), Get_ActionDisabledReason(*Delete, Rows).IsEmpty());

    TestFalse(TEXT("An unreferenced-only selection cannot be fixed up as redirectors"),
        Can_RunAction(*Redirect, Unreferenced));
    TestFalse(TEXT("...and says which kind of row it wanted"),
        Get_ActionDisabledReason(*Redirect, Unreferenced).IsEmpty());

    TestTrue(TEXT("The save action is live with no selection at all"),
        Can_RunAction(*Save, TArray<FCkOptimizationDebugger_CleanupRow>{}));

    // ---- Button labels ----
    TestEqual(TEXT("One row reads as the plain verb"),
        Build_ActionButtonLabel(*Redirect, Redirectors), Redirect->DisplayVerb);
    TestTrue(TEXT("Several rows carry the count"),
        Build_ActionButtonLabel(*Delete, Get_ApplicableRows(*Delete, Rows)).Contains(TEXT("8")));
    TestEqual(TEXT("The selection-blind action never grows a count"),
        Build_ActionButtonLabel(*Save, TArray<FCkOptimizationDebugger_CleanupRow>{}), Save->DisplayVerb);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
