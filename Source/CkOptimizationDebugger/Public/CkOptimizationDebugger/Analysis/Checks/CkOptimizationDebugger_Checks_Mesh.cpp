#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Mesh.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "BodySetupEnums.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

// --------------------------------------------------------------------------------------------------------------------

// The file-local helpers live in the module's own named namespace rather than an anonymous one — this module
// compiles unity, and two anonymous helpers of the same name in two check files would collide in the merged TU.
namespace ck_optimization_debugger_checks_mesh
{
    // Contract, and why the authored flag rather than the usage-check API, are on the declaration in the header.
    auto
        Is_NaniteIncompatible(
            const UMaterialInterface* InMaterial)
        -> bool
    {
        if (InMaterial == nullptr)
        { return false; }

        const auto* BaseMaterial = InMaterial->GetMaterial();

        if (BaseMaterial == nullptr)
        { return false; }

        return BaseMaterial->bUsedWithNanite == 0;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SimpleCollisionPrimitiveCount(
            const UStaticMesh* InMesh)
        -> int32
    {
        if (InMesh == nullptr)
        { return 0; }

        const auto* BodySetup = InMesh->GetBodySetup();

        if (BodySetup == nullptr)
        { return 0; }

        return BodySetup->AggGeom.GetElementCount();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Checks(
            const FCkOptimizationDebugger_ScanContext& InContext,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        for (const auto& Asset : InContext.StaticMeshes)
        {
            auto* Mesh = Cast<UStaticMesh>(Asset.Asset.Get());

            if (Mesh == nullptr)
            { continue; }

            const auto Target = Build_AssetTarget(Asset.Path, Asset.DisplayName);
            const auto Usage = Build_UsageSentence(Asset);

            const auto TriangleCount = Mesh->GetNumTriangles(0);
            const auto LodCount = Mesh->GetNumLODs();
            const auto NaniteEnabled = Mesh->IsNaniteEnabled();

            // ---- Mesh.TriangleBudget ----
            if (TriangleCount > InThresholds.MaxTriangleCountLOD0)
            {
                const auto Graded = Get_Graded(TriangleCount, InThresholds.MaxTriangleCountLOD0,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Mesh.TriangleBudget")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("LOD0 triangle count over budget"),
                    ck::Format_UE(TEXT("LOD0 carries {} triangles against a budget of {}. Every placement pays that vertex cost in full at close range, and Nanite is the only thing that changes the shape of that bill.{}"),
                        TriangleCount, InThresholds.MaxTriangleCountLOD0, Usage),
                    TEXT("Decimate the source mesh, split it into separate assets, or enable Nanite if this is a static prop."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.MissingLods ----
            // Only worth saying about a mesh dense enough for a second LOD to matter, and only when Nanite is not
            // already doing the job that LODs would do.
            if (LodCount <= 1 && NOT NaniteEnabled && TriangleCount >= InThresholds.MinTrianglesForNanite)
            {
                const auto Graded = Get_Graded(TriangleCount, InThresholds.MaxTriangleCountLOD0,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Mesh.MissingLods")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Dense mesh with a single LOD"),
                    ck::Format_UE(TEXT("{} triangles across exactly one LOD, with Nanite off. The mesh renders at full density no matter how few pixels it covers.{}"),
                        TriangleCount, Usage),
                    TEXT("Generate LODs, or enable Nanite if the mesh is static and opaque."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Generate a default LOD chain for this mesh through the engine's own LOD reduction settings.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.NaniteCandidate ----
            if (NOT NaniteEnabled && TriangleCount >= InThresholds.MinTrianglesForNanite)
            {
                const auto Graded = Get_Graded(TriangleCount, InThresholds.MinTrianglesForNanite,
                    ECkOptimizationDebugger_Severity::Minor);

                auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteCandidate")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Nanite candidate"),
                    ck::Format_UE(TEXT("{} triangles with Nanite off. Above roughly {} triangles Nanite's cluster culling usually costs less than drawing the mesh outright.{}"),
                        TriangleCount, InThresholds.MinTrianglesForNanite, Usage),
                    TEXT("Enable Nanite — then confirm every material on the mesh is flagged Used With Nanite."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Enable Nanite on this static mesh.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.NaniteOnLowPoly ----
            if (NaniteEnabled && TriangleCount > 0 && TriangleCount < InThresholds.MaxTrianglesForNaniteWarning)
            {
                auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteOnLowPoly")},
                    ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Nanite on a low-poly mesh"),
                    ck::Format_UE(TEXT("Nanite is on for a mesh of {} triangles, under the {}-triangle floor. Nanite's per-cluster and streaming overhead is not repaid by a mesh this simple.{}"),
                        TriangleCount, InThresholds.MaxTrianglesForNaniteWarning, Usage),
                    TEXT("Disable Nanite on this mesh unless it is instanced in the thousands."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Disable Nanite on this static mesh.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.NaniteMaterialIncompatible ----
            if (NaniteEnabled)
            {
                auto OffendingSlots = TArray<FString>{};

                for (const auto& StaticMaterial : Mesh->GetStaticMaterials())
                {
                    if (NOT Is_NaniteIncompatible(StaticMaterial.MaterialInterface))
                    { continue; }

                    OffendingSlots.Add(StaticMaterial.MaterialInterface->GetName());
                }

                if (NOT OffendingSlots.IsEmpty())
                {
                    auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteMaterialIncompatible")},
                        ECkOptimizationDebugger_Severity::Major,
                        ECkOptimizationDebugger_Category::Mesh,
                        Target,
                        TEXT("Nanite mesh with materials not flagged for Nanite"),
                        ck::Format_UE(TEXT("Nanite is on, but {} of this mesh's materials do not declare Used With Nanite: {}. A material that does not declare the usage falls back at render time, so the mesh silently stops being the thing you enabled Nanite for.{}"),
                            OffendingSlots.Num(), FString::Join(OffendingSlots, TEXT(", ")), Usage),
                        TEXT("Tick Used With Nanite on each listed material, or disable Nanite on the mesh."));

                    Finding.HasAutoFix = true;
                    Finding.FixDescription = TEXT("Set Used With Nanite on the offending materials (queues a shader compile).");

                    OutFindings.Add(MoveTemp(Finding));
                }
            }

            // ---- Mesh.ComplexCollision ----
            if (const auto* BodySetup = Mesh->GetBodySetup())
            {
                const auto TraceFlag = BodySetup->GetCollisionTraceFlag();

                if (TraceFlag == CTF_UseComplexAsSimple)
                {
                    auto Finding = Build_Finding(FName{TEXT("Mesh.ComplexCollision")},
                        ECkOptimizationDebugger_Severity::Major,
                        ECkOptimizationDebugger_Category::Mesh,
                        Target,
                        TEXT("Collision uses the render mesh"),
                        ck::Format_UE(TEXT("Collision complexity is Use Complex Collision As Simple, so every query against this mesh runs against its {} render triangles instead of a handful of primitives.{}"),
                            TriangleCount, Usage),
                        TEXT("Add simple collision primitives and switch the complexity back to Simple And Complex."));

                    Finding.HasAutoFix = true;
                    Finding.FixDescription = TEXT("Generate simple collision primitives for this mesh.");

                    OutFindings.Add(MoveTemp(Finding));
                }
            }

            // ---- Mesh.CollisionPrimitiveCount ----
            const auto PrimitiveCount = Get_SimpleCollisionPrimitiveCount(Mesh);

            if (PrimitiveCount > InThresholds.MaxCollisionPrimitives)
            {
                const auto Graded = Get_Graded(PrimitiveCount, InThresholds.MaxCollisionPrimitives,
                    ECkOptimizationDebugger_Severity::Minor);

                auto Finding = Build_Finding(FName{TEXT("Mesh.CollisionPrimitiveCount")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Simple collision built from many primitives"),
                    ck::Format_UE(TEXT("{} simple-collision primitives against a budget of {}. Each one is a separate broadphase entry and a separate narrowphase test per overlap.{}"),
                        PrimitiveCount, InThresholds.MaxCollisionPrimitives, Usage),
                    TEXT("Simplify the collision hull — a coarser shape that is queried once beats an exact one queried a dozen times."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
