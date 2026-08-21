#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_queue_spec
{
    /** Fixtures go through the real key builder, so the identity the queue and the mute set are keyed by is the same
     *  string the window would reuse a row by — and the same one the sort tie-breaks on. */
    auto
        MakeFindingUnder(
            const FString& InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            ECkOptimizationDebugger_Category InCategory,
            const FString& InFolder,
            const FString& InAssetName)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};
        Finding.CheckId = FName{*InCheckId};
        Finding.Severity = InSeverity;
        Finding.Category = InCategory;
        Finding.Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Finding.Target.Path = FSoftObjectPath{ck::Format_UE(TEXT("{}/{}.{}"), InFolder, InAssetName, InAssetName)};
        Finding.Target.DisplayName = InAssetName;
        Finding.Title = ck::Format_UE(TEXT("Spec finding from {}"), InCheckId);
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(Finding.CheckId, Finding.Target);
        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeFinding(
            const FString& InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            ECkOptimizationDebugger_Category InCategory,
            const FString& InAssetName)
        -> FCkOptimizationDebugger_FindingRow
    {
        return MakeFindingUnder(InCheckId, InSeverity, InCategory, TEXT("/Game/Spec"), InAssetName);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Queue_CollapseIsNotFilter,
    "Ck.OptimizationDebugger.Queue.CollapseIsNotFilter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Queue_CollapseIsNotFilter::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_queue_spec;
    using namespace ck_optimization_debugger_model;

    const auto MeshCheck = FName{TEXT("Mesh.Spec")};
    const auto TextureCheck = FName{TEXT("Texture.Spec")};

    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings(
    {
        MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Critical,
            ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Alpha")),
        MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Bravo")),
        MakeFinding(TEXT("Texture.Spec"), ECkOptimizationDebugger_Severity::Minor,
            ECkOptimizationDebugger_Category::Texture, TEXT("T_Gravel")),
    });

    TestFalse(TEXT("A fresh model collapses nothing"), Model.Get_IsCheckCollapsed(MeshCheck));

    // ---- Collapse is VIEW state, and every count in the window is blind to it ----
    // This is the assertion that stops a future change folding collapse into `Matches_Filter`: collapsing a group
    // folds rows the way a section header does, whereas filtering answers which findings the reader is asking about.
    // Merging the two would make collapsing a group read as findings disappearing.
    Model.Set_CheckCollapsed(MeshCheck, true);

    TestTrue(TEXT("The collapsed check reads as collapsed"), Model.Get_IsCheckCollapsed(MeshCheck));
    TestFalse(TEXT("...and its neighbour does not"), Model.Get_IsCheckCollapsed(TextureCheck));

    TestEqual(TEXT("Collapsing a group hides no finding from the visible LIST"),
        Model.Get_VisibleFindings().Num(), 3);
    TestEqual(TEXT("...nor from the visible COUNT"), Model.Get_VisibleFindingCount(), 3);
    TestEqual(TEXT("...nor from the visible severity counts"),
        Model.Get_VisibleCountsBySeverity().Get_Total(), 3);
    TestEqual(TEXT("...and the collapsed group is still a group"),
        Model.Get_VisibleFindingsGroupedByCheck().Num(), 2);
    TestEqual(TEXT("...still carrying every one of its findings"),
        Model.Get_VisibleFindingsGroupedByCheck()[0].Findings.Num(), 2);

    // The predicate itself has never heard of a check id, which is what makes all of the above true at one site.
    TestTrue(TEXT("A collapsed check's finding still matches the filter"),
        Matches_Filter(Model.Get_Findings()[0], Model.Get_Filter()));

    // ---- Collapse all covers the checks the CURRENT findings hold ----
    Model.Set_CollapsedCheckIds(TSet<FName>{});
    Model.Set_AllChecksCollapsed(true);

    TestEqual(TEXT("Collapse all collapses one entry per distinct check id"),
        Model.Get_CollapsedCheckIds().Num(), 2);
    TestTrue(TEXT("...including the mesh check"), Model.Get_IsCheckCollapsed(MeshCheck));
    TestTrue(TEXT("...and the texture check"), Model.Get_IsCheckCollapsed(TextureCheck));

    // ---- Expand all EMPTIES the set rather than enumerating ids as expanded ----
    // Asserted on the set itself, not merely on what a read returns: the set names what is FOLDED, so a check this
    // scan has never produced comes up expanded. Storing the inverse would make a new check inherit whatever the last
    // Expand all decided about checks it was not part of.
    Model.Set_AllChecksCollapsed(false);

    TestEqual(TEXT("Expand all leaves the collapsed set EMPTY"), Model.Get_CollapsedCheckIds().Num(), 0);
    TestFalse(TEXT("A check the findings never held reads as expanded afterwards"),
        Model.Get_IsCheckCollapsed(FName{TEXT("Lighting.NeverScanned")}));

    // ---- A nameless check names no group ----
    Model.Set_CheckCollapsed(FName{}, true);

    TestFalse(TEXT("A None check id is refused"), Model.Get_IsCheckCollapsed(FName{}));
    TestEqual(TEXT("...leaving the set untouched"), Model.Get_CollapsedCheckIds().Num(), 0);

    // ---- The persisted set is loaded back wholesale, exactly as the muted and exclusion sets are ----
    Model.Set_CollapsedCheckIds(TSet<FName>{MeshCheck});

    TestEqual(TEXT("Loading a persisted set replaces the whole set"), Model.Get_CollapsedCheckIds().Num(), 1);
    TestTrue(TEXT("...with what was handed in"), Model.Get_IsCheckCollapsed(MeshCheck));
    TestEqual(TEXT("...and it still hides nothing from the counts"), Model.Get_VisibleFindingCount(), 3);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Queue_Staging,
    "Ck.OptimizationDebugger.Queue.Staging",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Queue_Staging::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_queue_spec;
    using namespace ck_optimization_debugger_model;

    const auto MinorTexture = MakeFinding(TEXT("Texture.Spec"), ECkOptimizationDebugger_Severity::Minor,
        ECkOptimizationDebugger_Category::Texture, TEXT("T_Gravel"));

    const auto MajorMesh = MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Major,
        ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Bravo"));

    const auto CriticalMesh = MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Critical,
        ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Alpha"));

    // Insertion order is deliberately the reverse of the sorted one, so a projection that walked the input — or the
    // key set — could not read worst-first by accident.
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({MinorTexture, MajorMesh, CriticalMesh});

    TestEqual(TEXT("A fresh model stages nothing"), Model.Get_QueuedFindingCount(), 0);
    TestFalse(TEXT("...and nothing reads as queued"), Model.Get_IsQueued(CriticalMesh.StableKey));

    // ---- Staging is keyed by stable key ----
    Model.Set_Queued(MinorTexture.StableKey, true);
    Model.Set_Queued(CriticalMesh.StableKey, true);

    TestEqual(TEXT("Two findings are staged"), Model.Get_QueuedFindingCount(), 2);
    TestTrue(TEXT("A staged finding reads as queued"), Model.Get_IsQueued(CriticalMesh.StableKey));
    TestFalse(TEXT("...and an unstaged one does not"), Model.Get_IsQueued(MajorMesh.StableKey));

    // ---- The tray reads in the model's SORTED order, never the set's ----
    const auto Queued = Model.Get_QueuedFindings();

    TestEqual(TEXT("Both staged findings come back"), Queued.Num(), 2);
    TestEqual(TEXT("The worst one is first"), Queued[0].StableKey, CriticalMesh.StableKey);
    TestEqual(TEXT("...and the Minor one last, though it was inserted first"),
        Queued[1].StableKey, MinorTexture.StableKey);

    // ---- The queue is independent of the filter, which is the entire reason it exists ----
    // `SListView` selection is transient — clicking any row replaces it — so a reader assembling a batch across two
    // filter changes had nowhere to put it. A queue that emptied when a filter hid a row would be that defect again.
    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Texture, false);

    TestEqual(TEXT("Hiding a category hides its finding from the list"), Model.Get_VisibleFindingCount(), 2);
    TestTrue(TEXT("...and leaves that finding queued"), Model.Get_IsQueued(MinorTexture.StableKey));
    TestEqual(TEXT("...with the queued COUNT unmoved"), Model.Get_QueuedFindingCount(), 2);
    TestEqual(TEXT("...and the tray still holding it"), Model.Get_QueuedFindings().Num(), 2);

    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Texture, true);

    // ---- An empty key names no finding that can ever be built ----
    Model.Set_Queued(FString{}, true);

    TestEqual(TEXT("An empty stable key is refused"), Model.Get_QueuedFindingCount(), 2);

    // ---- A whole group stages at once ----
    Model.Set_QueuedForKeys({MajorMesh.StableKey, CriticalMesh.StableKey}, true);

    TestEqual(TEXT("Staging a set adds the missing one and leaves the present one alone"),
        Model.Get_QueuedFindingCount(), 3);

    Model.Set_QueuedForKeys({MajorMesh.StableKey, CriticalMesh.StableKey}, false);

    TestEqual(TEXT("Unstaging the same set drops exactly those two"), Model.Get_QueuedFindingCount(), 1);
    TestTrue(TEXT("...and leaves the one it did not name"), Model.Get_IsQueued(MinorTexture.StableKey));

    // ---- The tray groups worst-group-first, the same contract the visible grouping holds ----
    Model.Set_QueuedForKeys({MinorTexture.StableKey, MajorMesh.StableKey, CriticalMesh.StableKey}, true);

    const auto Groups = Model.Get_QueuedFindingsGroupedByCheck();

    TestEqual(TEXT("One group per staged check id"), Groups.Num(), 2);
    TestEqual(TEXT("The group holding the worst staged finding comes first"),
        Groups[0].CheckId, FName{TEXT("Mesh.Spec")});
    TestEqual(TEXT("A group reports its WORST member's severity"),
        static_cast<int32>(Groups[0].WorstSeverity),
        static_cast<int32>(ECkOptimizationDebugger_Severity::Critical));
    TestEqual(TEXT("Both mesh findings land in one group"), Groups[0].Findings.Num(), 2);
    TestEqual(TEXT("...and the single-member group follows"), Groups[1].Findings.Num(), 1);

    // ---- Clearing is wholesale ----
    Model.Clear_Queue();

    TestEqual(TEXT("Clearing empties the queue"), Model.Get_QueuedFindingCount(), 0);
    TestEqual(TEXT("...and the tray with it"), Model.Get_QueuedFindings().Num(), 0);
    TestEqual(TEXT("...and its grouping"), Model.Get_QueuedFindingsGroupedByCheck().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Queue_PruneOnRescan,
    "Ck.OptimizationDebugger.Queue.PruneOnRescan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Queue_PruneOnRescan::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_queue_spec;
    using namespace ck_optimization_debugger_model;

    const auto Reproduced = MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Critical,
        ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Alpha"));

    const auto Gone = MakeFinding(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Major,
        ECkOptimizationDebugger_Category::Mesh, TEXT("SM_Bravo"));

    const auto Untouched = MakeFinding(TEXT("Texture.Spec"), ECkOptimizationDebugger_Severity::Minor,
        ECkOptimizationDebugger_Category::Texture, TEXT("T_Gravel"));

    // ---- The pure rule, before the setter that uses it ----
    const auto Kept = Prune_KeysToFindings(
        TSet<FString>{Reproduced.StableKey, Gone.StableKey, TEXT("Never.Scanned|/Game/Nothing")},
        {Reproduced, Untouched});

    TestEqual(TEXT("Pruning keeps only the keys a finding still names"), Kept.Num(), 1);
    TestTrue(TEXT("...which is the reproduced one"), Kept.Contains(Reproduced.StableKey));

    // ---- What a re-scan does to a queue ----
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({Reproduced, Gone, Untouched});

    Model.Set_QueuedForKeys({Reproduced.StableKey, Gone.StableKey}, true);

    auto GoneSuppression = FCkOptimizationDebugger_Suppression{};
    GoneSuppression.Scope = ECkOptimizationDebugger_SuppressionScope::Finding;
    GoneSuppression.Tier = ECkOptimizationDebugger_SuppressionTier::Personal;
    GoneSuppression.Pattern = Gone.StableKey;
    GoneSuppression.Reason = TEXT("Spec reason");

    Model.Add_Suppression(GoneSuppression);

    TestEqual(TEXT("Two findings are staged before the re-scan"), Model.Get_QueuedFindingCount(), 2);
    TestEqual(TEXT("...and one of them is suppressed"), Model.Get_SuppressedFindingCount(), 1);

    // A queue entry naming a finding the project no longer has is a fix the reader cannot inspect and the window
    // cannot apply, so the scan that fails to reproduce it drops it.
    Model.Set_Findings({Reproduced, Untouched});

    TestEqual(TEXT("The unreproduced finding leaves the queue"), Model.Get_QueuedFindingCount(), 1);
    TestFalse(TEXT("...and reads as unqueued"), Model.Get_IsQueued(Gone.StableKey));
    TestTrue(TEXT("The reproduced one stays queued"), Model.Get_IsQueued(Reproduced.StableKey));

    // ---- The SUPPRESSION list is NOT pruned, and the asymmetry is the thing being protected ----
    // A suppression is a standing judgement about a PROBLEM and must survive a scan of another level or another
    // branch that happens not to reproduce it; a queue entry is work staged against a ROW the reader can see.
    // Pruning suppressions here would silently un-suppress every exception in a level nobody opened this session.
    TestEqual(TEXT("A suppression survives a scan that does not reproduce it"),
        Model.Get_Suppressions().Num(), 1);
    TestEqual(TEXT("...naming the same finding"), Model.Get_Suppressions()[0].Pattern, Gone.StableKey);
    TestEqual(TEXT("...while the count of CURRENT suppressed findings honestly drops to none"),
        Model.Get_SuppressedFindingCount(), 0);

    // ---- Reset drops the work and keeps the arrangement ----
    // A PIE boundary invalidates the answers, never the way the reader had arranged them or what they had judged.
    Model.Set_CheckCollapsed(FName{TEXT("Mesh.Spec")}, true);

    Model.Reset();

    TestEqual(TEXT("Reset empties the queue"), Model.Get_QueuedFindingCount(), 0);
    TestEqual(TEXT("Reset KEEPS the collapsed set"), Model.Get_CollapsedCheckIds().Num(), 1);
    TestTrue(TEXT("...naming the same check"), Model.Get_IsCheckCollapsed(FName{TEXT("Mesh.Spec")}));
    TestEqual(TEXT("Reset KEEPS the suppressions"), Model.Get_Suppressions().Num(), 1);
    TestEqual(TEXT("...naming the same finding"), Model.Get_Suppressions()[0].Pattern, Gone.StableKey);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Queue_CategoryCounts,
    "Ck.OptimizationDebugger.Queue.CategoryCounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Queue_CategoryCounts::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_queue_spec;
    using namespace ck_optimization_debugger_model;

    constexpr auto MeshIndex = static_cast<int32>(ECkOptimizationDebugger_Category::Mesh);
    constexpr auto TextureIndex = static_cast<int32>(ECkOptimizationDebugger_Category::Texture);
    constexpr auto LightingIndex = static_cast<int32>(ECkOptimizationDebugger_Category::Lighting);

    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings(
    {
        MakeFindingUnder(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Critical,
            ECkOptimizationDebugger_Category::Mesh, TEXT("/Game/Characters"), TEXT("SM_Hero")),
        MakeFindingUnder(TEXT("Mesh.Spec"), ECkOptimizationDebugger_Severity::Minor,
            ECkOptimizationDebugger_Category::Mesh, TEXT("/Game/Characters"), TEXT("SM_Cape")),
        MakeFindingUnder(TEXT("Texture.Spec"), ECkOptimizationDebugger_Severity::Minor,
            ECkOptimizationDebugger_Category::Texture, TEXT("/Game/Props"), TEXT("T_Gravel")),
    });

    const auto Counts = Model.Get_VisibleCountsByCategory();

    TestEqual(TEXT("There is one entry per category"), Counts.Num(), k_CategoryCount);
    TestEqual(TEXT("...sized from the category list itself"), Counts.Num(), Get_AllCategories().Num());
    TestEqual(TEXT("A category is indexed by its own enum value"), Counts[MeshIndex], 2);
    TestEqual(TEXT("...and so is the other one"), Counts[TextureIndex], 1);
    TestEqual(TEXT("A category with no findings reads zero rather than being absent"), Counts[LightingIndex], 0);

    // ---- The CATEGORY mask is lifted out of this one projection ----
    // A count printed on the control that toggles a category has to say what that control would GIVE the reader, so a
    // category currently toggled off still reports its true count — otherwise the button that turns it back on would
    // be the one claiming there is nothing behind it.
    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, false);

    TestEqual(TEXT("Hiding a category drops its findings from the list"), Model.Get_VisibleFindingCount(), 1);
    TestEqual(TEXT("...while its own count keeps reporting what is behind it"),
        Model.Get_VisibleCountsByCategory()[MeshIndex], 2);
    TestEqual(TEXT("...and the surviving category is unaffected"),
        Model.Get_VisibleCountsByCategory()[TextureIndex], 1);

    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, true);

    // ---- EVERY other axis IS honoured, which is what keeps these counts describing the filter the reader built ----
    Model.Set_PathScope(TEXT("/Game/Props"));

    TestEqual(TEXT("A path scope that excludes a category's findings DOES reduce its count"),
        Model.Get_VisibleCountsByCategory()[MeshIndex], 0);
    TestEqual(TEXT("...and leaves the in-scope category alone"),
        Model.Get_VisibleCountsByCategory()[TextureIndex], 1);

    Model.Set_PathScope(FString{});
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, false);

    TestEqual(TEXT("A severity mask reduces the per-category count too"),
        Model.Get_VisibleCountsByCategory()[MeshIndex], 1);
    TestEqual(TEXT("...down to nothing where every finding was that severity"),
        Model.Get_VisibleCountsByCategory()[TextureIndex], 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
