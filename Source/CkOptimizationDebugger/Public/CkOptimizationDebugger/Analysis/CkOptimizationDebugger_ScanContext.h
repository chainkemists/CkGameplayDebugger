#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"  // EComponentMobility — the usage rows carry it by value
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

// --------------------------------------------------------------------------------------------------------------------

class AActor;
class ULightComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;

// --------------------------------------------------------------------------------------------------------------------

/**
 * What one pass over the open levels found, gathered ONCE and handed to every check.
 *
 * The pointers here are weak and live only for the duration of the synchronous scan that produced them. Nothing in
 * this struct is stored on the model: findings are plain data by contract (see the module CLAUDE.md's
 * no-live-handle invariant), and a context that outlived its scan would be exactly the state that invariant exists
 * to forbid.
 *
 * Gathering once rather than letting each check walk the world again is not only speed. A check that re-walks can
 * disagree with its neighbour about what was in the level, and two findings that disagree about the same level are
 * worse than one finding fewer.
 */

// --------------------------------------------------------------------------------------------------------------------

/** One level the scan walked. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanLevel
{
    // The level's package short name — what the reader calls the level.
    FString LevelName;

    // The level asset's own path, so a per-level finding can still name something the Content Browser can find.
    FSoftObjectPath LevelPath;

    bool IsPersistentLevel = false;

    int32 ActorCount = 0;

    // Movable lights found in THIS level, counted during the gather rather than recounted per check. The lighting
    // check's question is per-level ("how many share a room"), and `Lights` below is a flat list across every
    // scanned level — deriving the per-level number by re-filtering that list in the check would put the same
    // counting rule in two places.
    int32 MovableLightCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** One actor the scan walked, addressed the way a finding has to address it: by path, never by pointer. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanActor
{
    TWeakObjectPtr<AActor> Actor;

    FSoftObjectPath ActorPath;
    FString DisplayName;
    FString LevelName;
};

// --------------------------------------------------------------------------------------------------------------------

/** One static-mesh component the scan walked, with everything the mesh / material / lighting / instancing checks
 *  need to reason about the way this level USES a mesh — as opposed to what the mesh asset is in itself. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanMeshUsage
{
    TWeakObjectPtr<UStaticMeshComponent> Component;
    TWeakObjectPtr<UStaticMesh> Mesh;

    FSoftObjectPath ActorPath;
    FSoftObjectPath MeshPath;

    FString ActorDisplayName;
    FString MeshDisplayName;
    FString LevelName;

    TEnumAsByte<EComponentMobility::Type> Mobility = EComponentMobility::Static;

    bool OverridesLightMapRes = false;
    int32 OverriddenLightMapRes = 0;

    // Whether the owning actor is a plain AStaticMeshActor — the only shape the instancing candidate check is
    // willing to propose converting, because anything else may carry logic an instance cannot.
    bool IsPlainStaticMeshActor = false;

    // Mesh path + every resolved material path, joined. Two usages sharing this string are interchangeable as far
    // as an instanced renderer is concerned, which is exactly what the instancing check groups on.
    FString InstancingSignature;
};

// --------------------------------------------------------------------------------------------------------------------

/** One light component the scan walked. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanLight
{
    TWeakObjectPtr<ULightComponent> Light;

    FSoftObjectPath ActorPath;
    FString DisplayName;
    FString LevelName;

    TEnumAsByte<EComponentMobility::Type> Mobility = EComponentMobility::Static;
};

// --------------------------------------------------------------------------------------------------------------------

/** One asset the scan reached, and the levels that reach it.
 *
 *  The referencing-level list is what makes an ASSET finding actionable: "this mesh is too dense" is a fact about
 *  the mesh, but "…and it is placed in Gym_Combat and Gym_Traversal" is what tells the reader whether they care. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanAsset
{
    TWeakObjectPtr<UObject> Asset;

    FSoftObjectPath Path;
    FString DisplayName;

    // Sorted and unique — a finding's explanation quotes this, and an explanation that reordered itself between two
    // identical scans would read as a change that did not happen.
    TArray<FString> ReferencingLevelNames;

    // How many placements of this asset the scan saw, across every referencing level.
    int32 UsageCount = 0;

    auto Get_ReferencingLevelsText() const -> FString;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ScanContext
{
    // Persistent level first, then loaded sub-levels in the order the world lists them.
    TArray<FCkOptimizationDebugger_ScanLevel> Levels;

    // Sub-levels that exist in the world but are NOT loaded. Reported rather than skipped silently: a check that
    // says nothing about a level it never opened is indistinguishable from a check that found it clean.
    TArray<FString> SkippedUnloadedLevelNames;

    TArray<FCkOptimizationDebugger_ScanActor> Actors;
    TArray<FCkOptimizationDebugger_ScanMeshUsage> MeshUsages;
    TArray<FCkOptimizationDebugger_ScanLight> Lights;

    // Unique assets, keyed by path, in first-encounter order made deterministic by an explicit sort at gather time.
    TArray<FCkOptimizationDebugger_ScanAsset> StaticMeshes;
    TArray<FCkOptimizationDebugger_ScanAsset> Materials;
    TArray<FCkOptimizationDebugger_ScanAsset> Textures;
    TArray<FCkOptimizationDebugger_ScanAsset> BlueprintClasses;

    auto Get_ScannedLevelNames() const -> TArray<FString>;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_scan
{
    /** Builds a finding with its stable key already derived from the check id and the target — the one place that
     *  pairing is made, so no check can invent a second key format for the same idea. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_Finding(
        FName InCheckId,
        ECkOptimizationDebugger_Severity InSeverity,
        ECkOptimizationDebugger_Category InCategory,
        const FCkOptimizationDebugger_Target& InTarget,
        const FString& InTitle,
        const FString& InExplanation,
        const FString& InRecommendation) -> FCkOptimizationDebugger_FindingRow;

    /** An asset target — a mesh, texture, material, Blueprint class or level asset. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_AssetTarget(
        const FSoftObjectPath& InPath,
        const FString& InDisplayName) -> FCkOptimizationDebugger_Target;

    /** An actor target. The level name rides along because an actor path alone does not tell the reader which
     *  sub-level they have to have loaded to go look at it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ActorTarget(
        const FSoftObjectPath& InActorPath,
        const FString& InDisplayName,
        const FString& InLevelName) -> FCkOptimizationDebugger_Target;

    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ProjectSettingsTarget(
        const FString& InSectionName,
        const FString& InDisplayName) -> FCkOptimizationDebugger_Target;

    /** "used in Gym_Combat, Gym_Traversal" — the sentence fragment an asset finding's explanation ends with. Empty
     *  when the asset has no recorded referencing level, so a caller can append it unconditionally. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_UsageSentence(
        const FCkOptimizationDebugger_ScanAsset& InAsset) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
