#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ProjectScan.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_project_scan_spec
{
    auto
        Build_Thresholds()
        -> FCkOptimizationDebugger_Thresholds
    {
        // Hand-built rather than read from the settings CDO: a spec that depended on one QA person's calibration
        // would pass or fail depending on whose machine ran it.
        auto Thresholds = FCkOptimizationDebugger_Thresholds{};
        Thresholds.MaxTriangleCountLOD0 = 100000;
        Thresholds.MinTrianglesForNanite = 5000;
        Thresholds.MaxTrianglesForNaniteWarning = 2000;
        Thresholds.MaxCollisionPrimitives = 8;
        Thresholds.MaxTextureSize = 2048;
        Thresholds.MaxMaterialSlots = 8;

        return Thresholds;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_MeshFacts(
            const FString& InName)
        -> FCkOptimizationDebugger_AssetFacts
    {
        auto Facts = FCkOptimizationDebugger_AssetFacts{};
        Facts.Path = FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InName, InName)};
        Facts.DisplayName = InName;
        Facts.IsStaticMesh = true;

        return Facts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_TextureFacts(
            const FString& InName)
        -> FCkOptimizationDebugger_AssetFacts
    {
        auto Facts = FCkOptimizationDebugger_AssetFacts{};
        Facts.Path = FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InName, InName)};
        Facts.DisplayName = InName;
        Facts.IsTexture = true;

        return Facts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Has_Finding(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            const FString& InCheckId)
        -> bool
    {
        return InFindings.ContainsByPredicate([&InCheckId](const FCkOptimizationDebugger_FindingRow& InFinding) -> bool
        {
            return InFinding.CheckId == FName{*InCheckId};
        });
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_ProjectScan_RegistryChecks,
    "Ck.OptimizationDebugger.ProjectScan.RegistryChecks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_ProjectScan_RegistryChecks::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_project_scan;
    using namespace ck_optimization_debugger_project_scan_spec;

    const auto Thresholds = Build_Thresholds();

    // ---- A dense mesh with one LOD and Nanite off ----
    auto Dense = Build_MeshFacts(TEXT("SM_Dense"));
    Dense.TriangleCount = 250000;
    Dense.LodCount = 1;
    Dense.NaniteEnabled = 0;

    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};
    Run_RegistryChecks(Dense, Thresholds, Findings);

    TestTrue(TEXT("Over the triangle budget fires"), Has_Finding(Findings, TEXT("Mesh.TriangleBudget")));
    TestTrue(TEXT("One LOD with Nanite off fires"), Has_Finding(Findings, TEXT("Mesh.MissingLods")));
    TestTrue(TEXT("Nanite candidate fires"), Has_Finding(Findings, TEXT("Mesh.NaniteCandidate")));
    TestFalse(TEXT("Nanite-on-low-poly does not"), Has_Finding(Findings, TEXT("Mesh.NaniteOnLowPoly")));

    // The provenance sentence is on every project finding. Without it a project row is indistinguishable from a
    // level row whose level went missing, which is a different and much more alarming thing.
    TestTrue(TEXT("A project finding says where it came from"),
        Findings[0].Explanation.Contains(TEXT("project scan")));

    // ---- A clean mesh fires nothing ----
    auto Clean = Build_MeshFacts(TEXT("SM_Clean"));
    Clean.TriangleCount = 900;
    Clean.LodCount = 4;
    Clean.NaniteEnabled = 0;
    Clean.CollisionPrimitiveCount = 2;
    Clean.MaterialSlotCount = 2;

    Findings.Reset();
    Run_RegistryChecks(Clean, Thresholds, Findings);

    TestEqual(TEXT("A clean mesh produces no findings"), Findings.Num(), 0);

    // ---- Nanite on a low-poly mesh ----
    auto LowPoly = Build_MeshFacts(TEXT("SM_Pebble"));
    LowPoly.TriangleCount = 300;
    LowPoly.LodCount = 1;
    LowPoly.NaniteEnabled = 1;

    Findings.Reset();
    Run_RegistryChecks(LowPoly, Thresholds, Findings);

    TestTrue(TEXT("Nanite on a low-poly mesh fires"), Has_Finding(Findings, TEXT("Mesh.NaniteOnLowPoly")));
    TestFalse(TEXT("...and the missing-LOD check does not, because Nanite is doing that job"),
        Has_Finding(Findings, TEXT("Mesh.MissingLods")));

    // ---- A texture over budget, and a non-power-of-two one ----
    auto Big = Build_TextureFacts(TEXT("T_Big"));
    Big.Width = 4096;
    Big.Height = 4096;

    Findings.Reset();
    Run_RegistryChecks(Big, Thresholds, Findings);

    TestTrue(TEXT("An over-budget texture fires"), Has_Finding(Findings, TEXT("Texture.MaxSize")));
    TestFalse(TEXT("...and a power-of-two one does not trip the POT check"),
        Has_Finding(Findings, TEXT("Texture.NonPowerOfTwo")));

    auto Odd = Build_TextureFacts(TEXT("T_Odd"));
    Odd.Width = 1000;
    Odd.Height = 512;

    Findings.Reset();
    Run_RegistryChecks(Odd, Thresholds, Findings);

    TestTrue(TEXT("A non-power-of-two texture fires"), Has_Finding(Findings, TEXT("Texture.NonPowerOfTwo")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_ProjectScan_UnknownIsNotZero,
    "Ck.OptimizationDebugger.ProjectScan.UnknownIsNotZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_ProjectScan_UnknownIsNotZero::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_project_scan;
    using namespace ck_optimization_debugger_project_scan_spec;

    const auto Thresholds = Build_Thresholds();

    TestFalse(TEXT("-1 is not a known value"), Get_IsKnown(-1));
    TestTrue(TEXT("Zero IS a known value"), Get_IsKnown(0));

    // A registry built by an older editor, an asset saved before a tag existed, a class that writes no tags: all
    // ordinary, and all of them arrive as -1. A check that read that as zero would report every one of them as
    // clean — the silent false-negative this whole convention exists to prevent.
    auto Silent = Build_MeshFacts(TEXT("SM_NoTags"));

    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};
    Run_RegistryChecks(Silent, Thresholds, Findings);

    TestEqual(TEXT("An asset the registry said nothing about produces no findings"), Findings.Num(), 0);

    // And specifically: an unknown triangle count must not read as "under the Nanite floor" either, which would be
    // a finding fired on the strength of a number nobody has.
    Silent.NaniteEnabled = 1;

    Findings.Reset();
    Run_RegistryChecks(Silent, Thresholds, Findings);

    TestFalse(TEXT("An unknown triangle count does not fire the low-poly check"),
        Has_Finding(Findings, TEXT("Mesh.NaniteOnLowPoly")));

    // A texture with no Dimensions tag is the same case.
    auto NoDimensions = Build_TextureFacts(TEXT("T_NoTags"));

    Findings.Reset();
    Run_RegistryChecks(NoDimensions, Thresholds, Findings);

    TestEqual(TEXT("A texture with no dimensions produces no findings"), Findings.Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_ProjectScan_Progress,
    "Ck.OptimizationDebugger.ProjectScan.Progress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_ProjectScan_Progress::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_project_scan_spec;

    auto State = FCkOptimizationDebugger_ProjectScanState{};

    TestEqual(TEXT("An empty scan reports no progress rather than dividing by zero"), State.Get_Progress(), 0.0f);

    State.Assets.Add(Build_MeshFacts(TEXT("SM_A")));
    State.Assets.Add(Build_MeshFacts(TEXT("SM_B")));

    TestEqual(TEXT("Nothing walked is zero progress"), State.Get_Progress(), 0.0f);

    State.NextRegistryIndex = 1;
    TestEqual(TEXT("Half the registry pass is half the progress"), State.Get_Progress(), 0.5f);

    // The deep queue GROWS while the registry pass walks, so the denominator moves under the numerator. A bar that
    // read full while loads remained is a bar the reader stops believing.
    State.DeepQueue.Add(0);
    State.NextRegistryIndex = 2;

    TestTrue(TEXT("A finished registry pass with loads outstanding is not complete"), State.Get_Progress() < 1.0f);

    State.NextDeepIndex = 1;
    TestEqual(TEXT("Both passes done is complete"), State.Get_Progress(), 1.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_ProjectScan_HeaviestMeshes,
    "Ck.OptimizationDebugger.ProjectScan.HeaviestMeshes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_ProjectScan_HeaviestMeshes::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_project_scan;
    using namespace ck_optimization_debugger_project_scan_spec;

    auto Light = Build_MeshFacts(TEXT("SM_Light"));
    Light.TriangleCount = 500;

    auto Heavy = Build_MeshFacts(TEXT("SM_Heavy"));
    Heavy.TriangleCount = 900000;

    auto Middle = Build_MeshFacts(TEXT("SM_Middle"));
    Middle.TriangleCount = 40000;

    // The registry said nothing about this one. It must be ABSENT from the ranking, not bottom of it: a table
    // claiming a mesh has no triangles because nobody wrote the tag is worse than a shorter table.
    auto Untagged = Build_MeshFacts(TEXT("SM_Untagged"));

    auto Texture = Build_TextureFacts(TEXT("T_NotAMesh"));
    Texture.Width = 4096;
    Texture.Height = 4096;

    const auto Ranked = Get_HeaviestMeshes({Light, Heavy, Untagged, Texture, Middle}, 10);

    TestEqual(TEXT("Only meshes with a known triangle count are ranked"), Ranked.Num(), 3);
    TestEqual(TEXT("The densest is first"), Ranked[0].DisplayName, FString{TEXT("SM_Heavy")});
    TestEqual(TEXT("...then the next"), Ranked[1].DisplayName, FString{TEXT("SM_Middle")});
    TestEqual(TEXT("...then the least"), Ranked[2].DisplayName, FString{TEXT("SM_Light")});

    // The cap is a cap, not a suggestion.
    TestEqual(TEXT("The top count bounds the table"), Get_HeaviestMeshes({Light, Heavy, Middle}, 2).Num(), 2);

    // Two meshes of equal density break on path, so an unstable sort cannot swap them between identical scans.
    auto TieA = Build_MeshFacts(TEXT("SM_Alpha"));
    TieA.TriangleCount = 1000;

    auto TieZ = Build_MeshFacts(TEXT("SM_Zulu"));
    TieZ.TriangleCount = 1000;

    TestEqual(TEXT("A density tie breaks on path"),
        Get_HeaviestMeshes({TieZ, TieA}, 10)[0].DisplayName, FString{TEXT("SM_Alpha")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
