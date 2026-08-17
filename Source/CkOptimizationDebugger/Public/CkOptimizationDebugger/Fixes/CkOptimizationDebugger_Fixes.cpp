#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Fixes.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#if WITH_EDITOR
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Mesh.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"

#include "BodySetupEnums.h"
#include "LevelUtils.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/RendererSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace rather than an anonymous one — this module compiles unity,
// and a same-named anonymous helper in another .cpp would collide in the merged translation unit.
namespace ck_optimization_debugger_fixes_impl
{
    // The check ids the registry and the dispatch BOTH key off. One definition per id, so the two cannot drift into
    // a state where a fix is advertised and nothing runs.
    const auto k_MeshMissingLods         = FName{TEXT("Mesh.MissingLods")};
    const auto k_MeshNaniteCandidate     = FName{TEXT("Mesh.NaniteCandidate")};
    const auto k_MeshNaniteOnLowPoly     = FName{TEXT("Mesh.NaniteOnLowPoly")};
    const auto k_MeshComplexCollision    = FName{TEXT("Mesh.ComplexCollision")};
    const auto k_TextureNormalMap        = FName{TEXT("Texture.NormalMapCompression")};
    const auto k_TextureDataSrgb         = FName{TEXT("Texture.DataTextureSrgb")};
    const auto k_TextureMissingMipmaps   = FName{TEXT("Texture.MissingMipmaps")};
    const auto k_MeshNaniteMaterial      = FName{TEXT("Mesh.NaniteMaterialIncompatible")};
    const auto k_LightingLightmapRes     = FName{TEXT("Lighting.LightmapResolution")};
    const auto k_BlueprintTickEnabled    = FName{TEXT("Blueprint.TickEnabled")};
    const auto k_LightingMovableCount    = FName{TEXT("Lighting.MovableLightCount")};
    const auto k_ActorEmptyStaticMesh    = FName{TEXT("Actor.EmptyStaticMesh")};
    const auto k_ActorInstancingCandidate = FName{TEXT("Actor.InstancingCandidate")};
    const auto k_SettingsTextureStreaming = FName{TEXT("ProjectSettings.TextureStreamingDisabled")};

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Failure(
            const FString& InMessage)
        -> FCkOptimizationDebugger_FixResult
    {
        auto Result = FCkOptimizationDebugger_FixResult{};
        Result.Succeeded = false;
        Result.Message = InMessage;
        Result.ChangedState = false;

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Success(
            const FString& InMessage,
            bool InChangedState)
        -> FCkOptimizationDebugger_FixResult
    {
        auto Result = FCkOptimizationDebugger_FixResult{};
        Result.Succeeded = true;
        Result.Message = InMessage;
        Result.ChangedState = InChangedState;

        return Result;
    }

#if WITH_EDITOR
    // ----------------------------------------------------------------------------------------------------------------

    /** An ASSET target is allowed to load: the finding names a package the reader asked us to change, and a fix that
     *  refused to load it would only work for assets that happened to still be in memory. */
    auto
        TryLoad_Asset(
            const FSoftObjectPath& InPath)
        -> UObject*
    {
        if (InPath.IsNull())
        { return nullptr; }

        if (auto* Resolved = InPath.ResolveObject())
        { return Resolved; }

        return InPath.TryLoad();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** An ACTOR target is NOT allowed to load. An actor path names something inside a world, and loading it would
     *  mean loading a map — so an actor whose level is closed is reported as gone rather than dragged back in. */
    auto
        TryResolve_Actor(
            const FSoftObjectPath& InPath)
        -> AActor*
    {
        if (InPath.IsNull())
        { return nullptr; }

        return Cast<AActor>(InPath.ResolveObject());
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_LevelShortName(
            const ULevel* InLevel)
        -> FString
    {
        if (InLevel == nullptr)
        { return FString{}; }

        const auto* Package = InLevel->GetOutermost();

        if (Package == nullptr)
        { return FString{}; }

        return FPackageName::GetShortName(Package->GetName());
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether the level this actor lives in is locked against edits.
     *
     *  `UWorld::EditorDestroyActor` performs no lock check of its own — the path that does is
     *  `UEditorEngine::edactDeleteSelected`, which this module deliberately does not go through (it operates on the
     *  editor selection, and a fix must act on the finding rather than on whatever the reader happens to have
     *  clicked). So the check that would otherwise be skipped is made here instead: a reader who locked a sub-level
     *  did so to stop exactly this. */
    auto
        Is_ActorLevelLocked(
            const AActor* InActor)
        -> bool
    {
        if (InActor == nullptr)
        { return false; }

        const auto* Level = InActor->GetLevel();

        return Level != nullptr && FLevelUtils::IsLevelLocked(const_cast<ULevel*>(Level));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Drops an actor out of the editor selection before it is destroyed.
     *
     *  `EditorDestroyActor` does not touch `USelection`, so an actor deleted while selected leaves a stale entry the
     *  outliner and the details panel both read. The engine's own delete path clears the selection for this reason;
     *  this one deletes a named actor rather than the selection, so it removes just that entry. */
    auto
        Deselect_ActorBeforeDestroy(
            AActor* InActor)
        -> void
    {
        if (GEditor == nullptr || ck::Is_NOT_Valid(InActor))
        { return; }

        if (NOT InActor->IsSelected())
        { return; }

        GEditor->SelectActor(InActor, /*bInSelected*/ false, /*bNotify*/ false);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Mesh fixes
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_NaniteEnabled(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            bool InEnabled)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        const auto StateWord = FString{InEnabled ? TEXT("enabled") : TEXT("disabled")};

        if (Mesh->IsNaniteEnabled() == InEnabled)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: Nanite is already {} — nothing to do."),
                InFinding.Target.DisplayName, StateWord));
        }

        // The SECOND half of the check's condition, re-asked. `Mesh.NaniteCandidate` fires on "Nanite off AND at
        // least N triangles"; re-validating only the flag would let a mesh somebody has since simplified past the
        // floor still be given Nanite, on the strength of a scan that is no longer true about it. The thresholds are
        // re-read here rather than carried on the finding: this is a user action, not a scan, so reading the current
        // setting is the honest thing — a reader who lowered the floor and pressed Fix means the new floor.
        const auto Thresholds = ck_optimization_debugger_thresholds::Build_FromSettings();
        const auto TriangleCount = Mesh->GetNumTriangles(0);

        if (InEnabled && TriangleCount < Thresholds.MinTrianglesForNanite)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: now {} triangles, under the {}-triangle Nanite floor — re-scan."),
                InFinding.Target.DisplayName, TriangleCount, Thresholds.MinTrianglesForNanite));
        }

