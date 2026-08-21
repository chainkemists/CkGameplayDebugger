#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"

// --------------------------------------------------------------------------------------------------------------------

class AActor;
class AStaticMeshActor;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** Why one placement cannot become an instance.
 *
 *  Named values rather than a bool, because the reader's next action differs per reason: a tagged placement needs a
 *  decision about the tag, a placement that differs from the template needs someone to look at the property named
 *  beside it, and an externally referenced one needs the reference followed. A gate that answered yes/no told them
 *  none of that — and told them nothing at all about the placements it silently dropped. */
enum class ECkOptimizationDebugger_RefusalKind : uint8
{
    /** A subclass may run logic an instance cannot carry. */
    NotPlainStaticMeshActor,

    /** The placement no longer uses the mesh the finding is about. */
    MeshChanged,

    /** A non-static placement may move; instances do not. */
    NonStaticMobility,

    /** An attachment in either direction is a relationship an instance has no way to express. */
    Attached,

    /** A second scene component is behaviour the conversion would drop. */
    ExtraSceneComponents,

    /** `AActor::Tags` — an instance is a transform in a component, so there is nowhere for a tag to go and every
     *  tag-gated query naming this actor would silently return one fewer result. */
    CarriesActorTags,

    /** `UActorComponent::ComponentTags`, for the same reason. */
    CarriesComponentTags,

    /** The placement belongs to an editor Group. Converting one member rewrites a relationship the level designer
     *  authored, and the surviving group would hold a reference to a destroyed actor. */
    InEditorGroup,

    /** A Data Layer assignment decides whether the placement loads at all. Instances carry the component's. */
    InDataLayer,

    /** An HLOD layer assignment is per actor and does not survive the conversion. */
    HasHlodLayer,

    /** A dynamic material instance is created and driven at runtime against THIS component. */
    DynamicMaterialInstance,

    /** Something outside this level names the actor — a Level Blueprint, another actor, or a soft path. */
    ExternallyReferenced,

    /** A property differs from the template placement, so one shared instanced component cannot represent both.
     *  `Detail` carries the property's own name. */
    DiffersFromTemplate,

    /** The placement's level is locked against edits. */
    LevelLocked,
};

// --------------------------------------------------------------------------------------------------------------------

/** One reason, plus whatever detail makes it actionable (a property name, a material name, a referencer count). */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Refusal
{
    ECkOptimizationDebugger_RefusalKind Kind = ECkOptimizationDebugger_RefusalKind::NotPlainStaticMeshActor;

    FString Detail;
};

// --------------------------------------------------------------------------------------------------------------------

/** What one candidate placement was judged to be, and every reason it was refused.
 *
 *  Plain data with no actor pointer: an audit is shown to the reader, carried into a preview and printed in a
 *  result message, and every one of those outlives the frame the world was walked in. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_PlacementAudit
{
    FSoftObjectPath Path;

    FString DisplayName;

    // Empty means convertible. Populated in the order the audit asked its questions, cheapest first.
    TArray<FCkOptimizationDebugger_Refusal> Refusals;

    auto
    Get_IsConvertible() const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

/** Whether a mesh's materials animate vertices, and the third answer that is not a yes or a no.
 *
 *  `Unknown` exists because `UMaterialInterface::GetCachedExpressionData()` returns a shared EMPTY record when a
 *  material has no cache, and every question asked of that record answers "no". Reporting that as "no World Position
 *  Offset" would be a claim the material never made — the same defect as printing `0 B` where the engine reports no
 *  separable figure. */
