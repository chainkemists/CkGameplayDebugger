#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Actor.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/EngineTypes.h"

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace — this module compiles unity, and an anonymous namespace
// would collide with the identically-shaped helpers in the other check files.
namespace ck_optimization_debugger_checks_actor
{
    /** One set of interchangeable placements: same mesh, same materials, same level population. The signature is
     *  the grouping key precisely because two placements that differ in ANY material cannot share one instanced
     *  renderer — proposing a conversion for them would be proposing a visual change. */
    struct FInstancingGroup
    {
        FString Signature;

        FSoftObjectPath MeshPath;
        FString MeshDisplayName;

        int32 Count = 0;
        TArray<FString> LevelNames;
    };

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

        // ---- Actor.EmptyStaticMesh ----
        for (const auto& Usage : InContext.MeshUsages)
        {
            if (Usage.Mesh.IsValid())
            { continue; }

            if (NOT Usage.IsPlainStaticMeshActor)
            {
                // A component with no mesh on a gameplay actor is very often deliberate — a socket, an attachment
                // point, something a script fills in. Only the bare AStaticMeshActor case is unambiguous.
                continue;
            }

            auto Finding = Build_Finding(FName{TEXT("Actor.EmptyStaticMesh")},
                ECkOptimizationDebugger_Severity::Minor,
                ECkOptimizationDebugger_Category::Actor,
                Build_ActorTarget(Usage.ActorPath, Usage.ActorDisplayName, Usage.LevelName),
                TEXT("Static Mesh Actor with no mesh"),
                TEXT("This is a plain Static Mesh Actor with nothing assigned. It renders nothing, but it is still a level entry, an outliner row, and a thing every future reader has to decide about."),
                TEXT("Delete it, or assign the mesh it was placed for."));

            Finding.HasAutoFix = true;
            Finding.FixDescription = TEXT("Delete this empty Static Mesh Actor.");

            OutFindings.Add(MoveTemp(Finding));
        }

        // ---- Actor.InstancingCandidate ----
        auto Groups = TArray<FInstancingGroup>{};
        auto GroupIndexBySignature = TMap<FString, int32>{};

        for (const auto& Usage : InContext.MeshUsages)
        {
            // Only placements an instanced renderer could actually replace: a plain Static Mesh Actor, static
            // mobility, and a real mesh. Anything else carries behaviour or movement an instance cannot.
            if (NOT Usage.IsPlainStaticMeshActor || Usage.Mobility != EComponentMobility::Static)
            { continue; }

            if (Usage.MeshPath.IsNull())
            { continue; }

            if (auto* Found = GroupIndexBySignature.Find(Usage.InstancingSignature))
            {
                auto& Group = Groups[*Found];
                ++Group.Count;
                Group.LevelNames.AddUnique(Usage.LevelName);
                continue;
            }

            auto Group = FInstancingGroup{};
            Group.Signature = Usage.InstancingSignature;
            Group.MeshPath = Usage.MeshPath;
            Group.MeshDisplayName = Usage.MeshDisplayName;
            Group.Count = 1;
            Group.LevelNames.Add(Usage.LevelName);

            GroupIndexBySignature.Add(Usage.InstancingSignature, Groups.Num());
            Groups.Add(MoveTemp(Group));
        }

        // Sorted before the reduction below so "the biggest group for this mesh" is decided the same way twice.
        Groups.Sort([](const FInstancingGroup& InLhs, const FInstancingGroup& InRhs)
        {
            if (InLhs.Count != InRhs.Count)
            { return InLhs.Count > InRhs.Count; }

            return InLhs.Signature.Compare(InRhs.Signature, ESearchCase::CaseSensitive) < 0;
        });

        // One finding per MESH, not per material combination: the finding's stable key is `check id | target path`,
        // so two groups sharing a mesh would produce two rows the list could not tell apart. The largest group wins
        // and its material combination is the one the recommendation is about.
        auto ReportedMeshes = TSet<FSoftObjectPath>{};

        for (const auto& Group : Groups)
        {
            if (Group.Count < InThresholds.MinRepeatedActorsForInstancing)
            { continue; }

            if (ReportedMeshes.Contains(Group.MeshPath))
            { continue; }

            ReportedMeshes.Add(Group.MeshPath);

            auto LevelNames = Group.LevelNames;
            LevelNames.Sort([](const FString& InLhs, const FString& InRhs)
            {
                return InLhs.Compare(InRhs, ESearchCase::IgnoreCase) < 0;
            });

            auto Finding = Build_Finding(FName{TEXT("Actor.InstancingCandidate")},
                Get_GraduatedSeverity(Group.Count, InThresholds.MinRepeatedActorsForInstancing,
                    ECkOptimizationDebugger_Severity::Major),
                ECkOptimizationDebugger_Category::Actor,
                Build_AssetTarget(Group.MeshPath,
                    Group.MeshDisplayName.IsEmpty() ? Group.MeshPath.GetAssetName() : Group.MeshDisplayName),
                TEXT("Repeated actors that could be instanced"),
                ck::Format_UE(TEXT("{} static-mobility Static Mesh Actors share this mesh AND its exact material set, against an instancing threshold of {}. Each one is its own draw call and its own scene-proxy; one hierarchical instanced component would draw them together. Found in: {}."),
                    Group.Count, InThresholds.MinRepeatedActorsForInstancing,
                    FString::Join(LevelNames, TEXT(", "))),
                TEXT("Convert the group to a Hierarchical Instanced Static Mesh component on a single actor."));

            Finding.HasAutoFix = true;

            // "UP TO", and the sentence says why. This count is what the SCAN matched — mesh, materials and static
            // mobility. The fix additionally refuses any placement with an attach parent, attached children or a
            // second scene component (an instance cannot express those), and converts one LEVEL at a time because a
            // hierarchical instanced component lives in exactly one. Promising the scan's number here and reporting
            // a smaller one after the click was the finding overstating what the button does.
            Finding.FixDescription = ck::Format_UE(TEXT("Convert up to {} matching Static Mesh Actors into HISM instances on one actor, inside a transaction Undo can reverse. The fix re-derives the group from the live world at the moment you press it: placements that are attached to (or have) another actor, or that carry a second scene component, are left alone, and it converts the largest group within a SINGLE level. The result message reports how many it actually took."),
                Group.Count);

            OutFindings.Add(MoveTemp(Finding));
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