        if (NOT InEnabled && TriangleCount >= Thresholds.MaxTrianglesForNaniteWarning)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: now {} triangles, at or over the {}-triangle low-poly floor — Nanite left on. Re-scan."),
                InFinding.Target.DisplayName, TriangleCount, Thresholds.MaxTrianglesForNaniteWarning));
        }

        Mesh->Modify();

        auto Settings = Mesh->GetNaniteSettings();
        Settings.bEnabled = InEnabled;
        Mesh->SetNaniteSettings(Settings);

        // The same PostEditChangeProperty the mesh editor's own Nanite checkbox raises — and the thing that actually
        // builds or drops the Nanite data. Setting the struct alone would leave the flag saying one thing and the
        // built data saying another.
        Mesh->NotifyNaniteSettingsChanged();
        Mesh->MarkPackageDirty();

        return Make_Success(ck::Format_UE(TEXT("{}: Nanite {}."),
            InFinding.Target.DisplayName, StateWord), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The LOD group a mesh with no LOD chain is given. Preferring the prop groups is a judgement about what an
     *  over-dense single-LOD mesh usually IS; falling through to the first configured group keeps the fix working on
     *  a project that renamed them. */
    auto
        Get_DefaultLodGroup()
        -> FName
    {
        auto Groups = TArray<FName>{};
        UStaticMesh::GetLODGroups(Groups);

        const auto Preferred = TArray<FName>{
            FName{TEXT("LargeProp")},
            FName{TEXT("SmallProp")},
            FName{TEXT("Deco")}};

        for (const auto& Candidate : Preferred)
        {
            if (Groups.Contains(Candidate))
            { return Candidate; }
        }

        for (const auto& Group : Groups)
        {
            if (NOT Group.IsNone())
            { return Group; }
        }

        return NAME_None;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_GenerateLods(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Mesh->GetNumLODs() > 1)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the mesh already has {} LODs — re-scan."),
                InFinding.Target.DisplayName, Mesh->GetNumLODs()));
        }

        const auto Group = Get_DefaultLodGroup();

        if (Group.IsNone())
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: this platform has no configured static-mesh LOD groups, so there is no reduction setup to apply."),
                InFinding.Target.DisplayName));
        }

        if (Mesh->GetLODGroup() == Group)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: already assigned to the {} LOD group yet still has one LOD — generate the chain in the Static Mesh editor."),
                InFinding.Target.DisplayName, Group));
        }

        Mesh->Modify();

        // `SetLODGroup` is the engine's own path: it applies the group's default LOD count and per-LOD reduction
        // settings and rebuilds. Driving the reduction interface directly from here would be a second, worse copy of
        // that rule — and the mesh-editor tooling that owns it is not reachable from a non-editor module.
        Mesh->SetLODGroup(Group);
        Mesh->MarkPackageDirty();

        return Make_Success(ck::Format_UE(TEXT("{}: assigned to the {} LOD group, which generated its LOD chain from the group's reduction settings."),
            InFinding.Target.DisplayName, Group), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_SimpleCollision(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        auto* BodySetup = Mesh->GetBodySetup();

        if (ck::Is_NOT_Valid(BodySetup))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the mesh has no body setup to change."),
                InFinding.Target.DisplayName));
        }

        if (BodySetup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: collision complexity is no longer Use Complex As Simple — re-scan."),
                InFinding.Target.DisplayName));
        }

        Mesh->Modify();
        BodySetup->Modify();

        // A mesh with the flag flipped and NO simple primitives would collide with nothing at all — a silent
        // behaviour change far worse than the cost the finding was about. One box from the mesh's own bounds is the
        // conservative stand-in; the reader is told to replace it with a real hull.
        auto AddedBox = false;

        if (BodySetup->AggGeom.GetElementCount() == 0)
        {
            const auto Bounds = Mesh->GetBounds();

            if (NOT Bounds.BoxExtent.IsNearlyZero())
            {
                auto BoxElem = FKBoxElem{};
                BoxElem.Center = Bounds.Origin;
                BoxElem.X = static_cast<float>(Bounds.BoxExtent.X * 2.0);
                BoxElem.Y = static_cast<float>(Bounds.BoxExtent.Y * 2.0);
                BoxElem.Z = static_cast<float>(Bounds.BoxExtent.Z * 2.0);

                BodySetup->AggGeom.BoxElems.Add(BoxElem);
                AddedBox = true;
            }
        }

        BodySetup->CollisionTraceFlag = CTF_UseSimpleAndComplex;

        BodySetup->InvalidatePhysicsData();
        BodySetup->CreatePhysicsMeshes();

        Mesh->PostEditChange();
        Mesh->MarkPackageDirty();

        return Make_Success(AddedBox
            ? ck::Format_UE(TEXT("{}: collision complexity set to Simple And Complex, with a box primitive added from the mesh bounds — replace it with a fitted hull when you can."),
                InFinding.Target.DisplayName)
            : ck::Format_UE(TEXT("{}: collision complexity set to Simple And Complex over the existing simple primitives."),
                InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Texture fixes
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_NormalMapCompression(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->CompressionSettings == TC_Normalmap && Texture->SRGB == 0)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: already compressed as a normal map — re-scan."),
                InFinding.Target.DisplayName));
        }

        Texture->Modify();

        Texture->CompressionSettings = TC_Normalmap;

        // A normal map is not colour data. Leaving sRGB on would apply a gamma curve to vectors, which is the same
        // defect the compression setting is being fixed for.
        Texture->SRGB = 0;

        Texture->PostEditChange();
        Texture->MarkPackageDirty();

        return Make_Success(ck::Format_UE(TEXT("{}: compression set to Normalmap and sRGB turned off."),
            InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_DisableSrgb(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->SRGB == 0)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: sRGB is already off — re-scan."),
                InFinding.Target.DisplayName));
        }

        // The OTHER half of the check's condition, re-asked through the check's own predicate rather than a second
        // copy of the rule. `Texture.DataTextureSrgb` fires on "sRGB on AND this reads as packed data"; a texture
        // re-authored as an albedo since the scan is no longer a data texture, and stripping sRGB off a colour map
        // would be a silent visual regression reported as a successful fix.
        if (NOT ck_optimization_debugger_checks_texture::Is_DataTexture(Texture, Texture->GetName()))
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: this no longer reads as a data texture — its compression settings or name have changed since the scan. sRGB left on; re-scan."),
                InFinding.Target.DisplayName));
        }

        Texture->Modify();

        Texture->SRGB = 0;

        Texture->PostEditChange();
        Texture->MarkPackageDirty();

        return Make_Success(ck::Format_UE(TEXT("{}: sRGB turned off."), InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_RestoreMipmaps(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->MipGenSettings != TMGS_NoMipmaps)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: mip generation is already on — re-scan."),
                InFinding.Target.DisplayName));
        }

        // The check's OTHER half. `Texture.MissingMipmaps` fires on "no mips AND not in the UI group", and a texture
        // moved into the UI group since the scan is legitimately mipless — generating mips for it would undo a
        // deliberate authoring decision and report it as a fix.
        if (Texture->LODGroup == TEXTUREGROUP_UI)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: this is now in the UI texture group, where shipping without mips is correct. Left alone; re-scan."),
                InFinding.Target.DisplayName));
        }

        Texture->Modify();

        // `FromTextureGroup` rather than a specific setting: the group is where a project states its mip policy, so
        // this hands the decision back to that policy instead of this tool inventing one per texture.
        Texture->MipGenSettings = TMGS_FromTextureGroup;

        Texture->PostEditChange();
        Texture->MarkPackageDirty();

        return Make_Success(ck::Format_UE(
            TEXT("{}: mip generation set to FromTextureGroup — the texture rebuilds with mips."),
            InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_NaniteMaterialUsage(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        // The check fires on "Nanite is ON and some slot's material does not declare the usage". A mesh whose Nanite
        // was turned off since the scan has nothing to fix here — and flagging its materials anyway would compile
        // shaders for a claim nothing is making.
        if (NOT Mesh->GetNaniteSettings().bEnabled)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: Nanite is no longer enabled on this mesh, so its materials do not need the usage flag. Re-scan."),
                InFinding.Target.DisplayName));
        }

        auto ChangedNames = TArray<FString>{};

        for (const auto& StaticMaterial : Mesh->GetStaticMaterials())
        {
            // The check's own predicate, exported rather than copied — a second spelling of "is this incompatible"
            // in the fix is a second place for it to drift from what the list reported.
            if (NOT ck_optimization_debugger_checks_mesh::Is_NaniteIncompatible(StaticMaterial.MaterialInterface))
            { continue; }

            auto* BaseMaterial = StaticMaterial.MaterialInterface->GetMaterial();

            if (BaseMaterial == nullptr)
            { continue; }

            // The BASE material carries the usage flag, so a mesh using several instances of one parent is fixed
            // once — `Is_NaniteIncompatible` stops matching for the rest on the next iteration.
            BaseMaterial->Modify();

            BaseMaterial->bUsedWithNanite = 1;

            BaseMaterial->PostEditChange();
            BaseMaterial->MarkPackageDirty();

            ChangedNames.AddUnique(BaseMaterial->GetName());
        }

        if (ChangedNames.IsEmpty())
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: every material on this mesh already declares Used With Nanite — re-scan."),
                InFinding.Target.DisplayName));
        }

        // Said out loud, because it is the cost the reader is about to pay and nothing else on screen would tell
        // them: setting the usage flag invalidates the material's shader map and queues a compile.
        return Make_Success(ck::Format_UE(
            TEXT("{}: Used With Nanite set on {} material(s) ({}). This queues a shader compile."),
            InFinding.Target.DisplayName,
            ChangedNames.Num(),
            FString::Join(ChangedNames, TEXT(", "))), true);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Actor fixes
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_ClampLightmapResolution(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Actor = TryResolve_Actor(InFinding.Target.Path);

        if (ck::Is_NOT_Valid(Actor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the actor is no longer in a loaded level — re-scan."),
                InFinding.Target.DisplayName));
        }

        // Read fresh rather than carried on the finding: a threshold the reader tightened after the scan is the one
        // they mean now, and clamping to a stale number would write a value the current settings still flag.
        const auto Thresholds = ck_optimization_debugger_thresholds::Build_FromSettings();
        const auto Budget = Thresholds.MaxLightmapResolution;

        if (Budget <= 0)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: the lightmap-resolution budget is not a positive number, so there is nothing to clamp to."),
                InFinding.Target.DisplayName));
        }

        // The level lock is checked BEFORE anything is modified, exactly as the two destructive actor fixes do:
        // `Modify` on a component in a locked level would rewrite a level the editor is protecting.
        if (Is_ActorLevelLocked(Actor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: its level is locked — unlock it first. Nothing changed."),
                InFinding.Target.DisplayName));
        }

        auto Components = TArray<UStaticMeshComponent*>{};
        Actor->GetComponents<UStaticMeshComponent>(Components);

        auto ChangedCount = 0;
        auto WorstBefore = 0;

        // EVERY over-budget component on the actor, not the first. The check aggregates per actor precisely because
        // one actor can carry several over-budget overrides, so a fix that clamped one of them would leave a finding
        // the reader watched "fix" and then reappear.
        for (auto* Component : Components)
        {
            if (ck::Is_NOT_Valid(Component))
            { continue; }

            if (NOT Component->bOverrideLightMapRes)
            { continue; }

            if (Component->OverriddenLightMapRes <= Budget)
            { continue; }

            WorstBefore = FMath::Max(WorstBefore, Component->OverriddenLightMapRes);

            Component->Modify();

            // Clamped to the budget rather than the override cleared: clearing falls back to the mesh's own default,
            // which is a DIFFERENT number nobody chose and may be higher than the budget too. Clamping states exactly
            // what the reader asked for.
            Component->OverriddenLightMapRes = Budget;

            Component->PostEditChange();
            ++ChangedCount;
        }

        if (ChangedCount == 0)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: no component on this actor is over the {} lightmap-resolution budget any more — re-scan."),
                InFinding.Target.DisplayName, Budget));
        }

        Actor->MarkPackageDirty();

        return Make_Success(ck::Format_UE(
            TEXT("{}: lightmap resolution clamped to {} on {} component(s) (worst was {})."),
            InFinding.Target.DisplayName, Budget, ChangedCount, WorstBefore), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_DisableBlueprintStartWithTick(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Asset = TryLoad_Asset(InFinding.Target.Path);

        // The finding targets the Blueprint ASSET; the tick flag lives on its generated class's CDO. Both spellings
        // are accepted because a target path can name either depending on how the asset was discovered.
        auto* GeneratedClass = Cast<UBlueprintGeneratedClass>(Asset);

        if (GeneratedClass == nullptr)
        {
            if (const auto* Blueprint = Cast<UBlueprint>(Asset))
            { GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass); }
        }

        if (GeneratedClass == nullptr)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the Blueprint class could not be loaded."),
                InFinding.Target.DisplayName));
        }

        auto* Cdo = Cast<AActor>(GeneratedClass->GetDefaultObject());

        if (ck::Is_NOT_Valid(Cdo))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: this Blueprint is not an Actor, so it has no tick to turn off."),
                InFinding.Target.DisplayName));
        }

        auto& Tick = Cdo->PrimaryActorTick;

        // The check's WHOLE condition — `bCanEverTick && bStartWithTickEnabled` — re-asked. A class that can no
        // longer tick at all is already where the fix would take it, and reporting a change there would be a success
        // message for nothing.
        if (NOT Tick.bCanEverTick)
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: this class can no longer tick at all — nothing to turn off. Re-scan."),
                InFinding.Target.DisplayName));
        }

        if (NOT Tick.bStartWithTickEnabled)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: it already starts with tick disabled — re-scan."),
                InFinding.Target.DisplayName));
        }

        Cdo->Modify();

        // `bStartWithTickEnabled`, never `bCanEverTick`. The class keeps the ABILITY to tick, so anything that
        // enables it deliberately at runtime still works — this only stops it ticking from frame zero. Clearing
        // `bCanEverTick` instead would break `SetActorTickEnabled` and turn a cost fix into a broken actor.
        Tick.bStartWithTickEnabled = false;

        Cdo->PostEditChange();
        GeneratedClass->MarkPackageDirty();

        return Make_Success(ck::Format_UE(
            TEXT("{}: Start With Tick Enabled turned off. The class can still be ticked deliberately — test anything ")
            TEXT("that relied on it ticking from spawn."),
            InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_DeleteEmptyStaticMeshActor(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Actor = TryResolve_Actor(InFinding.Target.Path);

        if (ck::Is_NOT_Valid(Actor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the actor is no longer in a loaded level — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto* MeshActor = Cast<AStaticMeshActor>(Actor);

        if (ck::Is_NOT_Valid(MeshActor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: this is no longer a Static Mesh Actor — nothing deleted."),
                InFinding.Target.DisplayName));
        }

        // Re-validated rather than trusted: the scan may be minutes old, and deleting an actor somebody has since
        // assigned a mesh to is exactly the destructive mistake this check must not make.
        const auto* Component = MeshActor->GetStaticMeshComponent();

        if (Component != nullptr && Component->GetStaticMesh().Get() != nullptr)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: a mesh has been assigned since the scan — nothing deleted."),
                InFinding.Target.DisplayName));
        }

        auto* World = Actor->GetWorld();

        if (ck::Is_NOT_Valid(World))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the actor's world is gone — re-scan."),
                InFinding.Target.DisplayName));
        }

        if (Is_ActorLevelLocked(Actor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: its level is locked — unlock it first. Nothing deleted."),
                InFinding.Target.DisplayName));
        }

        Actor->Modify();

        Deselect_ActorBeforeDestroy(Actor);

        if (NOT World->EditorDestroyActor(Actor, true))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the editor refused to delete the actor."),
                InFinding.Target.DisplayName));
        }

        if (GEditor != nullptr)
        { GEditor->NoteSelectionChange(); }

        return Make_Success(ck::Format_UE(TEXT("{}: empty Static Mesh Actor deleted."),
            InFinding.Target.DisplayName), true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Mesh path plus every resolved material path — two placements sharing this string are interchangeable as far
     *  as one instanced renderer is concerned, which is the only thing that makes a conversion visually neutral. */
    auto
        Build_MaterialSignature(
            const UStaticMeshComponent* InComponent)
        -> FString
    {
        if (InComponent == nullptr)
        { return FString{}; }

        auto Parts = TArray<FString>{};

        const auto MaterialCount = InComponent->GetNumMaterials();

        for (auto Index = 0; Index < MaterialCount; ++Index)
        {
            const auto* Material = InComponent->GetMaterial(Index);
            Parts.Add(Material != nullptr ? Material->GetPathName() : FString{TEXT("none")});
        }

        return FString::Join(Parts, TEXT("|"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether this placement can become an instance without changing what the level DOES. Every clause is a thing
     *  an instanced renderer cannot carry: a subclass may run logic, a non-static component may move, an attachment
     *  in either direction is a relationship an instance has no way to express, and an extra scene component is
     *  behaviour the conversion would silently drop. */
    auto
        Is_ConvertibleToInstance(
            AStaticMeshActor* InActor,
            const UStaticMesh* InMesh)
        -> bool
    {
        if (ck::Is_NOT_Valid(InActor))
        { return false; }

        if (InActor->GetClass() != AStaticMeshActor::StaticClass())
        { return false; }

        const auto* Component = InActor->GetStaticMeshComponent();

        if (Component == nullptr || Component->GetStaticMesh().Get() != InMesh)
        { return false; }

        if (Component->GetMobility() != EComponentMobility::Static)
        { return false; }

        if (Component->GetAttachParent() != nullptr)
        { return false; }

        auto Attached = TArray<AActor*>{};
        InActor->GetAttachedActors(Attached);

        if (NOT Attached.IsEmpty())
        { return false; }

        auto SceneComponents = TInlineComponentArray<USceneComponent*>{};
        InActor->GetComponents(SceneComponents);

        return SceneComponents.Num() == 1;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_ConvertToInstances(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
        if (ck::Is_NOT_Valid(InEditorWorld))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: no editor world to convert placements in."),
                InFinding.Target.DisplayName));
        }

        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        // Grouped by LEVEL as well as by material set: the scan's group may span sub-levels, and one instanced
        // component lives in exactly one of them. Converting the biggest same-level group is the part of the
        // finding this fix can honestly deliver.
        struct FConversionGroup
        {
            ULevel* Level = nullptr;
            FString Key;
            TArray<AStaticMeshActor*> Actors;
        };

        auto Groups = TArray<FConversionGroup>{};
        auto IndexByKey = TMap<FString, int32>{};

        for (auto* Level : InEditorWorld->GetLevels())
        {
            if (Level == nullptr)
            { continue; }

            const auto LevelName = Get_LevelShortName(Level);

            for (AActor* Actor : Level->Actors)
            {
                auto* MeshActor = Cast<AStaticMeshActor>(Actor);

                if (NOT Is_ConvertibleToInstance(MeshActor, Mesh))
                { continue; }

                const auto Key = ck::Format_UE(TEXT("{}|{}"),
                    LevelName, Build_MaterialSignature(MeshActor->GetStaticMeshComponent()));

                if (auto* Found = IndexByKey.Find(Key))
                {
                    Groups[*Found].Actors.Add(MeshActor);
                    continue;
                }

                auto Group = FConversionGroup{};
                Group.Level = Level;
                Group.Key = Key;
                Group.Actors.Add(MeshActor);

                IndexByKey.Add(Key, Groups.Num());
                Groups.Add(MoveTemp(Group));
            }
        }

        // Biggest group wins, with the key as the tie-break so two equal groups are not decided by whichever
        // sub-level happened to load first.
        Groups.Sort([](const FConversionGroup& InLhs, const FConversionGroup& InRhs)
        {
            if (InLhs.Actors.Num() != InRhs.Actors.Num())
            { return InLhs.Actors.Num() > InRhs.Actors.Num(); }

            return InLhs.Key.Compare(InRhs.Key, ESearchCase::CaseSensitive) < 0;
        });

        if (Groups.IsEmpty() || Groups[0].Actors.Num() < 2)
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: fewer than two convertible placements remain — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto& Group = Groups[0];

        // Checked once for the group's level, before anything is spawned. `SpawnActor` with an `OverrideLevel` and
        // `EditorDestroyActor` both bypass the lock that `UEditorEngine::AddActor` and `edactDeleteSelected` respect,
        // so without this a locked sub-level would be rewritten anyway — and this fix spawns BEFORE it deletes, so
        // discovering the problem half way through would leave the level holding an instanced actor and its
        // originals at the same time.
        if (FLevelUtils::IsLevelLocked(Group.Level))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: {} is locked — unlock it first. Nothing was changed."),
                InFinding.Target.DisplayName, Get_LevelShortName(Group.Level)));
        }

        // Deterministic instance order: the transform list a reader compares between two runs must not depend on
        // level iteration order.
        Group.Actors.Sort([](const AStaticMeshActor& InLhs, const AStaticMeshActor& InRhs)
        {
            return InLhs.GetPathName().Compare(InRhs.GetPathName(), ESearchCase::CaseSensitive) < 0;
        });

        const auto* TemplateComponent = Group.Actors[0]->GetStaticMeshComponent();

        auto SpawnParams = FActorSpawnParameters{};
        SpawnParams.OverrideLevel = Group.Level;
        SpawnParams.ObjectFlags = RF_Transactional;

        auto* InstanceActor = InEditorWorld->SpawnActor<AActor>(
            AActor::StaticClass(), FTransform::Identity, SpawnParams);

        if (ck::Is_NOT_Valid(InstanceActor))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: the instanced-mesh actor could not be spawned — nothing was deleted."),
                InFinding.Target.DisplayName));
        }

        auto* InstancedMesh = NewObject<UHierarchicalInstancedStaticMeshComponent>(
            InstanceActor, NAME_None, RF_Transactional);

        InstanceActor->SetRootComponent(InstancedMesh);
        InstanceActor->AddInstanceComponent(InstancedMesh);
        InstancedMesh->OnComponentCreated();
        InstancedMesh->RegisterComponent();

        InstancedMesh->SetStaticMesh(Mesh);

        if (TemplateComponent != nullptr)
        {
            const auto MaterialCount = TemplateComponent->GetNumMaterials();

            for (auto Index = 0; Index < MaterialCount; ++Index)
            { InstancedMesh->SetMaterial(Index, TemplateComponent->GetMaterial(Index)); }
        }

        for (const auto* Converted : Group.Actors)
        { InstancedMesh->AddInstance(Converted->GetActorTransform(), true); }

        InstanceActor->SetActorLabel(ck::Format_UE(TEXT("HISM_{}"), Mesh->GetName()));

        auto DeletedCount = 0;

        for (auto* Converted : Group.Actors)
        {
            Converted->Modify();

            // Dropped from the editor selection first — `EditorDestroyActor` leaves `USelection` holding whatever it
            // destroyed, and this loop can remove dozens of actors at once.
            Deselect_ActorBeforeDestroy(Converted);

            if (InEditorWorld->EditorDestroyActor(Converted, true))
            { ++DeletedCount; }
        }

        if (GEditor != nullptr)
        { GEditor->NoteSelectionChange(); }

        return Make_Success(ck::Format_UE(TEXT("{}: {} placement(s) in {} converted into one hierarchical instanced component ({} original actor(s) deleted)."),
            InFinding.Target.DisplayName,
            Group.Actors.Num(),
            Get_LevelShortName(Group.Level),
            DeletedCount), true);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Lighting "fix" — a review action, not a mutation
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_ReviewMovableLights(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
        if (GEditor == nullptr || ck::Is_NOT_Valid(InEditorWorld))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: no editor world to select lights in."),
                InFinding.Target.DisplayName));
        }

        auto Matched = TArray<AActor*>{};

        for (auto* Level : InEditorWorld->GetLevels())
        {
            if (Level == nullptr)
            { continue; }

            if (Get_LevelShortName(Level) != InFinding.Target.DisplayName)
            { continue; }

            for (AActor* Actor : Level->Actors)
            {
                if (ck::Is_NOT_Valid(Actor))
                { continue; }

                auto Lights = TInlineComponentArray<ULightComponent*>{};
                Actor->GetComponents(Lights);

                const auto HasMovableLight = Lights.ContainsByPredicate([](const ULightComponent* InLight) -> bool
                {
                    return InLight != nullptr && InLight->GetMobility() == EComponentMobility::Movable;
                });

                if (HasMovableLight)
                { Matched.Add(Actor); }
            }
        }

        if (Matched.IsEmpty())
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: no movable-light actor found in that level — re-scan."),
                InFinding.Target.DisplayName));
        }

        Matched.Sort([](const AActor& InLhs, const AActor& InRhs)
        {
            return InLhs.GetPathName().Compare(InRhs.GetPathName(), ESearchCase::CaseSensitive) < 0;
        });

        // Deliberately NOT a mobility change. A light that genuinely moves must stay Movable, and no offline rule
        // can tell which ones those are — so the fix hands the reader the exact set to judge instead of guessing
        // for them.
        GEditor->SelectNone(false, true);

        for (auto* Actor : Matched)
        { GEditor->SelectActor(Actor, true, false); }

        GEditor->NoteSelectionChange();

        return Make_Success(ck::Format_UE(TEXT("{}: selected {} movable-light actor(s) for review — set the ones that never move to Stationary or Static."),
            InFinding.Target.DisplayName, Matched.Num()), false);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Project settings fix — a config write, outside the transaction buffer
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_EnableTextureStreaming(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixResult
    {
        auto* Settings = GetMutableDefault<URendererSettings>();

        if (ck::Is_NOT_Valid(Settings))
        { return Make_Failure(TEXT("The renderer settings object could not be reached.")); }

        if (Settings->bTextureStreaming != 0)
        { return Make_Failure(TEXT("Texture streaming is already enabled — re-scan.")); }

        auto* Property = URendererSettings::StaticClass()->FindPropertyByName(
            GET_MEMBER_NAME_CHECKED(URendererSettings, bTextureStreaming));

        if (Property == nullptr)
        {
            return Make_Failure(TEXT("The Texture Streaming property could not be found on the renderer settings."));
        }

        const auto PreviousValue = Settings->bTextureStreaming;

        Settings->bTextureStreaming = 1;

        // `TryUpdateDefaultConfigFile`, never `UpdateSinglePropertyInConfigFile`: the latter returns `void` and
        // checks nothing, so a `DefaultEngine.ini` that is read-only under source control absorbed the call and this
        // fix reported success over a file it never touched. The CDO would then disagree with the ini until the next
        // editor restart silently reverted it — the worst shape a "fix" can have.
        //
        // The whole-object variant is what carries a result; the property is still passed so the write stays
        // scoped to the one line rather than re-serializing every renderer setting.
        if (NOT Settings->TryUpdateDefaultConfigFile())
        {
            // Rolled back so the running editor and the file on disk keep agreeing. A CDO left saying "enabled" over
            // an ini that says "disabled" is a state nothing in the session would ever correct.
            Settings->bTextureStreaming = PreviousValue;

            return Make_Failure(ck::Format_UE(
                TEXT("{} could not be written — check it out of source control (or clear its read-only flag) and try again. Nothing was changed."),
                Settings->GetDefaultConfigFilename()));
        }

        // The settings editor's own apply path also pushes the value at the console variable the property is bound
        // to. Without this the ini would say one thing and the running editor another until a restart.
        //
        // Read back rather than assumed: `Set` at `ECVF_SetByProjectSetting` is REFUSED when the variable was last
        // set at a higher priority — which is exactly what has happened if the reader typed `r.TextureStreaming 0`
        // at the console, the most likely way to arrive at this finding in the first place.
        auto CvarApplied = true;

        if (auto* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming")))
        {
            ConsoleVariable->Set(1, ECVF_SetByProjectSetting);
            CvarApplied = ConsoleVariable->GetInt() != 0;
        }

        (void)InFinding;

        return Make_Success(CvarApplied
            ? ck::Format_UE(TEXT("Texture Streaming enabled and written to {} — this one is a config write, so Undo cannot reverse it."),
                Settings->GetDefaultConfigFilename())
            : ck::Format_UE(TEXT("Texture Streaming written to {} — but r.TextureStreaming was last set at a higher priority (a console command overrides a project setting), so this session still has it off. Undo cannot reverse the config write."),
                Settings->GetDefaultConfigFilename()),
            true);
    }
#endif

    // ----------------------------------------------------------------------------------------------------------------

    /** The dispatch. No transaction of its own — the single-fix and batch entry points each own the record they
     *  want, and a fix that opened its own inside a batch would split one undo into many. */
    auto
        DoApply_Fix(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
#if WITH_EDITOR
        const auto CheckId = InFinding.CheckId;

        if (CheckId == k_MeshNaniteCandidate)
        { return Apply_NaniteEnabled(InFinding, true); }

        if (CheckId == k_MeshNaniteOnLowPoly)
        { return Apply_NaniteEnabled(InFinding, false); }

        if (CheckId == k_MeshMissingLods)
        { return Apply_GenerateLods(InFinding); }

        if (CheckId == k_MeshComplexCollision)
        { return Apply_SimpleCollision(InFinding); }

        if (CheckId == k_TextureNormalMap)
        { return Apply_NormalMapCompression(InFinding); }

        if (CheckId == k_TextureDataSrgb)
        { return Apply_DisableSrgb(InFinding); }

        if (CheckId == k_TextureMissingMipmaps)
        { return Apply_RestoreMipmaps(InFinding); }

        if (CheckId == k_MeshNaniteMaterial)
        { return Apply_NaniteMaterialUsage(InFinding); }

        if (CheckId == k_LightingLightmapRes)
        { return Apply_ClampLightmapResolution(InFinding); }

        if (CheckId == k_BlueprintTickEnabled)
        { return Apply_DisableBlueprintStartWithTick(InFinding); }

        if (CheckId == k_ActorEmptyStaticMesh)
        { return Apply_DeleteEmptyStaticMeshActor(InFinding); }

        if (CheckId == k_ActorInstancingCandidate)
        { return Apply_ConvertToInstances(InFinding, InEditorWorld); }

        if (CheckId == k_LightingMovableCount)
        { return Apply_ReviewMovableLights(InFinding, InEditorWorld); }

        if (CheckId == k_SettingsTextureStreaming)
        { return Apply_EnableTextureStreaming(InFinding); }

        return Make_Failure(ck::Format_UE(TEXT("{} has no automatic fix."), CheckId));
#else
        // The module ships in packaged Development/DebugGame targets, where there is no transaction buffer, no
        // asset to edit and no editor world. Saying so beats a silent no-op that reads as a successful fix.
        (void)InEditorWorld;

        return Make_Failure(ck::Format_UE(TEXT("{}: applying a fix needs an editor session."),
            InFinding.Target.DisplayName));
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Accumulate(
            FCkOptimizationDebugger_BatchFixResult& InOutBatch,
            const FCkOptimizationDebugger_FixResult& InResult)
        -> void
    {
        if (InResult.Succeeded)
        { ++InOutBatch.SucceededCount; }
        else
        { ++InOutBatch.FailedCount; }

        InOutBatch.ChangedState = InOutBatch.ChangedState || InResult.ChangedState;
        InOutBatch.Messages.Add(InResult.Message);
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_fixes
{
    auto
        Get_AllFixes()
        -> const TArray<FCkOptimizationDebugger_FixInfo>&
    {
        using namespace ck_optimization_debugger_fixes_impl;

        // Built on first use rather than at static-init time: the ids are FNames, and a table that constructs itself
        // before anything asks for it is a table nothing can order against module startup.
        using EExecution = ECkOptimizationDebugger_FixExecution;

        static const auto Fixes = TArray<FCkOptimizationDebugger_FixInfo>{
            FCkOptimizationDebugger_FixInfo{k_MeshNaniteCandidate,  TEXT("Enable Nanite"),              EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_MeshNaniteOnLowPoly,  TEXT("Disable Nanite"),             EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_MeshMissingLods,      TEXT("Generate LODs"),              EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_MeshComplexCollision, TEXT("Generate Simple Collision"),  EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_TextureNormalMap,     TEXT("Fix Normal Map Compression"), EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_TextureDataSrgb,      TEXT("Disable sRGB"),               EExecution::Transactional, false},
            FCkOptimizationDebugger_FixInfo{k_TextureMissingMipmaps, TEXT("Restore Mip Generation"),     EExecution::Transactional, false},

            // Sets `Used With Nanite` on the offending base materials. Not destructive and Undo reverses it — but it
            // invalidates their shader maps, which the result message says because nothing else on screen would.
            FCkOptimizationDebugger_FixInfo{k_MeshNaniteMaterial,    TEXT("Flag Materials For Nanite"),  EExecution::Transactional, false},

            // Clamps the override on every over-budget component of the actor. An actor-component property edit, so
            // it respects the level lock exactly as the two destructive actor fixes do.
            FCkOptimizationDebugger_FixInfo{k_LightingLightmapRes,   TEXT("Clamp Lightmap Resolution"),  EExecution::Transactional, false},

            // Destructive: these two remove actors from the level rather than editing a property on one. That flag is
            // what puts a confirmation in front of a batch containing them.
            FCkOptimizationDebugger_FixInfo{k_ActorEmptyStaticMesh,     TEXT("Delete Empty Actor"),   EExecution::Transactional, true},
            FCkOptimizationDebugger_FixInfo{k_ActorInstancingCandidate, TEXT("Convert To Instances"), EExecution::Transactional, true},

            // Selects the offending lights so the reader can judge each one's mobility. It writes NOTHING, which is
            // why it is `Review` rather than sharing the config-write bucket with an ini edit — and why it runs last,
            // so the selection it leaves is the one the reader is looking at when the batch finishes.
            FCkOptimizationDebugger_FixInfo{k_LightingMovableCount, TEXT("Select Lights For Review"), EExecution::Review, false},

            // The ONLY entry that changes BEHAVIOUR rather than cost, and the reason the fourth flag exists. Undo
            // reverses it like any property edit, so it is not destructive — but a Blueprint that no longer ticks
            // from frame zero looks identical until something it was driving quietly stops happening, which is
            // exactly the surprise a batch of "make it cheaper" fixes must not spring on a reader unannounced.
            FCkOptimizationDebugger_FixInfo{k_BlueprintTickEnabled, TEXT("Disable Start With Tick"),
                EExecution::Transactional, /*IsDestructive*/ false, /*ChangesBehavior*/ true},

            // A config write. Undo does not reach DefaultEngine.ini, which is why this one is separated out of the
            // batch's transaction rather than smuggled inside it.
            FCkOptimizationDebugger_FixInfo{k_SettingsTextureStreaming, TEXT("Enable Texture Streaming"), EExecution::ConfigWrite, false}};

        return Fixes;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_FixInfo(
            FName InCheckId)
        -> const FCkOptimizationDebugger_FixInfo*
    {
        return Get_AllFixes().FindByPredicate([InCheckId](const FCkOptimizationDebugger_FixInfo& InFix) -> bool
        {
            return InFix.CheckId == InCheckId;
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Has_Fix(
            FName InCheckId)
        -> bool
    {
        return TryGet_FixInfo(InCheckId) != nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Can_ApplyFix(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> bool
    {
        return InFinding.HasAutoFix && Has_Fix(InFinding.CheckId);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_FixableFindings(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TArray<FCkOptimizationDebugger_FindingRow>
    {
        auto Fixable = TArray<FCkOptimizationDebugger_FindingRow>{};

        for (const auto& Finding : InFindings)
        {
            if (NOT Can_ApplyFix(Finding))
            { continue; }

            Fixable.Add(Finding);
        }

        return Fixable;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Partition_ForBatch(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> FCkOptimizationDebugger_FixBatchPartition
    {
        auto Partition = FCkOptimizationDebugger_FixBatchPartition{};

        for (const auto& Finding : InFindings)
        {
            if (NOT Can_ApplyFix(Finding))
            { continue; }

            const auto* Info = TryGet_FixInfo(Finding.CheckId);

            if (Info == nullptr)
            { continue; }

            switch (Info->Execution)
            {
                case ECkOptimizationDebugger_FixExecution::Transactional:
                {
                    Partition.Transactional.Add(Finding);
                    break;
                }
                case ECkOptimizationDebugger_FixExecution::ConfigWrite:
                {
                    Partition.ConfigWrite.Add(Finding);
                    break;
                }
                case ECkOptimizationDebugger_FixExecution::Review:
                {
                    Partition.Review.Add(Finding);
                    break;
                }
            }
        }

        return Partition;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_BatchConfirmation(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> FCkOptimizationDebugger_FixConfirmation
    {
        auto Confirmation = FCkOptimizationDebugger_FixConfirmation{};

        auto DestructiveVerbs = TArray<FString>{};
        auto ConfigVerbs = TArray<FString>{};
        auto BehaviorVerbs = TArray<FString>{};

        for (const auto& Finding : InFindings)
        {
            if (NOT Can_ApplyFix(Finding))
            { continue; }

            const auto* Info = TryGet_FixInfo(Finding.CheckId);

            if (Info == nullptr)
            { continue; }

            if (Info->IsDestructive)
            {
                ++Confirmation.DestructiveCount;
                DestructiveVerbs.AddUnique(Info->DisplayVerb);
            }

            if (Info->ChangesBehavior)
            {
                ++Confirmation.BehaviorChangeCount;
                BehaviorVerbs.AddUnique(Info->DisplayVerb);
            }

            if (Info->Execution == ECkOptimizationDebugger_FixExecution::ConfigWrite)
            {
                ++Confirmation.ConfigWriteCount;
                ConfigVerbs.AddUnique(Info->DisplayVerb);
            }
        }

        // A property edit inside a transaction needs no ceremony — Ctrl+Z is the whole answer. The two that do are
        // the ones Ctrl+Z does not cover: actors that stop existing, and a line written into a file the rest of the
        // team shares.
        Confirmation.IsRequired = Confirmation.DestructiveCount > 0
            || Confirmation.ConfigWriteCount > 0
            || Confirmation.BehaviorChangeCount > 0;

        if (NOT Confirmation.IsRequired)
        { return Confirmation; }

        Confirmation.Title = FString{TEXT("Apply optimization fixes?")};

        auto Lines = TArray<FString>{};

        if (Confirmation.DestructiveCount > 0)
        {
            Lines.Add(ck::Format_UE(
                TEXT("{} fix(es) REMOVE or REPLACE actors in the level ({}). Undo reverses them in one step."),
                Confirmation.DestructiveCount,
                FString::Join(DestructiveVerbs, TEXT(", "))));
        }

        if (Confirmation.BehaviorChangeCount > 0)
        {
            // Undo DOES reverse these, and the line says so — the prompt is not a warning about permanence, it is a
            // warning that a batch of COST fixes is about to change what the game does. A reader who applied six
            // "make it cheaper" fixes and got a behaviour change unannounced would stop trusting the button.
            Lines.Add(ck::Format_UE(
                TEXT("{} fix(es) can change how the game BEHAVES, not only what it costs ({}). Undo reverses them, ")
                TEXT("but test the affected assets."),
                Confirmation.BehaviorChangeCount,
                FString::Join(BehaviorVerbs, TEXT(", "))));
        }

        if (Confirmation.ConfigWriteCount > 0)
        {
            // Named separately and last, because this is the half a reader cannot take back. Everything else in the
            // batch is one Ctrl+Z; this one edits a file under source control.
            Lines.Add(ck::Format_UE(
                TEXT("{} fix(es) WRITE A PROJECT CONFIG FILE ({}). Undo cannot reverse that, and the file is shared with the rest of the project."),
                Confirmation.ConfigWriteCount,
                FString::Join(ConfigVerbs, TEXT(", "))));
        }

        Confirmation.Body = FString::Join(Lines, TEXT("\n\n"));

        return Confirmation;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_FixButtonLabel(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFixableFindings)
        -> FString
    {
        if (InFixableFindings.IsEmpty())
        { return FString{TEXT("Apply Fix")}; }

        if (InFixableFindings.Num() == 1)
        {
            const auto* Info = TryGet_FixInfo(InFixableFindings[0].CheckId);

            // The single-selection label names the ACTION, not the count. "Delete Empty Actor" tells the reader
            // what the click does; "Fix 1 Finding" tells them nothing they did not already know.
            return Info != nullptr ? Info->DisplayVerb : FString{TEXT("Apply Fix")};
        }

        return ck::Format_UE(TEXT("Fix {} Findings"), InFixableFindings.Num());
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CanApplyFixes()
        -> bool
    {
        return Get_FixesUnavailableReason().IsEmpty();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_FixesUnavailableReason()
        -> FString
    {
#if WITH_EDITOR
        if (GEditor == nullptr)
        { return FString{TEXT("Applying a fix needs an editor session.")}; }

        // Blocked during PIE for the same reason the cleanup actions are: every transactional fix here edits an
        // asset or destroys an actor, and doing either to a world a play session has running is a change nobody
        // asked for at a moment nothing can undo cleanly. The findings stay readable; only the button waits.
        if (GEditor->PlayWorld != nullptr || GEditor->bIsSimulatingInEditor)
        { return FString{TEXT("Not while a play session is running — stop PIE first.")}; }

        return FString{};
#else
        return FString{TEXT("Applying a fix needs an editor session.")};
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_Fix(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
        using namespace ck_optimization_debugger_fixes_impl;

        if (NOT Get_CanApplyFixes())
        { return Make_Failure(Get_FixesUnavailableReason()); }

        if (NOT Can_ApplyFix(InFinding))
        {
            return Make_Failure(ck::Format_UE(TEXT("{} has no automatic fix."), InFinding.CheckId));
        }

        const auto* Info = TryGet_FixInfo(InFinding.CheckId);

        if (Info == nullptr)
        {
            return Make_Failure(ck::Format_UE(TEXT("{} has no automatic fix."), InFinding.CheckId));
        }

#if WITH_EDITOR
        if (Info->Execution == ECkOptimizationDebugger_FixExecution::Transactional)
        {
            // The per-fix undo label — "Enable Nanite (SM_Foo)" — which is the whole reason this entry point exists
            // separately from the batch one. `Apply_Fixes` labels its record by count, because a batch has no single
            // verb to name it by.
            const auto Transaction = FScopedTransaction{FText::FromString(
                ck::Format_UE(TEXT("{} ({})"), Info->DisplayVerb, InFinding.Target.DisplayName))};

            return DoApply_Fix(InFinding, InEditorWorld);
        }
#endif

        return DoApply_Fix(InFinding, InEditorWorld);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_Fixes(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_BatchFixResult
    {
        using namespace ck_optimization_debugger_fixes_impl;

        auto Batch = FCkOptimizationDebugger_BatchFixResult{};

        if (NOT Get_CanApplyFixes())
        {
            Accumulate(Batch, Make_Failure(Get_FixesUnavailableReason()));
            return Batch;
        }

        const auto Partition = Partition_ForBatch(InFindings);

        if (NOT Partition.Transactional.IsEmpty())
        {
#if WITH_EDITOR
            // ONE record for the whole transactional part: a reader who applied six fixes with one click expects
            // one Ctrl+Z to put the level back, not six.
            const auto Transaction = FScopedTransaction{FText::FromString(
                ck::Format_UE(TEXT("Apply {} optimization fix(es)"), Partition.Transactional.Num()))};

            for (const auto& Finding : Partition.Transactional)
            { Accumulate(Batch, DoApply_Fix(Finding, InEditorWorld)); }
#else
            for (const auto& Finding : Partition.Transactional)
            { Accumulate(Batch, DoApply_Fix(Finding, InEditorWorld)); }
#endif
        }

        // Config writes land AFTER the record and outside it. Undo does not reach a line written to an ini file,
        // and holding one inside the record would promise a Ctrl+Z that silently does nothing.
        for (const auto& Finding : Partition.ConfigWrite)
        { Accumulate(Batch, DoApply_Fix(Finding, InEditorWorld)); }

        // Review actions LAST, and that order is load-bearing: the only one that exists replaces the editor
        // selection, so running it before the transactional part would leave the batch's own actor edits selecting
        // over the top of it. Last means the set it hands the reader is the set still highlighted when it finishes.
        for (const auto& Finding : Partition.Review)
        { Accumulate(Batch, DoApply_Fix(Finding, InEditorWorld)); }

        return Batch;
    }
}

// --------------------------------------------------------------------------------------------------------------------