enum class ECkOptimizationDebugger_WpoAnswer : uint8
{
    NoWorldPositionOffset,
    UsesWorldPositionOffset,
    Unknown,
};

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/** Who references whom, built ONCE for a whole world and consulted per placement.
 *
 *  `FBlueprintEditorUtils::GetActorReferenceMap` walks the world; asking it per candidate would walk it forty times
 *  for one conversion. This is the same shape `UEditorEngine::edactDeleteSelected` uses for the same reason.
 *
 *  `WasBuilt` is on the record because "nothing references this placement" and "nobody asked" are different
 *  statements, and only the first of them permits a conversion. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ReferenceContext
{
    TMap<AActor*, TArray<AActor*>> ReferencingActors;

    bool WasBuilt = false;
};

#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_preflight
{
    /** The reason as one sentence, in the words the preview and the result message both print. Pure, so a spec can
     *  pin the wording without a world. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_RefusalSentence(
        const FCkOptimizationDebugger_Refusal& InRefusal) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ConvertiblePlacements(
        const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits) -> TArray<FCkOptimizationDebugger_PlacementAudit>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_RefusedPlacements(
        const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits) -> TArray<FCkOptimizationDebugger_PlacementAudit>;

    /** What was refused and why, counted by reason, as one line for the result message.
     *
     *  Counted rather than listed: forty refusals would push everything else off a status strip, and the reader's
     *  question at that moment is "why did it take twelve of forty", not "which twelve". The preview lists them
     *  individually. Reasons print in enum order so two runs over one selection produce the same sentence. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_RefusalSummary(
        const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits) -> FString;

    /** The properties the template comparison deliberately does NOT consider, and the only place one may be excluded.
     *
     *  The comparison is reflection-driven precisely so that no property can be forgotten — forgetting one is the
     *  defect that shipped. Every exclusion is therefore a decision, listed here, asserted by
     *  `Ck.OptimizationDebugger.Fixes.ComparisonExclusions`, and justified in the module doc. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ExcludedComparisonProperties() -> const TArray<FName>&;

    // ----------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

    /** Every reflected, editable property on which `InCandidate` differs from `InTemplate`, as refusals naming the
     *  property. Both objects must be the same class; a class mismatch is caught earlier by the audit.
     *
     *  Reflection rather than a hand-written property list: a list is a second place to forget a property, and the
     *  bug this exists to fix was exactly that — a conversion that compared mesh and material paths, then discarded
     *  custom primitive data, stencil values, shadow flags and draw distances without a word. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Compare_ReflectedProperties(
        const UObject* InTemplate,
        const UObject* InCandidate) -> TArray<FCkOptimizationDebugger_Refusal>;

    /** The world's actor-to-referencing-actors map, built once. Returns an unbuilt context when there is no world,
     *  which every caller must treat as "references unknown" rather than as "no references". */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ReferenceContext(
        UWorld* InWorld) -> FCkOptimizationDebugger_ReferenceContext;

    /** Judges ONE placement against the template placement. `InTemplate` may be the candidate itself, which is how
     *  the template is judged against the absolute reasons without comparing it to itself. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Audit_Placement(
        AStaticMeshActor* InCandidate,
        AStaticMeshActor* InTemplate,
        const UStaticMesh* InMesh,
        const FCkOptimizationDebugger_ReferenceContext& InReferences) -> FCkOptimizationDebugger_PlacementAudit;

    /** Removes an actor from its editor Group, exactly as `UEditorEngine::edactDeleteSelected` does before it
     *  destroys one. `UWorld::EditorDestroyActor` performs no such removal, so an actor destroyed while grouped
     *  leaves the surviving `AGroupActor` holding a reference to it.
     *
     *  Returns the group's name when it removed the actor from one, empty otherwise — the caller says so in its
     *  result message, because a level designer whose group silently lost a member has no way to find out. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Detach_FromEditorGroup(
        AActor* InActor) -> FString;

    /** Whether any of the mesh's slot materials animates vertex positions, naming the ones that do.
     *
     *  Read off the material's serialized cached-expression data, which is a load and not a shader compile — the
     *  same fence every other material read in this module holds to. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_WorldPositionOffsetAnswer(
        const UStaticMesh* InMesh,
        TArray<FString>& OutMaterialNames) -> ECkOptimizationDebugger_WpoAnswer;

    /** How many OTHER packages reference this asset, asset-registry only. What makes the blast radius of an edit to
     *  a shared material sayable before it is paid for. A negative return means the registry could not answer. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ReferencingPackageCount(
        const FSoftObjectPath& InPath) -> int32;

#endif
}

// --------------------------------------------------------------------------------------------------------------------
