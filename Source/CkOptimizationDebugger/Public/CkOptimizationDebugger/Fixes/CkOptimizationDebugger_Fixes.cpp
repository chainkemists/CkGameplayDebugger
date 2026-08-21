#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Fixes.h"

#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPreflight.h"
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPlan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#if WITH_EDITOR
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Mesh.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"

#include "BodySetupEnums.h"
#include "Editor/GroupActor.h"
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

    // ----------------------------------------------------------------------------------------------------------------
    // Plan construction
    //
    // A plan is what a fix WOULD do, computed without doing it, and it is where every fix's re-validation lives.
    // Each planner ends by capturing the objects it resolved into `Plan.Execute` — so the apply never re-resolves,
    // never re-validates, and cannot write a property the plan did not list.
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_FixVerb(
            FName InCheckId)
        -> FString
    {
        const auto* FixInfo = ck_optimization_debugger_fixes::TryGet_FixInfo(InCheckId);

        return FixInfo != nullptr ? FixInfo->DisplayVerb : FString{TEXT("Fix")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_RefusedPlan(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            const FString& InReason)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto Plan = FCkOptimizationDebugger_FixPlan{};
        Plan.Finding = InFinding;
        Plan.FixVerb = Get_FixVerb(InFinding.CheckId);
        Plan.CanApply = false;
        Plan.RefusalReason = InReason;

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Plan(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto Plan = FCkOptimizationDebugger_FixPlan{};
        Plan.Finding = InFinding;
        Plan.FixVerb = Get_FixVerb(InFinding.CheckId);
        Plan.CanApply = true;

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Add_Change(
            FCkOptimizationDebugger_FixPlan& InOutPlan,
            const UObject* InObject,
            const FString& InPropertyLabel,
            const FString& InBeforeText,
            const FString& InAfterText)
        -> void
    {
        auto Change = FCkOptimizationDebugger_PlannedChange{};
        Change.ObjectPath = InObject != nullptr ? FSoftObjectPath{InObject} : FSoftObjectPath{};
        Change.ObjectLabel = InObject != nullptr ? InObject->GetName() : FString{};
        Change.PropertyLabel = InPropertyLabel;
        Change.BeforeText = InBeforeText;
        Change.AfterText = InAfterText;

        InOutPlan.Changes.Add(MoveTemp(Change));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Add_Effect(
            FCkOptimizationDebugger_FixPlan& InOutPlan,
            const FString& InDescription,
            bool InIsRisk)
        -> void
    {
        auto Effect = FCkOptimizationDebugger_PlannedEffect{};
        Effect.Description = InDescription;
        Effect.IsRisk = InIsRisk;

        InOutPlan.Effects.Add(MoveTemp(Effect));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_OnOff(
            bool InValue)
        -> FString
    {
        return InValue ? FString{TEXT("On")} : FString{TEXT("Off")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether the reader left this change ticked. Asked by every write, so a cleared tick is honoured by the code
     *  that writes rather than by the code that draws the row. */
    auto
        Is_Ticked(
            const FCkOptimizationDebugger_FixPlan& InPlan,
            const UObject* InObject,
            const FString& InPropertyLabel)
        -> bool
    {
        return ck_optimization_debugger_fixplan::Get_IsChangeIncluded(InPlan,
            InObject != nullptr ? FSoftObjectPath{InObject} : FSoftObjectPath{},
            InPropertyLabel);
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
        Plan_NaniteEnabled(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            bool InEnabled)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        const auto StateWord = FString{InEnabled ? TEXT("enabled") : TEXT("disabled")};

        if (Mesh->IsNaniteEnabled() == InEnabled)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: Nanite is already {} — nothing to do."),
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
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: now {} triangles, under the {}-triangle Nanite floor — re-scan."),
                InFinding.Target.DisplayName, TriangleCount, Thresholds.MinTrianglesForNanite));
        }

        if (NOT InEnabled && TriangleCount >= Thresholds.MaxTrianglesForNaniteWarning)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: now {} triangles, at or over the {}-triangle low-poly floor — Nanite left on. Re-scan."),
                InFinding.Target.DisplayName, TriangleCount, Thresholds.MaxTrianglesForNaniteWarning));
        }

        // A material that animates vertices is the one case where turning Nanite ON is not cost-neutral: World
        // Position Offset is evaluated differently and stops being evaluated past a distance, so wind, panners and
        // any displaced geometry change on screen. Refused rather than warned — a warning inside a batch
        // confirmation is a warning the reader learns to dismiss, and nobody connects "the wind stopped" back to a
        // checkbox they ticked last week. Only ever asked when ENABLING; turning Nanite off restores the old path.
        if (InEnabled)
        {
            auto WpoMaterialNames = TArray<FString>{};

            switch (ck_optimization_debugger_preflight::Get_WorldPositionOffsetAnswer(Mesh, WpoMaterialNames))
            {
                case ECkOptimizationDebugger_WpoAnswer::UsesWorldPositionOffset:
                {
                    return Make_RefusedPlan(InFinding, ck::Format_UE(
                        TEXT("{}: left alone — {} of its material(s) animate vertices with World Position Offset ({}). ")
                        TEXT("Nanite evaluates that differently and stops past a distance, so enable it by hand once ")
                        TEXT("you have looked at the result."),
                        InFinding.Target.DisplayName,
                        WpoMaterialNames.Num(),
                        FString::Join(WpoMaterialNames, TEXT(", "))));
                }
                case ECkOptimizationDebugger_WpoAnswer::Unknown:
                {
                    return Make_RefusedPlan(InFinding, ck::Format_UE(
                        TEXT("{}: left alone — a slot material carries no cached expression data, so whether it uses ")
                        TEXT("World Position Offset could not be answered. Open the material and enable Nanite by hand."),
                        InFinding.Target.DisplayName));
                }
                default: break;
            }
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Mesh, TEXT("Nanite Enabled"), Format_OnOff(NOT InEnabled), Format_OnOff(InEnabled));
        Add_Effect(Plan, InEnabled
            ? ck::Format_UE(TEXT("Builds Nanite data for {} triangles"), TriangleCount)
            : FString{TEXT("Drops the mesh's built Nanite data")}, false);

        Plan.Execute = [Mesh, InEnabled, StateWord](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Mesh, TEXT("Nanite Enabled")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Mesh->Modify();

            auto Settings = Mesh->GetNaniteSettings();
            Settings.bEnabled = InEnabled;
            Mesh->SetNaniteSettings(Settings);

            // The same PostEditChangeProperty the mesh editor's own Nanite checkbox raises — and the thing that
            // actually builds or drops the Nanite data. Setting the struct alone would leave the flag saying one
            // thing and the built data saying another.
            Mesh->NotifyNaniteSettingsChanged();
            Mesh->MarkPackageDirty();

            return Make_Success(ck::Format_UE(TEXT("{}: Nanite {}."), DisplayName, StateWord), true);
        };

        return Plan;
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
        Plan_GenerateLods(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Mesh->GetNumLODs() > 1)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the mesh already has {} LODs — re-scan."),
                InFinding.Target.DisplayName, Mesh->GetNumLODs()));
        }

        const auto Group = Get_DefaultLodGroup();

        if (Group.IsNone())
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: this platform has no configured static-mesh LOD groups, so there is no reduction setup to apply."),
                InFinding.Target.DisplayName));
        }

        const auto PreviousGroup = Mesh->GetLODGroup();

        if (PreviousGroup == Group)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: already assigned to the {} LOD group yet still has one LOD — generate the chain in the Static Mesh editor."),
                InFinding.Target.DisplayName, Group));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Mesh, TEXT("LOD Group"),
            PreviousGroup.IsNone() ? FString{TEXT("None")} : PreviousGroup.ToString(), Group.ToString());
        Add_Effect(Plan, TEXT("Applies the group's LOD count and per-LOD reduction settings, and rebuilds the mesh"), false);

        Plan.Execute = [Mesh, Group](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Mesh, TEXT("LOD Group")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Mesh->Modify();

            // `SetLODGroup` is the engine's own path: it applies the group's default LOD count and per-LOD reduction
            // settings and rebuilds. Driving the reduction interface directly from here would be a second, worse
            // copy of that rule — and the mesh-editor tooling that owns it is not reachable from a non-editor module.
            Mesh->SetLODGroup(Group);
            Mesh->MarkPackageDirty();

            return Make_Success(ck::Format_UE(TEXT("{}: assigned to the {} LOD group, which generated its LOD chain from the group's reduction settings."),
                DisplayName, Group), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_SimpleCollision(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        auto* BodySetup = Mesh->GetBodySetup();

        if (ck::Is_NOT_Valid(BodySetup))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the mesh has no body setup to change."),
                InFinding.Target.DisplayName));
        }

        if (BodySetup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: collision complexity is no longer Use Complex As Simple — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Mesh, TEXT("Collision Complexity"),
            TEXT("Use Complex As Simple"), TEXT("Use Simple And Complex"));

        // A mesh with the flag flipped and NO simple primitives would collide with nothing at all — a silent
        // behaviour change far worse than the cost the finding was about. The box is a SECOND change rather than a
        // silent side effect of the first, so a reader who has a hull in mind can untick it and keep the flag.
        const auto NeedsBox = BodySetup->AggGeom.GetElementCount() == 0
            && NOT Mesh->GetBounds().BoxExtent.IsNearlyZero();

        if (NeedsBox)
        {
            Add_Change(Plan, Mesh, TEXT("Simple Collision"), TEXT("none"), TEXT("1 box from the mesh bounds"));
            Add_Effect(Plan, TEXT("The box is a stand-in, not a fitted hull — replace it when you can"), false);
        }

        Plan.Execute = [Mesh, BodySetup, NeedsBox](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Mesh, TEXT("Collision Complexity")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            const auto AddBox = NeedsBox && Is_Ticked(InPlan, Mesh, TEXT("Simple Collision"));

            // Refused rather than half-applied. Flipping the flag with no simple primitives leaves a mesh that
            // collides with nothing, so unticking the box while keeping the flag is a combination that must not be
            // written — and saying why beats writing it and mentioning the consequence afterwards.
            if (NeedsBox && NOT AddBox)
            {
                return Make_Failure(ck::Format_UE(
                    TEXT("{}: this mesh has no simple collision, so flipping the flag without adding a primitive ")
                    TEXT("would leave it colliding with nothing. Tick the box primitive too, or add a hull by hand."),
                    DisplayName));
            }

            Mesh->Modify();
            BodySetup->Modify();

            if (AddBox)
            {
                const auto Bounds = Mesh->GetBounds();

                auto BoxElem = FKBoxElem{};
                BoxElem.Center = Bounds.Origin;
                BoxElem.X = static_cast<float>(Bounds.BoxExtent.X * 2.0);
                BoxElem.Y = static_cast<float>(Bounds.BoxExtent.Y * 2.0);
                BoxElem.Z = static_cast<float>(Bounds.BoxExtent.Z * 2.0);

                BodySetup->AggGeom.BoxElems.Add(BoxElem);
            }

            BodySetup->CollisionTraceFlag = CTF_UseSimpleAndComplex;

            BodySetup->InvalidatePhysicsData();
            BodySetup->CreatePhysicsMeshes();

            Mesh->PostEditChange();
            Mesh->MarkPackageDirty();

            return Make_Success(AddBox
                ? ck::Format_UE(TEXT("{}: collision complexity set to Simple And Complex, with a box primitive added from the mesh bounds — replace it with a fitted hull when you can."),
                    DisplayName)
                : ck::Format_UE(TEXT("{}: collision complexity set to Simple And Complex over the existing simple primitives."),
                    DisplayName), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Texture fixes
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_NormalMapCompression(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->CompressionSettings == TC_Normalmap && Texture->SRGB == 0)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: already compressed as a normal map — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        const auto CompressionEnum = StaticEnum<TextureCompressionSettings>();

        if (Texture->CompressionSettings != TC_Normalmap)
        {
            Add_Change(Plan, Texture, TEXT("Compression Settings"),
                CompressionEnum != nullptr
                    ? CompressionEnum->GetNameStringByValue(static_cast<int64>(Texture->CompressionSettings.GetValue()))
                    : FString{TEXT("?")},
                TEXT("TC_Normalmap"));
        }

        // A normal map is not colour data. Leaving sRGB on would apply a gamma curve to vectors, which is the same
        // defect the compression setting is being fixed for — but it is a SECOND property, so it gets its own row
        // and its own tick rather than riding along invisibly.
        if (Texture->SRGB != 0)
        { Add_Change(Plan, Texture, TEXT("sRGB"), Format_OnOff(true), Format_OnOff(false)); }

        Plan.Execute = [Texture](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            const auto WriteCompression = Is_Ticked(InPlan, Texture, TEXT("Compression Settings"));
            const auto WriteSrgb = Is_Ticked(InPlan, Texture, TEXT("sRGB"));

            if (NOT WriteCompression && NOT WriteSrgb)
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Texture->Modify();

            if (WriteCompression)
            { Texture->CompressionSettings = TC_Normalmap; }

            if (WriteSrgb)
            { Texture->SRGB = 0; }

            Texture->PostEditChange();
            Texture->MarkPackageDirty();

            return Make_Success(WriteCompression && WriteSrgb
                ? ck::Format_UE(TEXT("{}: compression set to Normalmap and sRGB turned off."), DisplayName)
                : ck::Format_UE(TEXT("{}: {}."), DisplayName,
                    WriteCompression ? TEXT("compression set to Normalmap") : TEXT("sRGB turned off")), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_DisableSrgb(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->SRGB == 0)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: sRGB is already off — re-scan."),
                InFinding.Target.DisplayName));
        }

        // The OTHER half of the check's condition, re-asked through the check's own predicate rather than a second
        // copy of the rule. `Texture.DataTextureSrgb` fires on "sRGB on AND this reads as packed data"; a texture
        // re-authored as an albedo since the scan is no longer a data texture, and stripping sRGB off a colour map
        // would be a silent visual regression reported as a successful fix.
        if (NOT ck_optimization_debugger_checks_texture::Is_DataTexture(Texture, Texture->GetName()))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: this no longer reads as a data texture — its compression settings or name have changed since the scan. sRGB left on; re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Texture, TEXT("sRGB"), Format_OnOff(true), Format_OnOff(false));

        Plan.Execute = [Texture](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Texture, TEXT("sRGB")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Texture->Modify();

            Texture->SRGB = 0;

            Texture->PostEditChange();
            Texture->MarkPackageDirty();

            return Make_Success(ck::Format_UE(TEXT("{}: sRGB turned off."), DisplayName), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_RestoreMipmaps(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Texture = Cast<UTexture>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Texture))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the texture could not be loaded."),
                InFinding.Target.DisplayName));
        }

        if (Texture->MipGenSettings != TMGS_NoMipmaps)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: mip generation is already on — re-scan."),
                InFinding.Target.DisplayName));
        }

        // The check's OTHER half. `Texture.MissingMipmaps` fires on "no mips AND not in the UI group", and a texture
        // moved into the UI group since the scan is legitimately mipless — generating mips for it would undo a
        // deliberate authoring decision and report it as a fix.
        if (Texture->LODGroup == TEXTUREGROUP_UI)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: this is now in the UI texture group, where shipping without mips is correct. Left alone; re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Texture, TEXT("Mip Gen Settings"), TEXT("NoMipmaps"), TEXT("FromTextureGroup"));
        Add_Effect(Plan, TEXT("Rebuilds the texture with a mip chain"), false);

        Plan.Execute = [Texture](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Texture, TEXT("Mip Gen Settings")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Texture->Modify();

            // `FromTextureGroup` rather than a specific setting: the group is where a project states its mip policy,
            // so this hands the decision back to that policy instead of this tool inventing one per texture.
            Texture->MipGenSettings = TMGS_FromTextureGroup;

            Texture->PostEditChange();
            Texture->MarkPackageDirty();

            return Make_Success(ck::Format_UE(
                TEXT("{}: mip generation set to FromTextureGroup — the texture rebuilds with mips."),
                DisplayName), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_NaniteMaterialUsage(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the mesh could not be loaded."),
                InFinding.Target.DisplayName));
        }

        // The check fires on "Nanite is ON and some slot's material does not declare the usage". A mesh whose Nanite
        // was turned off since the scan has nothing to fix here — and flagging its materials anyway would compile
        // shaders for a claim nothing is making.
        if (NOT Mesh->GetNaniteSettings().bEnabled)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: Nanite is no longer enabled on this mesh, so its materials do not need the usage flag. Re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);
        auto BaseMaterials = TArray<UMaterial*>{};

        for (const auto& StaticMaterial : Mesh->GetStaticMaterials())
        {
            // The check's own predicate, exported rather than copied — a second spelling of "is this incompatible"
            // in the fix is a second place for it to drift from what the list reported.
            if (NOT ck_optimization_debugger_checks_mesh::Is_NaniteIncompatible(StaticMaterial.MaterialInterface))
            { continue; }

            auto* BaseMaterial = StaticMaterial.MaterialInterface->GetMaterial();

            if (BaseMaterial == nullptr || BaseMaterials.Contains(BaseMaterial))
            { continue; }

            BaseMaterials.Add(BaseMaterial);

            // One row PER MATERIAL, because this fix edits SHARED assets and each one has its own blast radius. A
            // reader who is happy to flag one parent and not another can now say so.
            Add_Change(Plan, BaseMaterial, TEXT("Used With Nanite"), Format_OnOff(false), Format_OnOff(true));

            const auto ReferencingCount = ck_optimization_debugger_preflight::Get_ReferencingPackageCount(
                FSoftObjectPath{BaseMaterial});

            if (ReferencingCount > 0)
            {
                Add_Effect(Plan, ck::Format_UE(TEXT("{} is also used by {} other package(s) — all of them inherit this"),
                    BaseMaterial->GetName(), ReferencingCount), true);
            }
        }

        if (BaseMaterials.IsEmpty())
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: every material on this mesh already declares Used With Nanite — re-scan."),
                InFinding.Target.DisplayName));
        }

        Add_Effect(Plan, TEXT("Queues a shader compile"), false);

        Plan.Execute = [BaseMaterials](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            auto ChangedNames = TArray<FString>{};

            for (auto* BaseMaterial : BaseMaterials)
            {
                if (NOT Is_Ticked(InPlan, BaseMaterial, TEXT("Used With Nanite")))
                { continue; }

                // The BASE material carries the usage flag, so a mesh using several instances of one parent is
                // fixed once.
                BaseMaterial->Modify();

                BaseMaterial->bUsedWithNanite = 1;

                BaseMaterial->PostEditChange();
                BaseMaterial->MarkPackageDirty();

                ChangedNames.Add(BaseMaterial->GetName());
            }

            if (ChangedNames.IsEmpty())
            { return Make_Failure(ck::Format_UE(TEXT("{}: every material left unticked — nothing written."), DisplayName)); }

            // Said out loud, because it is the cost the reader is about to pay and nothing else on screen would
            // tell them: setting the usage flag invalidates the material's shader map and queues a compile.
            return Make_Success(ck::Format_UE(
                TEXT("{}: Used With Nanite set on {} material(s) ({}). This queues a shader compile."),
                DisplayName, ChangedNames.Num(), FString::Join(ChangedNames, TEXT(", "))), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Actor fixes
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_ClampLightmapResolution(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Actor = TryResolve_Actor(InFinding.Target.Path);

        if (ck::Is_NOT_Valid(Actor))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the actor is no longer in a loaded level — re-scan."),
                InFinding.Target.DisplayName));
        }

        // Read fresh rather than carried on the finding: a threshold the reader tightened after the scan is the one
        // they mean now, and clamping to a stale number would write a value the current settings still flag.
        const auto Thresholds = ck_optimization_debugger_thresholds::Build_FromSettings();
        const auto Budget = Thresholds.MaxLightmapResolution;

        if (Budget <= 0)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: the lightmap-resolution budget is not a positive number, so there is nothing to clamp to."),
                InFinding.Target.DisplayName));
        }

        // The level lock is checked BEFORE anything is planned, exactly as the two destructive actor fixes do:
        // `Modify` on a component in a locked level would rewrite a level the editor is protecting.
        if (Is_ActorLevelLocked(Actor))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: its level is locked — unlock it first. Nothing changed."),
                InFinding.Target.DisplayName));
        }

        auto Components = TArray<UStaticMeshComponent*>{};
        Actor->GetComponents<UStaticMeshComponent>(Components);

        auto Plan = Make_Plan(InFinding);
        auto OverBudget = TArray<UStaticMeshComponent*>{};

        // EVERY over-budget component on the actor, not the first. The check aggregates per actor precisely because
        // one actor can carry several over-budget overrides, so a fix that clamped one of them would leave a finding
        // the reader watched "fix" and then reappear. One row each, so each can be judged on its own.
        for (auto* Component : Components)
        {
            if (ck::Is_NOT_Valid(Component))
            { continue; }

            if (NOT Component->bOverrideLightMapRes)
            { continue; }

            if (Component->OverriddenLightMapRes <= Budget)
            { continue; }

            OverBudget.Add(Component);

            Add_Change(Plan, Component, TEXT("Overridden Light Map Res"),
                FString::FromInt(Component->OverriddenLightMapRes), FString::FromInt(Budget));
        }

        if (OverBudget.IsEmpty())
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: no component on this actor is over the {} lightmap-resolution budget any more — re-scan."),
                InFinding.Target.DisplayName, Budget));
        }

        Plan.Execute = [Actor, OverBudget, Budget](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            auto ChangedCount = 0;
            auto WorstBefore = 0;

            for (auto* Component : OverBudget)
            {
                if (NOT Is_Ticked(InPlan, Component, TEXT("Overridden Light Map Res")))
                { continue; }

                WorstBefore = FMath::Max(WorstBefore, Component->OverriddenLightMapRes);

                Component->Modify();

                // Clamped to the budget rather than the override cleared: clearing falls back to the mesh's own
                // default, which is a DIFFERENT number nobody chose and may be higher than the budget too.
                Component->OverriddenLightMapRes = Budget;

                Component->PostEditChange();
                ++ChangedCount;
            }

            if (ChangedCount == 0)
            { return Make_Failure(ck::Format_UE(TEXT("{}: every component left unticked — nothing written."), DisplayName)); }

            Actor->MarkPackageDirty();

            return Make_Success(ck::Format_UE(
                TEXT("{}: lightmap resolution clamped to {} on {} component(s) (worst was {})."),
                DisplayName, Budget, ChangedCount, WorstBefore), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_DisableBlueprintStartWithTick(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
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
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the Blueprint class could not be loaded."),
                InFinding.Target.DisplayName));
        }

        auto* Cdo = Cast<AActor>(GeneratedClass->GetDefaultObject());

        if (ck::Is_NOT_Valid(Cdo))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: this Blueprint is not an Actor, so it has no tick to turn off."),
                InFinding.Target.DisplayName));
        }

        const auto& Tick = Cdo->PrimaryActorTick;

        // The check's WHOLE condition — `bCanEverTick && bStartWithTickEnabled` — re-asked. A class that can no
        // longer tick at all is already where the fix would take it, and reporting a change there would be a success
        // message for nothing.
        if (NOT Tick.bCanEverTick)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(
                TEXT("{}: this class can no longer tick at all — nothing to turn off. Re-scan."),
                InFinding.Target.DisplayName));
        }

        if (NOT Tick.bStartWithTickEnabled)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: it already starts with tick disabled — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Cdo, TEXT("Start With Tick Enabled"), Format_OnOff(true), Format_OnOff(false));
        Add_Effect(Plan, TEXT("Changes BEHAVIOUR: instances stop ticking from spawn. Can Ever Tick is left alone, so anything that enables tick deliberately still works"), true);

        Plan.Execute = [Cdo, GeneratedClass](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            if (NOT Is_Ticked(InPlan, Cdo, TEXT("Start With Tick Enabled")))
            { return Make_Failure(ck::Format_UE(TEXT("{}: left unticked — nothing written."), DisplayName)); }

            Cdo->Modify();

            // `bStartWithTickEnabled`, never `bCanEverTick`. The class keeps the ABILITY to tick, so anything that
            // enables it deliberately at runtime still works — this only stops it ticking from frame zero.
            // Clearing `bCanEverTick` instead would break `SetActorTickEnabled` and turn a cost fix into a broken actor.
            Cdo->PrimaryActorTick.bStartWithTickEnabled = false;

            Cdo->PostEditChange();
            GeneratedClass->MarkPackageDirty();

            return Make_Success(ck::Format_UE(
                TEXT("{}: Start With Tick Enabled turned off. The class can still be ticked deliberately — test anything ")
                TEXT("that relied on it ticking from spawn."),
                DisplayName), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_DeleteEmptyStaticMeshActor(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Actor = TryResolve_Actor(InFinding.Target.Path);

        if (ck::Is_NOT_Valid(Actor))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the actor is no longer in a loaded level — re-scan."),
                InFinding.Target.DisplayName));
        }

        auto* MeshActor = Cast<AStaticMeshActor>(Actor);

        if (ck::Is_NOT_Valid(MeshActor))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: this is no longer a Static Mesh Actor — nothing deleted."),
                InFinding.Target.DisplayName));
        }

        // Re-validated rather than trusted: the scan may be minutes old, and deleting an actor somebody has since
        // assigned a mesh to is exactly the destructive mistake this check must not make.
        const auto* Component = MeshActor->GetStaticMeshComponent();

        if (Component != nullptr && Component->GetStaticMesh().Get() != nullptr)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: a mesh has been assigned since the scan — nothing deleted."),
                InFinding.Target.DisplayName));
        }

        auto* World = Actor->GetWorld();

        if (ck::Is_NOT_Valid(World))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the actor's world is gone — re-scan."),
                InFinding.Target.DisplayName));
        }

        if (Is_ActorLevelLocked(Actor))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: its level is locked — unlock it first. Nothing deleted."),
                InFinding.Target.DisplayName));
        }

        auto Plan = Make_Plan(InFinding);

        Add_Effect(Plan, ck::Format_UE(TEXT("Deletes the actor {}"), Actor->GetActorNameOrLabel()), true);

        // Named in the PREVIEW, not only afterwards: a level designer whose Group is about to lose a member should
        // be told before it happens, not in a status line after.
        if (const auto* GroupActor = Cast<AGroupActor>(Actor->GroupActor))
        {
            Add_Effect(Plan, ck::Format_UE(TEXT("Removes it from the Group [{}] first"),
                GroupActor->GetActorNameOrLabel()), true);
        }

        Plan.Execute = [Actor, World](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;

            Actor->Modify();

            // Removed from its editor Group BEFORE it is destroyed, exactly as `UEditorEngine::edactDeleteSelected`
            // does (`EditorActor.cpp`, `GetParentForActor` then `Remove`). `EditorDestroyActor` performs no such
            // removal, so without this the surviving group keeps a reference to a destroyed member — the same class
            // of omission as the level-lock check, and for the same reason: this path deletes a NAMED actor rather
            // than the selection, so every guarantee the engine's path was making has to be made here instead.
            const auto DetachedFromGroup = ck_optimization_debugger_preflight::Detach_FromEditorGroup(Actor);

            Deselect_ActorBeforeDestroy(Actor);

            if (NOT World->EditorDestroyActor(Actor, true))
            {
                return Make_Failure(ck::Format_UE(TEXT("{}: the editor refused to delete the actor."), DisplayName));
            }

            if (GEditor != nullptr)
            { GEditor->NoteSelectionChange(); }

            return Make_Success(DetachedFromGroup.IsEmpty()
                ? ck::Format_UE(TEXT("{}: empty Static Mesh Actor deleted."), DisplayName)
                : ck::Format_UE(TEXT("{}: empty Static Mesh Actor deleted, and removed from the Group [{}] first."),
                    DisplayName, DetachedFromGroup), true);
        };

        return Plan;
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

    /** The CANDIDATE set — every plain placement of this mesh, and deliberately nothing more.
     *
     *  This used to be the whole gate, answering yes or no over five clauses, and every placement it turned down
     *  vanished from the reader's view without a word. Worse, the five clauses were all about the SHAPE of a
     *  placement and none about what it CARRIES, so a placement with gameplay tags, an editor Group, a Data Layer or
     *  per-placement material state was converted and its state discarded in silence. Judging is now
     *  `Audit_Placement`'s job, which answers with reasons; this decides only who gets asked. */
    auto
        Is_ConversionCandidate(
            const AStaticMeshActor* InActor,
            const UStaticMesh* InMesh)
        -> bool
    {
        if (ck::Is_NOT_Valid(InActor))
        { return false; }

        if (InActor->GetClass() != AStaticMeshActor::StaticClass())
        { return false; }

        const auto* Component = InActor->GetStaticMeshComponent();

        return Component != nullptr && Component->GetStaticMesh().Get() == InMesh;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_ConvertToInstances(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixPlan
    {
        if (ck::Is_NOT_Valid(InEditorWorld))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: no editor world to convert placements in."),
                InFinding.Target.DisplayName));
        }

        auto* Mesh = Cast<UStaticMesh>(TryLoad_Asset(InFinding.Target.Path));

        if (ck::Is_NOT_Valid(Mesh))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: the static mesh could not be loaded."),
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

                if (NOT Is_ConversionCandidate(MeshActor, Mesh))
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

        if (Groups.IsEmpty())
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: no placement of this mesh remains — re-scan."),
                InFinding.Target.DisplayName));
        }

        // ---- The audit, in two passes, over every group ----
        //
        // Pass 1 asks each candidate the ABSOLUTE questions — the ones whose answer does not depend on any other
        // placement (tags, Group membership, Data Layers, HLOD, a dynamic material instance, an inbound reference,
        // a locked level). A placement that fails one is out whatever the rest look like, and the template has to
        // clear the same bar before it can BE one. Pass 2 then compares every survivor against that template,
        // property by property through reflection, so a placement differing in custom primitive data, a stencil
        // value, a shadow flag or a draw distance is REFUSED and named rather than silently rewritten to the
        // template's value — which is what the old five-clause gate did, without saying so.
        //
        // The level lock is one of those refusal reasons rather than a separate pre-check: a group whose level is
        // locked yields no convertible placements, so it cannot win below, and the reason reaches the reader in the
        // same sentence as every other one instead of pre-empting them.
        const auto ReferenceContext = ck_optimization_debugger_preflight::Build_ReferenceContext(InEditorWorld);

        struct FAuditedGroup
        {
            const FConversionGroup* Source = nullptr;
            TArray<AStaticMeshActor*> Convertible;
            TArray<FCkOptimizationDebugger_PlacementAudit> Audits;
        };

        auto AuditedGroups = TArray<FAuditedGroup>{};
        AuditedGroups.Reserve(Groups.Num());

        for (auto& Candidate : Groups)
        {
            // Deterministic instance order: the transform list a reader compares between two runs must not depend
            // on level iteration order. It also fixes WHICH placement becomes the template.
            Candidate.Actors.Sort([](const AStaticMeshActor& InLhs, const AStaticMeshActor& InRhs)
            {
                return InLhs.GetPathName().Compare(InRhs.GetPathName(), ESearchCase::CaseSensitive) < 0;
            });

            auto Audited = FAuditedGroup{};
            Audited.Source = &Candidate;

            auto SelfCleared = TArray<AStaticMeshActor*>{};

            for (auto* Placement : Candidate.Actors)
            {
                auto Audit = ck_optimization_debugger_preflight::Audit_Placement(
                    Placement, Placement, Mesh, ReferenceContext);

                if (Audit.Get_IsConvertible())
                {
                    SelfCleared.Add(Placement);
                    continue;
                }

                Audited.Audits.Add(MoveTemp(Audit));
            }

            if (NOT SelfCleared.IsEmpty())
            {
                auto* TemplatePlacement = SelfCleared[0];

                for (auto* Placement : SelfCleared)
                {
                    auto Audit = ck_optimization_debugger_preflight::Audit_Placement(
                        Placement, TemplatePlacement, Mesh, ReferenceContext);

                    if (Audit.Get_IsConvertible())
                    { Audited.Convertible.Add(Placement); }

                    Audited.Audits.Add(MoveTemp(Audit));
                }
            }

            AuditedGroups.Add(MoveTemp(Audited));
        }

        // Ranked by what is actually CONVERTIBLE, not by how many candidates a group started with: a group of forty
        // where thirty-eight carry tags is worth less than a clean group of ten, and candidate count would have
        // picked the first and then converted two placements.
        AuditedGroups.Sort([](const FAuditedGroup& InLhs, const FAuditedGroup& InRhs)
        {
            if (InLhs.Convertible.Num() != InRhs.Convertible.Num())
            { return InLhs.Convertible.Num() > InRhs.Convertible.Num(); }

            return InLhs.Source->Key.Compare(InRhs.Source->Key, ESearchCase::CaseSensitive) < 0;
        });

        const auto& Winner = AuditedGroups[0];
        const auto RefusalSummary = ck_optimization_debugger_preflight::Build_RefusalSummary(Winner.Audits);

        if (Winner.Convertible.Num() < 2)
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: nothing was converted. {}"),
                InFinding.Target.DisplayName, RefusalSummary));
        }

        auto ConvertibleActors = Winner.Convertible;
        auto* GroupLevel = Winner.Source->Level;
        const auto LevelName = Get_LevelShortName(GroupLevel);

        auto Plan = Make_Plan(InFinding);

        Add_Effect(Plan, ck::Format_UE(TEXT("Converts {} placement(s) in {} into one hierarchical instanced component"),
            ConvertibleActors.Num(), LevelName), false);
        Add_Effect(Plan, ck::Format_UE(TEXT("Deletes the {} original actor(s)"), ConvertibleActors.Num()), true);

        // What is being LEFT ALONE is part of the preview, not an afterthought in the result message. A reader who
        // expected forty and is being offered twenty-eight needs the reason before they press, not after.
        if (NOT RefusalSummary.IsEmpty())
        { Add_Effect(Plan, RefusalSummary, false); }

        Plan.Execute = [Mesh, InEditorWorld, GroupLevel, LevelName, ConvertibleActors, RefusalSummary]
            (const FCkOptimizationDebugger_FixPlan& InPlan) -> FCkOptimizationDebugger_FixResult
        {
            const auto DisplayName = InPlan.Finding.Target.DisplayName;
            const auto* TemplateComponent = ConvertibleActors[0]->GetStaticMeshComponent();

            auto SpawnParams = FActorSpawnParameters{};
            SpawnParams.OverrideLevel = GroupLevel;
            SpawnParams.ObjectFlags = RF_Transactional;

            auto* InstanceActor = InEditorWorld->SpawnActor<AActor>(
                AActor::StaticClass(), FTransform::Identity, SpawnParams);

            if (ck::Is_NOT_Valid(InstanceActor))
            {
                return Make_Failure(ck::Format_UE(TEXT("{}: the instanced-mesh actor could not be spawned — nothing was deleted."),
                    DisplayName));
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

            for (const auto* Converted : ConvertibleActors)
            { InstancedMesh->AddInstance(Converted->GetActorTransform(), true); }

            InstanceActor->SetActorLabel(ck::Format_UE(TEXT("HISM_{}"), Mesh->GetName()));

            auto DeletedCount = 0;

            for (auto* Converted : ConvertibleActors)
            {
                Converted->Modify();

                // Dropped from the editor selection first — `EditorDestroyActor` leaves `USelection` holding
                // whatever it destroyed, and this loop can remove dozens of actors at once.
                Deselect_ActorBeforeDestroy(Converted);

                if (InEditorWorld->EditorDestroyActor(Converted, true))
                { ++DeletedCount; }
            }

            if (GEditor != nullptr)
            { GEditor->NoteSelectionChange(); }

            // The refusal summary rides the SUCCESS message, not just the failure one. "40 candidates, 28
            // converted" is the sentence a reader needs in order to trust the twelve that were left alone.
            return Make_Success(RefusalSummary.IsEmpty()
                ? ck::Format_UE(TEXT("{}: {} placement(s) in {} converted into one hierarchical instanced component ({} original actor(s) deleted)."),
                    DisplayName, ConvertibleActors.Num(), LevelName, DeletedCount)
                : ck::Format_UE(TEXT("{}: {} placement(s) in {} converted into one hierarchical instanced component ({} original actor(s) deleted). {}"),
                    DisplayName, ConvertibleActors.Num(), LevelName, DeletedCount, RefusalSummary), true);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Lighting "fix" — a review action, not a mutation
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_ReviewMovableLights(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixPlan
    {
        if (GEditor == nullptr || ck::Is_NOT_Valid(InEditorWorld))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: no editor world to select lights in."),
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
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: no movable-light actor found in that level — re-scan."),
                InFinding.Target.DisplayName));
        }

        Matched.Sort([](const AActor& InLhs, const AActor& InRhs)
        {
            return InLhs.GetPathName().Compare(InRhs.GetPathName(), ESearchCase::CaseSensitive) < 0;
        });

        auto Plan = Make_Plan(InFinding);

        Add_Effect(Plan, ck::Format_UE(TEXT("Selects {} movable-light actor(s) for review. Writes nothing"),
            Matched.Num()), false);

        Plan.Execute = [Matched](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            // Deliberately NOT a mobility change. A light that genuinely moves must stay Movable, and no offline
            // rule can tell which ones those are — so the fix hands the reader the exact set to judge instead of
            // guessing for them.
            GEditor->SelectNone(false, true);

            for (auto* Actor : Matched)
            { GEditor->SelectActor(Actor, true, false); }

            GEditor->NoteSelectionChange();

            return Make_Success(ck::Format_UE(TEXT("{}: selected {} movable-light actor(s) for review — set the ones that never move to Stationary or Static."),
                InPlan.Finding.Target.DisplayName, Matched.Num()), false);
        };

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Project settings fix — a config write, outside the transaction buffer
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_EnableTextureStreaming(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto* Settings = GetMutableDefault<URendererSettings>();

        if (ck::Is_NOT_Valid(Settings))
        { return Make_RefusedPlan(InFinding, TEXT("The renderer settings object could not be reached.")); }

        if (Settings->bTextureStreaming != 0)
        { return Make_RefusedPlan(InFinding, TEXT("Texture streaming is already enabled — re-scan.")); }

        auto Plan = Make_Plan(InFinding);

        Add_Change(Plan, Settings, TEXT("Texture Streaming"), Format_OnOff(false), Format_OnOff(true));
        Add_Effect(Plan, ck::Format_UE(TEXT("Writes {} — Undo cannot reverse a config write"),
            Settings->GetDefaultConfigFilename()), true);

        Plan.Execute = [Settings](const FCkOptimizationDebugger_FixPlan& InPlan)
            -> FCkOptimizationDebugger_FixResult
        {
            if (NOT Is_Ticked(InPlan, Settings, TEXT("Texture Streaming")))
            { return Make_Failure(TEXT("Texture Streaming left unticked — nothing written.")); }

            const auto PreviousValue = Settings->bTextureStreaming;

            Settings->bTextureStreaming = 1;

            // `TryUpdateDefaultConfigFile`, never `UpdateSinglePropertyInConfigFile`: the latter returns `void` and
            // checks nothing, so a `DefaultEngine.ini` that is read-only under source control absorbed the call and
            // this fix reported success over a file it never touched. The CDO would then disagree with the ini until
            // the next editor restart silently reverted it — the worst shape a "fix" can have.
            if (NOT Settings->TryUpdateDefaultConfigFile())
            {
                // Rolled back so the running editor and the file on disk keep agreeing. A CDO left saying "enabled"
                // over an ini that says "disabled" is a state nothing in the session would ever correct.
                Settings->bTextureStreaming = PreviousValue;

                return Make_Failure(ck::Format_UE(
                    TEXT("{} could not be written — check it out of source control (or clear its read-only flag) and try again. Nothing was changed."),
                    Settings->GetDefaultConfigFilename()));
            }

            // The settings editor's own apply path also pushes the value at the console variable the property is
            // bound to. Without this the ini would say one thing and the running editor another until a restart.
            //
            // Read back rather than assumed: `Set` at `ECVF_SetByProjectSetting` is REFUSED when the variable was
            // last set at a higher priority — which is exactly what has happened if the reader typed
            // `r.TextureStreaming 0` at the console, the most likely way to arrive at this finding in the first place.
            auto CvarApplied = true;

            if (auto* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming")))
            {
                ConsoleVariable->Set(1, ECVF_SetByProjectSetting);
                CvarApplied = ConsoleVariable->GetInt() != 0;
            }

            return Make_Success(CvarApplied
                ? ck::Format_UE(TEXT("Texture Streaming enabled and written to {} — this one is a config write, so Undo cannot reverse it."),
                    Settings->GetDefaultConfigFilename())
                : ck::Format_UE(TEXT("Texture Streaming written to {} — but r.TextureStreaming was last set at a higher priority (a console command overrides a project setting), so this session still has it off. Undo cannot reverse the config write."),
                    Settings->GetDefaultConfigFilename()),
                true);
        };

        return Plan;
    }
#endif

    // ----------------------------------------------------------------------------------------------------------------

    /** The dispatch. No transaction of its own — the single-fix and batch entry points each own the record they
     *  want, and a fix that opened its own inside a batch would split one undo into many. */
    auto
        DoPlan_Fix(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixPlan
    {
#if WITH_EDITOR
        const auto CheckId = InFinding.CheckId;

        if (CheckId == k_MeshNaniteCandidate)
        { return Plan_NaniteEnabled(InFinding, true); }

        if (CheckId == k_MeshNaniteOnLowPoly)
        { return Plan_NaniteEnabled(InFinding, false); }

        if (CheckId == k_MeshMissingLods)
        { return Plan_GenerateLods(InFinding); }

        if (CheckId == k_MeshComplexCollision)
        { return Plan_SimpleCollision(InFinding); }

        if (CheckId == k_TextureNormalMap)
        { return Plan_NormalMapCompression(InFinding); }

        if (CheckId == k_TextureDataSrgb)
        { return Plan_DisableSrgb(InFinding); }

        if (CheckId == k_TextureMissingMipmaps)
        { return Plan_RestoreMipmaps(InFinding); }

        if (CheckId == k_MeshNaniteMaterial)
        { return Plan_NaniteMaterialUsage(InFinding); }

        if (CheckId == k_LightingLightmapRes)
        { return Plan_ClampLightmapResolution(InFinding); }

        if (CheckId == k_BlueprintTickEnabled)
        { return Plan_DisableBlueprintStartWithTick(InFinding); }

        if (CheckId == k_ActorEmptyStaticMesh)
        { return Plan_DeleteEmptyStaticMeshActor(InFinding); }

        if (CheckId == k_ActorInstancingCandidate)
        { return Plan_ConvertToInstances(InFinding, InEditorWorld); }

        if (CheckId == k_LightingMovableCount)
        { return Plan_ReviewMovableLights(InFinding, InEditorWorld); }

        if (CheckId == k_SettingsTextureStreaming)
        { return Plan_EnableTextureStreaming(InFinding); }

        return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{} has no automatic fix."), CheckId));
#else
        // The module ships in packaged Development/DebugGame targets, where there is no transaction buffer, no
        // asset to edit and no editor world. Saying so beats a silent no-op that reads as a successful fix.
        (void)InEditorWorld;

        return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{}: applying a fix needs an editor session."),
            InFinding.Target.DisplayName));
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Plans and immediately runs. The plan is the only validator, so a refusal here IS the plan's refusal — there
     *  is no second opinion that could disagree with what a preview showed. */
    auto
        DoApply_Fix(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
        const auto Plan = DoPlan_Fix(InFinding, InEditorWorld);

        if (NOT Plan.CanApply || NOT Plan.Execute)
        { return Make_Failure(Plan.RefusalReason); }

        return Plan.Execute(Plan);
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

// The PLANNING half of the fix engine. Its pure projections live in `CkOptimizationDebugger_FixPlan.cpp`; the
// planners themselves live here, beside the fix code they describe, because a planner that sat in another file
// would be a second place to look for what a fix does.
namespace ck_optimization_debugger_fixplan
{
    auto
        Plan_Fix(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixPlan
    {
        using namespace ck_optimization_debugger_fixes;
        using namespace ck_optimization_debugger_fixes_impl;

        // Both halves, the same two the button needs — a finding whose check never claimed a fix must not plan one
        // because the registry grew an entry.
        if (NOT Can_ApplyFix(InFinding))
        {
            return Make_RefusedPlan(InFinding, ck::Format_UE(TEXT("{} has no automatic fix."), InFinding.CheckId));
        }

        return DoPlan_Fix(InFinding, InEditorWorld);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Plan_Fixes(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            UWorld* InEditorWorld)
        -> TArray<FCkOptimizationDebugger_FixPlan>
    {
        auto Plans = TArray<FCkOptimizationDebugger_FixPlan>{};
        Plans.Reserve(InFindings.Num());

        for (const auto& Finding : InFindings)
        { Plans.Add(Plan_Fix(Finding, InEditorWorld)); }

        return Plans;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_PreviewedPlan(
            const FCkOptimizationDebugger_FixPlan& InPreviewedPlan,
            UWorld* InEditorWorld)
        -> FCkOptimizationDebugger_FixResult
    {
        using namespace ck_optimization_debugger_fixes;
        using namespace ck_optimization_debugger_fixes_impl;

        if (NOT Get_CanApplyFixes())
        { return Make_Failure(Get_FixesUnavailableReason()); }

        // Re-planned, never re-used. The previewed plan captured objects at preview time and its `Execute` closes
        // over them; between the preview and the press an import can finish, an undo can land, or another tool can
        // write. What runs is always THIS moment's plan.
        auto Fresh = DoPlan_Fix(InPreviewedPlan.Finding, InEditorWorld);

        if (NOT Fresh.CanApply || NOT Fresh.Execute)
        { return Make_Failure(Fresh.RefusalReason); }

        if (Get_HasDrifted(InPreviewedPlan, Fresh))
        {
            return Make_Failure(ck::Format_UE(
                TEXT("{}: it changed since you previewed it — nothing was written. Preview it again to see what it looks like now."),
                InPreviewedPlan.Finding.Target.DisplayName));
        }

        // The reader's ticks are carried over by (object, property), never by index: an index is only meaningful
        // against the list it was taken from, and the whole point of the drift check above is that the two lists
        // could have been different.
        for (auto& Change : Fresh.Changes)
        { Change.Included = Get_IsChangeIncluded(InPreviewedPlan, Change.ObjectPath, Change.PropertyLabel); }

        if (NOT Get_HasIncludedWork(Fresh))
        {
            return Make_Failure(ck::Format_UE(TEXT("{}: nothing was ticked, so nothing was written."),
                InPreviewedPlan.Finding.Target.DisplayName));
        }

        return Fresh.Execute(Fresh);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One log entry per attempted plan, built from the PREVIEWED plan: the drift check inside
     *  `Apply_PreviewedPlan` has already established that the fresh plan has the same shape and the same ticks, so
     *  the two agree about what was written. */
    auto
        Make_LogEntry(
            const FCkOptimizationDebugger_FixPlan& InPlan,
            const FCkOptimizationDebugger_FixResult& InResult)
        -> FCkOptimizationDebugger_FixLogEntry
    {
        auto Entry = FCkOptimizationDebugger_FixLogEntry{};
        Entry.CheckId = InPlan.Finding.CheckId;
        Entry.FixVerb = InPlan.FixVerb;
        Entry.TargetLabel = InPlan.Finding.Target.DisplayName;
        Entry.Message = InResult.Message;
        Entry.Succeeded = InResult.Succeeded;

        for (const auto& Change : Get_IncludedChanges(InPlan))
        {
            if (NOT Change.ObjectPath.IsNull())
            { Entry.WrittenObjectPaths.AddUnique(Change.ObjectPath); }
        }

        // A fix with no property rows still wrote something — it deleted an actor, converted placements, or
        // changed a setting. The finding's own target is the closest thing to the package that went dirty, and
        // naming nothing at all would drop those fixes out of the modified list entirely.
        if (Entry.WrittenObjectPaths.IsEmpty() && NOT InPlan.Finding.Target.Path.IsNull())
        { Entry.WrittenObjectPaths.Add(InPlan.Finding.Target.Path); }

        return Entry;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_PreviewedPlans(
            const TArray<FCkOptimizationDebugger_FixPlan>& InPreviewedPlans,
            UWorld* InEditorWorld,
            TArray<FCkOptimizationDebugger_FixLogEntry>& OutLogEntries)
        -> FCkOptimizationDebugger_BatchFixResult
    {
        using namespace ck_optimization_debugger_fixes;
        using namespace ck_optimization_debugger_fixes_impl;

        auto Batch = FCkOptimizationDebugger_BatchFixResult{};

        if (NOT Get_CanApplyFixes())
        {
            Accumulate(Batch, Make_Failure(Get_FixesUnavailableReason()));
            return Batch;
        }

        // The SAME three-way split `Apply_Fixes` makes, over plans rather than findings: one transaction for the
        // transactional part, config writes after it and outside it, review actions last so the selection one of
        // them leaves is the selection the reader ends up looking at.
        auto Transactional = TArray<const FCkOptimizationDebugger_FixPlan*>{};
        auto ConfigWrites = TArray<const FCkOptimizationDebugger_FixPlan*>{};
        auto Reviews = TArray<const FCkOptimizationDebugger_FixPlan*>{};

        for (const auto& Plan : InPreviewedPlans)
        {
            if (NOT Plan.CanApply)
            { continue; }

            const auto* Info = TryGet_FixInfo(Plan.Finding.CheckId);

            if (Info == nullptr)
            { continue; }

            switch (Info->Execution)
            {
                case ECkOptimizationDebugger_FixExecution::ConfigWrite: ConfigWrites.Add(&Plan); break;
                case ECkOptimizationDebugger_FixExecution::Review:      Reviews.Add(&Plan);      break;
                default:                                               Transactional.Add(&Plan); break;
            }
        }

        const auto RunOne = [&](const FCkOptimizationDebugger_FixPlan& InPlan) -> void
        {
            const auto Result = Apply_PreviewedPlan(InPlan, InEditorWorld);

            Accumulate(Batch, Result);
            OutLogEntries.Add(Make_LogEntry(InPlan, Result));
        };

        if (NOT Transactional.IsEmpty())
        {
#if WITH_EDITOR
            // ONE record for the whole transactional part: a reader who applied six fixes with one click expects
            // one Ctrl+Z to put the level back, not six.
            //
            // A part of exactly one keeps the fix's OWN verb as the undo label — "Enable Nanite (SM_Foo)" rather
            // than "Apply 1 optimization fix(es)", which is what the separate single-fix entry point used to exist
            // for. Naming it here means one apply path instead of two that can disagree.
            const auto Label = Transactional.Num() == 1
                ? ck::Format_UE(TEXT("{} ({})"), Transactional[0]->FixVerb, Transactional[0]->Finding.Target.DisplayName)
                : ck::Format_UE(TEXT("Apply {} optimization fix(es)"), Transactional.Num());

            const auto Transaction = FScopedTransaction{FText::FromString(Label)};

            for (const auto* Plan : Transactional)
            { RunOne(*Plan); }
#else
            for (const auto* Plan : Transactional)
            { RunOne(*Plan); }
#endif
        }

        for (const auto* Plan : ConfigWrites)
        { RunOne(*Plan); }

        for (const auto* Plan : Reviews)
        { RunOne(*Plan); }

        return Batch;
    }
}

// --------------------------------------------------------------------------------------------------------------------
