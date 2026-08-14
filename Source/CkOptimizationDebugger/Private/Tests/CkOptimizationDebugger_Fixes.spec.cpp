#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Fixes.h"
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Navigation.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_fixes_spec
{
    /** A finding built through the REAL `Build_Finding`, so the key the registry and the batch partition see is the
     *  same string the window would reuse a row by. The target path varies with the check id, which is what keeps
     *  two findings in one fixture distinguishable. */
    auto
        Build_TestFinding(
            FName InCheckId,
            bool InHasAutoFix)
        -> FCkOptimizationDebugger_FindingRow
    {
        const auto CheckName = InCheckId.ToString();

        auto Finding = ck_optimization_debugger_scan::Build_Finding(InCheckId,
            ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh,
            ck_optimization_debugger_scan::Build_AssetTarget(
                FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/{}.{}"), CheckName, CheckName)},
                CheckName),
            TEXT("Spec finding"),
            TEXT("Why."),
            TEXT("What to do."));

        Finding.HasAutoFix = InHasAutoFix;

        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The ten checks that set `HasAutoFix`. Spelled out here rather than read from the registry, because the whole
     *  point of the coverage test is to catch a registry that disagrees with the checks. */
    auto
        Get_ExpectedFixCheckIds()
        -> TArray<FName>
    {
        return TArray<FName>{
            FName{TEXT("Mesh.MissingLods")},
            FName{TEXT("Mesh.NaniteCandidate")},
            FName{TEXT("Mesh.NaniteOnLowPoly")},
            FName{TEXT("Mesh.ComplexCollision")},
            FName{TEXT("Texture.NormalMapCompression")},
            FName{TEXT("Texture.DataTextureSrgb")},
            FName{TEXT("Lighting.MovableLightCount")},
            FName{TEXT("Actor.EmptyStaticMesh")},
            FName{TEXT("Actor.InstancingCandidate")},
            FName{TEXT("ProjectSettings.TextureStreamingDisabled")}};
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Checks that explain something the reader acts on by hand. A fix appearing for one of these would mean the
     *  registry grew an action the check never promised. */
    auto
        Get_NonFixCheckIds()
        -> TArray<FName>
    {
        return TArray<FName>{
            FName{TEXT("Mesh.TriangleBudget")},
            FName{TEXT("Mesh.NaniteMaterialIncompatible")},
            FName{TEXT("Mesh.CollisionPrimitiveCount")},
            FName{TEXT("Texture.MaxSize")},
            FName{TEXT("Texture.NonPowerOfTwo")},
            FName{TEXT("Texture.MissingMipmaps")},
            FName{TEXT("Material.SlotCount")},
            FName{TEXT("Material.EmptySlot")},
            FName{TEXT("Material.DuplicateSlots")},
            FName{TEXT("Material.TranslucentTwoSided")},
            FName{TEXT("Material.SamplerBudget")},
            FName{TEXT("Lighting.LightmapResolution")},
            FName{TEXT("Blueprint.TickEnabled")},
            FName{TEXT("Blueprint.DependencyChain")},
            FName{TEXT("ProjectSettings.RayTracing")},
            FName{TEXT("ProjectSettings.PathTracing")},
            FName{TEXT("ProjectSettings.ForwardShading")},
            FName{TEXT("ProjectSettings.ExpensiveLightingFeatures")}};
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Fixes_RegistryCoverage,
    "Ck.OptimizationDebugger.Fixes.RegistryCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Fixes_RegistryCoverage::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixes;
    using namespace ck_optimization_debugger_fixes_spec;

    const auto Expected = Get_ExpectedFixCheckIds();

    // A check that advertises a fix through `HasAutoFix` and has no registry entry would render a FIX chip and an
    // enabled button that does nothing. That defect is invisible at runtime and obvious here.
    for (const auto& CheckId : Expected)
    {
        TestTrue(ck::Format_UE(TEXT("{} has a registered fix"), CheckId), Has_Fix(CheckId));

        const auto* Info = TryGet_FixInfo(CheckId);
        TestNotNull(ck::Format_UE(TEXT("{} resolves to fix info"), CheckId), Info);

        if (Info == nullptr)
        { continue; }

        TestFalse(ck::Format_UE(TEXT("{}'s verb names the action"), CheckId), Info->DisplayVerb.IsEmpty());
        TestEqual(ck::Format_UE(TEXT("{}'s entry is keyed by its own check id"), CheckId),
            Info->CheckId.ToString(), CheckId.ToString());
    }

    for (const auto& CheckId : Get_NonFixCheckIds())
    {
        TestFalse(ck::Format_UE(TEXT("{} has no registered fix"), CheckId), Has_Fix(CheckId));
    }

    // The registry holds exactly the fix-bearing checks — no more. An extra entry is an action nothing produces a
    // finding for.
    TestEqual(TEXT("The registry holds exactly the fix-bearing checks"), Get_AllFixes().Num(), Expected.Num());

    auto SeenIds = TSet<FName>{};

    for (const auto& Fix : Get_AllFixes())
    {
        TestFalse(ck::Format_UE(TEXT("{} appears exactly once"), Fix.CheckId), SeenIds.Contains(Fix.CheckId));
        SeenIds.Add(Fix.CheckId);
    }

    // Undo reaches the transaction buffer and nothing else. The config write is the one entry that must NOT claim
    // to be transactional, because a reader promised Undo on a `DefaultEngine.ini` line would get nothing.
    const auto* Streaming = TryGet_FixInfo(FName{TEXT("ProjectSettings.TextureStreamingDisabled")});
    TestNotNull(TEXT("The texture-streaming fix is registered"), Streaming);

    if (Streaming != nullptr)
    {
        TestEqual(TEXT("...and is classified as a config write, not a transaction"),
            Streaming->Execution, ECkOptimizationDebugger_FixExecution::ConfigWrite);
    }

    // The review-style fix WRITES NOTHING, which is a third thing — not a transaction and not a config write. It
    // shared the config-write bucket while that bucket was "everything non-transactional", which put a selection
    // action in a list named after ini edits.
    const auto* LightReview = TryGet_FixInfo(FName{TEXT("Lighting.MovableLightCount")});
    TestNotNull(TEXT("The light-mobility action is registered"), LightReview);

    if (LightReview != nullptr)
    {
        TestEqual(TEXT("...and is classified as Review — it is neither a transaction nor a config write"),
            LightReview->Execution, ECkOptimizationDebugger_FixExecution::Review);

        // The verb is the contract the button prints. "Review Light Mobility" read as though it changed mobility;
        // it selects actors and changes nothing.
        TestEqual(TEXT("...and its verb says it selects rather than changes"),
            LightReview->DisplayVerb, FString{TEXT("Select Lights For Review")});
    }

    // Exactly ONE entry writes a config file, and exactly ONE changes nothing. Both counts are pinned so a new fix
    // cannot quietly join either bucket without this failing.
    auto ConfigWriteCount = 0;
    auto ReviewCount = 0;

    for (const auto& Fix : Get_AllFixes())
    {
        if (Fix.Execution == ECkOptimizationDebugger_FixExecution::ConfigWrite)
        { ++ConfigWriteCount; }

        if (Fix.Execution == ECkOptimizationDebugger_FixExecution::Review)
        { ++ReviewCount; }
    }

    TestEqual(TEXT("Exactly one fix writes a config file"), ConfigWriteCount, 1);
    TestEqual(TEXT("Exactly one fix changes nothing at all"), ReviewCount, 1);

    // The two that remove actors are the two that must be flagged destructive.
    const auto* DeleteActor = TryGet_FixInfo(FName{TEXT("Actor.EmptyStaticMesh")});
    const auto* Instancing = TryGet_FixInfo(FName{TEXT("Actor.InstancingCandidate")});

    TestTrue(TEXT("Deleting an empty actor is destructive"), DeleteActor != nullptr && DeleteActor->IsDestructive);
    TestTrue(TEXT("Converting placements to instances is destructive"), Instancing != nullptr && Instancing->IsDestructive);

    // ...and nothing else is. `IsDestructive` drives the confirmation prompt, so an entry that acquired the flag by
    // accident would start asking the reader to agree to a property edit.
    auto DestructiveCount = 0;

    for (const auto& Fix : Get_AllFixes())
    {
        if (Fix.IsDestructive)
        { ++DestructiveCount; }
    }

    TestEqual(TEXT("Exactly two fixes are destructive"), DestructiveCount, 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Fixes_BatchConfirmation,
    "Ck.OptimizationDebugger.Fixes.BatchConfirmation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Fixes_BatchConfirmation::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixes;
    using namespace ck_optimization_debugger_fixes_spec;

    // The RULE this pins: a batch asks for confirmation exactly when it contains something Undo cannot take back —
    // an actor that stops existing, or a line written into a shared config file. A property edit inside a
    // transaction is one Ctrl+Z away and must not grow a dialog, or the reader learns to dismiss it unread.
    const auto Nanite = Build_TestFinding(FName{TEXT("Mesh.NaniteCandidate")}, true);
    const auto Srgb = Build_TestFinding(FName{TEXT("Texture.DataTextureSrgb")}, true);
    const auto DeleteActor = Build_TestFinding(FName{TEXT("Actor.EmptyStaticMesh")}, true);
    const auto Streaming = Build_TestFinding(FName{TEXT("ProjectSettings.TextureStreamingDisabled")}, true);
    const auto LightReview = Build_TestFinding(FName{TEXT("Lighting.MovableLightCount")}, true);
    const auto NoFix = Build_TestFinding(FName{TEXT("Material.EmptySlot")}, false);

    const auto Harmless = Build_BatchConfirmation(TArray<FCkOptimizationDebugger_FindingRow>{Nanite, Srgb});

    TestFalse(TEXT("Two undoable property edits ask for nothing"), Harmless.IsRequired);
    TestEqual(TEXT("...and count no destructive fixes"), Harmless.DestructiveCount, 0);
    TestEqual(TEXT("...and no config writes"), Harmless.ConfigWriteCount, 0);
    TestTrue(TEXT("...and carry no prompt text to show"), Harmless.Body.IsEmpty());

    const auto WithDelete = Build_BatchConfirmation(
        TArray<FCkOptimizationDebugger_FindingRow>{Nanite, DeleteActor});

    TestTrue(TEXT("A batch that deletes an actor asks first"), WithDelete.IsRequired);
    TestEqual(TEXT("...and says how many destructive fixes are in it"), WithDelete.DestructiveCount, 1);
    TestFalse(TEXT("...and has something to print"), WithDelete.Body.IsEmpty());

    const auto WithConfig = Build_BatchConfirmation(
        TArray<FCkOptimizationDebugger_FindingRow>{Nanite, Streaming});

    TestTrue(TEXT("A batch that writes a config file asks first"), WithConfig.IsRequired);
    TestEqual(TEXT("...and counts the write"), WithConfig.ConfigWriteCount, 1);
    TestEqual(TEXT("...and does not call it destructive"), WithConfig.DestructiveCount, 0);

    const auto Both = Build_BatchConfirmation(
        TArray<FCkOptimizationDebugger_FindingRow>{DeleteActor, Streaming, Nanite});

    TestTrue(TEXT("A batch with both asks once"), Both.IsRequired);
    TestEqual(TEXT("...counting the destructive one"), Both.DestructiveCount, 1);
    TestEqual(TEXT("...and the config write separately"), Both.ConfigWriteCount, 1);

    // A review action changes nothing, so it is not a reason to ask — a prompt in front of "select some lights"
    // would be the tool crying wolf.
    const auto ReviewOnly = Build_BatchConfirmation(TArray<FCkOptimizationDebugger_FindingRow>{LightReview});

    TestFalse(TEXT("Selecting lights for review asks for nothing"), ReviewOnly.IsRequired);

    // Findings the registry cannot apply are not part of the question.
    const auto Unapplicable = Build_BatchConfirmation(TArray<FCkOptimizationDebugger_FindingRow>{NoFix});

    TestFalse(TEXT("A selection with nothing applicable asks for nothing"), Unapplicable.IsRequired);

    TestFalse(TEXT("An empty selection asks for nothing"),
        Build_BatchConfirmation(TArray<FCkOptimizationDebugger_FindingRow>{}).IsRequired);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Fixes_SelectionProjection,
    "Ck.OptimizationDebugger.Fixes.SelectionProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Fixes_SelectionProjection::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixes;
    using namespace ck_optimization_debugger_fixes_spec;

    const auto Fixable = Build_TestFinding(FName{TEXT("Mesh.NaniteCandidate")}, true);
    const auto NotFlagged = Build_TestFinding(FName{TEXT("Mesh.NaniteOnLowPoly")}, false);
    const auto NoFixAtAll = Build_TestFinding(FName{TEXT("Mesh.TriangleBudget")}, false);

    // A finding built before a check learned to set `HasAutoFix` must not sprout a button because the registry
    // grew one — both halves of the gate matter, and this is the half that is easy to lose.
    const auto FalselyFlagged = Build_TestFinding(FName{TEXT("Mesh.TriangleBudget")}, true);

    TestTrue(TEXT("A flagged finding whose check has a fix is applicable"), Can_ApplyFix(Fixable));
    TestFalse(TEXT("A finding the check did not flag is not applicable"), Can_ApplyFix(NotFlagged));
    TestFalse(TEXT("A finding whose check has no fix is not applicable"), Can_ApplyFix(NoFixAtAll));
    TestFalse(TEXT("...even when it claims an auto-fix the registry does not have"), Can_ApplyFix(FalselyFlagged));

    const auto Selection = TArray<FCkOptimizationDebugger_FindingRow>{Fixable, NotFlagged, NoFixAtAll, FalselyFlagged};
    const auto Projected = Get_FixableFindings(Selection);

    TestEqual(TEXT("Only the applicable finding survives the projection"), Projected.Num(), 1);

    if (Projected.Num() == 1)
    { TestEqual(TEXT("...and it is the one that was flagged"), Projected[0].StableKey, Fixable.StableKey); }

    // The single-selection label names the ACTION — "Fix 1 Finding" would tell the reader nothing they did not
    // already know from having selected it.
    TestEqual(TEXT("One applicable finding labels the button with its own verb"),
        Build_FixButtonLabel(Projected), FString{TEXT("Enable Nanite")});

    const auto Two = TArray<FCkOptimizationDebugger_FindingRow>{
        Fixable, Build_TestFinding(FName{TEXT("Texture.DataTextureSrgb")}, true)};

    TestEqual(TEXT("Several applicable findings label the button with the count"),
        Build_FixButtonLabel(Get_FixableFindings(Two)), FString{TEXT("Fix 2 Findings")});

    TestEqual(TEXT("An empty selection still has a label to show while disabled"),
        Build_FixButtonLabel(TArray<FCkOptimizationDebugger_FindingRow>{}), FString{TEXT("Apply Fix")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Fixes_BatchPartition,
    "Ck.OptimizationDebugger.Fixes.BatchPartition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Fixes_BatchPartition::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixes;
    using namespace ck_optimization_debugger_fixes_spec;

    const auto Nanite = Build_TestFinding(FName{TEXT("Mesh.NaniteCandidate")}, true);
    const auto Srgb = Build_TestFinding(FName{TEXT("Texture.DataTextureSrgb")}, true);
    const auto Streaming = Build_TestFinding(FName{TEXT("ProjectSettings.TextureStreamingDisabled")}, true);
    const auto LightReview = Build_TestFinding(FName{TEXT("Lighting.MovableLightCount")}, true);
    const auto NoFix = Build_TestFinding(FName{TEXT("Material.EmptySlot")}, false);

    const auto Partition = Partition_ForBatch(
        TArray<FCkOptimizationDebugger_FindingRow>{Nanite, Streaming, LightReview, NoFix, Srgb});

    // The split is what keeps one Ctrl+Z honest: everything the transaction buffer can hold goes in one record, the
    // config write is applied after it and outside it, and the action that writes NOTHING is in neither.
    TestEqual(TEXT("Both asset edits land in the transactional part"), Partition.Transactional.Num(), 2);
    TestEqual(TEXT("The config write is separated out"), Partition.ConfigWrite.Num(), 1);
    TestEqual(TEXT("The review action is separated from BOTH"), Partition.Review.Num(), 1);

    if (Partition.ConfigWrite.Num() == 1)
    {
        TestEqual(TEXT("...and it is the texture-streaming one"),
            Partition.ConfigWrite[0].CheckId, FName{TEXT("ProjectSettings.TextureStreamingDisabled")});
    }

    if (Partition.Review.Num() == 1)
    {
        // The one that replaces the editor selection. It belongs in its own bucket both because it is not a write
        // and because `Apply_Fixes` has to run it LAST — running it first would leave the batch's own actor edits
        // selecting over the top of the set it just handed the reader.
        TestEqual(TEXT("...and it is the light-selection one"),
            Partition.Review[0].CheckId, FName{TEXT("Lighting.MovableLightCount")});
    }

    if (Partition.Transactional.Num() == 2)
    {
        // Input order is preserved, so a batch applies in the order the reader selected rather than in whatever
        // order the registry happens to list.
        TestEqual(TEXT("The transactional part keeps the input's order"),
            Partition.Transactional[0].CheckId, FName{TEXT("Mesh.NaniteCandidate")});
        TestEqual(TEXT("...both of them"),
            Partition.Transactional[1].CheckId, FName{TEXT("Texture.DataTextureSrgb")});
    }

    const auto Empty = Partition_ForBatch(TArray<FCkOptimizationDebugger_FindingRow>{NoFix});

    TestTrue(TEXT("A selection with nothing applicable partitions to nothing"),
        Empty.Transactional.IsEmpty() && Empty.ConfigWrite.IsEmpty() && Empty.Review.IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Navigation_TargetRouting,
    "Ck.OptimizationDebugger.Navigation.TargetRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Navigation_TargetRouting::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_navigation;
    using namespace ck_optimization_debugger_scan;

    const auto AssetTarget = Build_AssetTarget(
        FSoftObjectPath{TEXT("/Game/Spec/SM_Boulder.SM_Boulder")}, TEXT("SM_Boulder"));

    const auto ActorTarget = Build_ActorTarget(
        FSoftObjectPath{TEXT("/Game/Maps/L_Main.L_Main:PersistentLevel.SM_Rock_12")},
        TEXT("SM_Rock_12"), TEXT("L_Main"));

    const auto SettingsTarget = Build_ProjectSettingsTarget(TEXT("Rendering"), TEXT("Texture Streaming"));

    TestTrue(TEXT("An asset target is navigable"), Can_Navigate(AssetTarget));
    TestTrue(TEXT("An actor target is navigable"), Can_Navigate(ActorTarget));
    TestTrue(TEXT("A settings target is navigable"), Can_Navigate(SettingsTarget));

    // The whole point of scope-aware navigation: an asset finding goes to the ASSET, never to an actor that happens
    // to place it. The description is what the tooltip promises, so it is what must not drift.
    const auto AssetDescription = Get_NavigationDescription(AssetTarget);
    TestTrue(TEXT("An asset routes to the Content Browser"), AssetDescription.Contains(TEXT("Content Browser")));
    TestTrue(TEXT("...naming the asset"), AssetDescription.Contains(TEXT("SM_Boulder")));

    const auto ActorDescription = Get_NavigationDescription(ActorTarget);
    TestTrue(TEXT("An actor routes to the level viewport"), ActorDescription.Contains(TEXT("SM_Rock_12")));
    TestTrue(TEXT("...naming the level it has to be loaded from"), ActorDescription.Contains(TEXT("L_Main")));

    const auto SettingsDescription = Get_NavigationDescription(SettingsTarget);
    TestTrue(TEXT("A settings finding routes to Project Settings"),
        SettingsDescription.Contains(TEXT("Project Settings")));
    TestTrue(TEXT("...naming its section"), SettingsDescription.Contains(TEXT("Rendering")));

    // A target with nothing behind it is not navigable, which is what keeps the button from promising a place it
    // cannot take the reader to.
    auto PathlessAsset = AssetTarget;
    PathlessAsset.Path = FSoftObjectPath{};
    TestFalse(TEXT("An asset target with no path is not navigable"), Can_Navigate(PathlessAsset));

    auto SectionlessSettings = SettingsTarget;
    SectionlessSettings.SettingsSectionName = FString{};
    TestFalse(TEXT("A settings target with no section is not navigable"), Can_Navigate(SectionlessSettings));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
