#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_SnapshotCapture.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/SkeletalMesh.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_debugger_snapshot_capture_impl
{
    constexpr auto k_MinCaptureHeight = 144;
    constexpr auto k_MaxCaptureHeight = 4096;
    constexpr auto k_FallbackAspect = 16.0f / 9.0f;

    // ----------------------------------------------------------------------------------------------------------------

    /** One candidate primitive, still holding its component — this type never leaves `Run_Capture`. The snapshot it
     *  produces holds soft paths and copied numbers, because a snapshot outlives the world it pictured. */
    struct FCandidate
    {
        UPrimitiveComponent* Component = nullptr;
        FString ActorName;
        float Distance = 0.0f;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /** Registers, captures and always unregisters, whatever the readback does. A scene-capture component left
     *  registered on a world keeps rendering into a render target nobody reads. */
    struct FCaptureComponentScope
    {
        FCaptureComponentScope() = default;

        FCaptureComponentScope(const FCaptureComponentScope&) = delete;
        auto operator=(const FCaptureComponentScope&) -> FCaptureComponentScope& = delete;

        ~FCaptureComponentScope()
        {
            if (Component != nullptr)
            { Component->DestroyComponent(); }
        }

        USceneCaptureComponent2D* Component = nullptr;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /** The ONE visibility predicate the capture uses, named so Phase 5's stencil set can be held to the same one.
     *  Two predicates would put a primitive in the picture and out of the ID map, or the reverse, and the symptom is
     *  a mesh that cannot be clicked with no way to tell why. */
    auto
        Is_CandidateVisible(
            const UPrimitiveComponent* InComponent)
        -> bool
    {
        if (InComponent == nullptr || NOT InComponent->IsRegistered())
        { return false; }

        // `ShouldRender` folds the component's own visibility, the owner's hidden-in-game state and the world's
        // rules together, which is exactly the question "is this in the picture".
        return InComponent->ShouldRender();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_BlendModeLabel(
            const UMaterialInterface* InMaterial)
        -> FString
    {
        if (InMaterial == nullptr)
        { return FString{TEXT("None")}; }

        switch (InMaterial->GetBlendMode())
        {
            case BLEND_Opaque:         return FString{TEXT("Opaque")};
            case BLEND_Masked:         return FString{TEXT("Masked")};
            case BLEND_Translucent:    return FString{TEXT("Translucent")};
            case BLEND_Additive:       return FString{TEXT("Additive")};
            case BLEND_Modulate:       return FString{TEXT("Modulate")};
            case BLEND_AlphaComposite: return FString{TEXT("Alpha Composite")};
            case BLEND_AlphaHoldout:   return FString{TEXT("Alpha Holdout")};
            default:                   return FString{TEXT("Unknown")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ShadingModelLabel(
            const UMaterialInterface* InMaterial)
        -> FString
    {
        if (InMaterial == nullptr)
        { return FString{TEXT("None")}; }

        const auto Models = InMaterial->GetShadingModels();

        // A material may declare several and pick between them in the graph; the first is what the row can honestly
        // print, and the count is what says there are others.
        if (Models.IsLit() && Models.CountShadingModels() > 1)
        { return ck::Format_UE(TEXT("Multiple ({})"), Models.CountShadingModels()); }

        if (NOT Models.IsLit())
        { return FString{TEXT("Unlit")}; }

        return FString{TEXT("Lit")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_MaterialSlot(
            const FName& InSlotName,
            UMaterialInterface* InMaterial)
        -> FCkOptimizationDebugger_SnapshotMaterialSlot
    {
        auto Slot = FCkOptimizationDebugger_SnapshotMaterialSlot{};
        Slot.SlotName = InSlotName.IsNone() ? FString{TEXT("(unnamed)")} : InSlotName.ToString();

        if (InMaterial == nullptr)
        {
            Slot.MaterialName = FString{TEXT("(empty)")};
            return Slot;
        }

        Slot.MaterialName = InMaterial->GetName();
        Slot.MaterialPath = FSoftObjectPath{InMaterial};
        Slot.BlendMode = Get_BlendModeLabel(InMaterial);
        Slot.ShadingModel = Get_ShadingModelLabel(InMaterial);
        Slot.IsTwoSided = InMaterial->IsTwoSided();

        // The same sampler proxy `Material.SamplerBudget` uses, and the same call shape, so the two numbers cannot
        // disagree about one material.
        auto UsedTextures = TArray<UTexture*>{};
        InMaterial->GetUsedTextures(UsedTextures);

        auto UniqueTextures = TSet<FSoftObjectPath>{};

        for (const auto* Texture : UsedTextures)
        {
            if (Texture == nullptr)
            { continue; }

            UniqueTextures.Add(FSoftObjectPath{Texture});
        }

        Slot.UsedTextureCount = UniqueTextures.Num();

        return Slot;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Fill_StaticMeshPrim(
            UStaticMeshComponent* InComponent,
            FCkOptimizationDebugger_SnapshotPrim& OutPrim)
        -> bool
    {
        // `.Get()` because the accessor hands back a TObjectPtr; validity is asked of the raw pointer.
        auto* Mesh = InComponent->GetStaticMesh().Get();

        if (ck::Is_NOT_Valid(Mesh))
        { return false; }

        const auto* RenderData = Mesh->GetRenderData();

        if (RenderData == nullptr)
        { return false; }

        OutPrim.MeshDisplayName = Mesh->GetName();
        OutPrim.MeshAssetPath = FSoftObjectPath{Mesh};
        OutPrim.IsNanite = RenderData->HasValidNaniteData();

        for (auto LodIndex = 0; LodIndex < RenderData->LODResources.Num(); ++LodIndex)
        {
            const auto& Resource = RenderData->LODResources[LodIndex];

            auto Lod = FCkOptimizationDebugger_SnapshotLod{};
            Lod.Triangles = Resource.GetNumTriangles();
            Lod.Sections = Resource.Sections.Num();

            OutPrim.Lods.Add(Lod);
        }

        const auto& StaticMaterials = Mesh->GetStaticMaterials();

        for (auto SlotIndex = 0; SlotIndex < StaticMaterials.Num(); ++SlotIndex)
        {
            // The COMPONENT's material, not the mesh's: an override is what actually renders, and a row printing the
            // asset default would describe a material this placement does not use.
            OutPrim.MaterialSlots.Add(
                Build_MaterialSlot(StaticMaterials[SlotIndex].MaterialSlotName, InComponent->GetMaterial(SlotIndex)));
        }

        if (const auto* Instanced = Cast<UInstancedStaticMeshComponent>(InComponent))
        {
            OutPrim.Kind = ECkOptimizationDebugger_SnapshotPrimKind::InstancedStaticMesh;
            OutPrim.InstanceCount = Instanced->GetInstanceCount();
        }

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Fill_SkeletalMeshPrim(
            USkeletalMeshComponent* InComponent,
            FCkOptimizationDebugger_SnapshotPrim& OutPrim)
        -> bool
    {
        auto* Mesh = InComponent->GetSkeletalMeshAsset();

        if (ck::Is_NOT_Valid(Mesh))
        { return false; }

        const auto* RenderData = Mesh->GetResourceForRendering();

        if (RenderData == nullptr)
        { return false; }

        OutPrim.Kind = ECkOptimizationDebugger_SnapshotPrimKind::SkeletalMesh;
        OutPrim.MeshDisplayName = Mesh->GetName();
        OutPrim.MeshAssetPath = FSoftObjectPath{Mesh};

        for (auto LodIndex = 0; LodIndex < RenderData->LODRenderData.Num(); ++LodIndex)
        {
            const auto& LodData = RenderData->LODRenderData[LodIndex];

            auto Lod = FCkOptimizationDebugger_SnapshotLod{};
            Lod.Sections = LodData.RenderSections.Num();

            for (const auto& Section : LodData.RenderSections)
            { Lod.Triangles += static_cast<int32>(Section.NumTriangles); }

            OutPrim.Lods.Add(Lod);
        }

        const auto& Materials = Mesh->GetMaterials();

        for (auto SlotIndex = 0; SlotIndex < Materials.Num(); ++SlotIndex)
        {
            OutPrim.MaterialSlots.Add(
                Build_MaterialSlot(Materials[SlotIndex].MaterialSlotName, InComponent->GetMaterial(SlotIndex)));
        }

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_capture
{
    auto
        TryGet_CaptureWorld()
        -> UWorld*
    {
        if (GEngine == nullptr)
        { return nullptr; }

        // A running game or PIE session wins: while one is up, the picture worth taking is the one the player is
        // looking at, not the one the editor viewport happens to be pointed at behind it.
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::Game && Context.WorldType != EWorldType::PIE)
            { continue; }

            if (ck::IsValid(Context.World()))
            { return Context.World(); }
        }

#if WITH_EDITOR
        if (GEditor != nullptr)
        { return GEditor->GetEditorWorldContext().World(); }
#endif

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_CaptureView(
            UWorld* InWorld)
        -> TOptional<FMinimalViewInfo>
    {
        if (ck::Is_NOT_Valid(InWorld))
        { return {}; }

        if (const auto* PlayerController = InWorld->GetFirstPlayerController();
            PlayerController != nullptr && ck::IsValid(PlayerController->PlayerCameraManager))
        {
            return PlayerController->PlayerCameraManager->GetCameraCacheView();
        }

#if WITH_EDITOR
        // The module's established viewport discriminator. Read directly rather than through `CalcSceneView`, which
        // returns stale matrices outside a draw.
        if (GCurrentLevelEditingViewportClient != nullptr)
        {
            auto View = FMinimalViewInfo{};
            View.Location = GCurrentLevelEditingViewportClient->GetViewLocation();
            View.Rotation = GCurrentLevelEditingViewportClient->GetViewRotation();
            View.FOV = GCurrentLevelEditingViewportClient->ViewFOV;

            return View;
        }
#endif

        return {};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Capture(
            UWorld* InWorld,
            const FCkOptimizationDebugger_SnapshotCaptureParams& InParams,
            FString& OutFailureReason)
        -> TOptional<FCkOptimizationDebugger_Snapshot>
    {
        using namespace ck_optimization_debugger_snapshot_capture_impl;

        OutFailureReason.Reset();

        if (ck::Is_NOT_Valid(InWorld))
        {
            OutFailureReason = TEXT("There is no world to capture.");
            return {};
        }

        const auto View = TryGet_CaptureView(InWorld);

        if (NOT View.IsSet())
        {
            OutFailureReason = TEXT("No camera to capture from — open a level viewport or start a play session.");
            return {};
        }

        // ---- Size ----
        const auto Aspect = View->AspectRatio > 0.0f ? View->AspectRatio : k_FallbackAspect;
        const auto Width = FMath::Clamp(InParams.CaptureWidth, 256, 4096);
        const auto Height = FMath::Clamp(
            FMath::RoundToInt(static_cast<float>(Width) / Aspect), k_MinCaptureHeight, k_MaxCaptureHeight);

        // ---- Candidates ----
        auto ProjectionView = FMinimalViewInfo{View.GetValue()};
        ProjectionView.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
        ProjectionView.bConstrainAspectRatio = true;

        const auto ViewRotationMatrix = FInverseRotationMatrix{ProjectionView.Rotation} * FMatrix{
            FPlane{0.0, 0.0, 1.0, 0.0},
            FPlane{1.0, 0.0, 0.0, 0.0},
            FPlane{0.0, 1.0, 0.0, 0.0},
            FPlane{0.0, 0.0, 0.0, 1.0}};

        const auto ViewMatrix = FTranslationMatrix{-ProjectionView.Location} * ViewRotationMatrix;
        const auto ViewProjectionMatrix = ViewMatrix * ProjectionView.CalculateProjectionMatrix();

        auto Frustum = FConvexVolume{};
        constexpr auto UseNearPlane = false;
        GetViewFrustumBounds(Frustum, ViewProjectionMatrix, UseNearPlane);

        auto Candidates = TArray<FCandidate>{};
        auto ExcludedCount = 0;

        for (auto ActorIt = TActorIterator<AActor>{InWorld}; ActorIt; ++ActorIt)
        {
            auto* Actor = *ActorIt;

            if (ck::Is_NOT_Valid(Actor))
            { continue; }

            for (auto* Component : TInlineComponentArray<UPrimitiveComponent*>{Actor})
            {
                if (NOT Is_CandidateVisible(Component))
                { continue; }

                const auto IsSupported = Component->IsA<UStaticMeshComponent>() || Component->IsA<USkeletalMeshComponent>();

                if (NOT IsSupported)
                {
                    // Counted rather than ignored: the notes line saying how many primitives are not identifiable is
                    // what stops a reader treating an unclickable region as a bug.
                    ++ExcludedCount;
                    continue;
                }

                const auto Bounds = Component->Bounds;

                if (NOT Frustum.IntersectSphere(Bounds.Origin, Bounds.SphereRadius))
                { continue; }

                auto Candidate = FCandidate{};
                Candidate.Component = Component;
                Candidate.ActorName = Actor->GetActorNameOrLabel();
                Candidate.Distance = static_cast<float>(FVector::Dist(Bounds.Origin, ProjectionView.Location));

                Candidates.Add(MoveTemp(Candidate));
            }
        }

        // Nearest first, name as the tie-break: world iteration order depends on what happened to load, and a
        // snapshot whose prim indices moved between two captures of the same scene would be unreadable.
        Candidates.Sort([](const FCandidate& InLhs, const FCandidate& InRhs)
        {
            if (InLhs.Distance != InRhs.Distance)
            { return InLhs.Distance < InRhs.Distance; }

            return InLhs.ActorName.Compare(InRhs.ActorName) < 0;
        });

        auto TruncatedCount = 0;

        if (const auto MaxPrims = FMath::Max(1, InParams.MaxPrims); Candidates.Num() > MaxPrims)
        {
            TruncatedCount = Candidates.Num() - MaxPrims;
            Candidates.SetNum(MaxPrims);
        }

        // ---- Stats ----
        auto Snapshot = FCkOptimizationDebugger_Snapshot{};
        Snapshot.Id = FGuid::NewGuid();
        Snapshot.Label = InParams.Label;
        Snapshot.CapturedAt = InParams.CapturedAt;
        Snapshot.WorldName = InWorld->GetMapName();
        Snapshot.Width = Width;
        Snapshot.Height = Height;

        for (const auto& Candidate : Candidates)
        {
            auto Prim = FCkOptimizationDebugger_SnapshotPrim{};
            Prim.DistanceFromCamera = Candidate.Distance;

            const auto Filled = [&]() -> bool
            {
                if (auto* Skeletal = Cast<USkeletalMeshComponent>(Candidate.Component))
                { return Fill_SkeletalMeshPrim(Skeletal, Prim); }

                if (auto* Static = Cast<UStaticMeshComponent>(Candidate.Component))
                { return Fill_StaticMeshPrim(Static, Prim); }

                return false;
            }();

            if (NOT Filled)
            {
                // A component with no render data is in the picture but cannot be described; it belongs in the
                // excluded count rather than in the table as a row of zeroes.
                ++ExcludedCount;
                continue;
            }

            Prim.DisplayName = ck::Format_UE(TEXT("{} / {}"), Candidate.ActorName, Prim.MeshDisplayName);

            Snapshot.Prims.Add(MoveTemp(Prim));
        }

        // ---- Colour ----
        auto CaptureScope = FCaptureComponentScope{};
        CaptureScope.Component = NewObject<USceneCaptureComponent2D>(GetTransientPackage());

        if (ck::Is_NOT_Valid(CaptureScope.Component))
        {
            OutFailureReason = TEXT("Could not create the scene-capture component.");
            return {};
        }

        auto* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());

        if (ck::Is_NOT_Valid(RenderTarget))
        {
            OutFailureReason = TEXT("Could not create the capture render target.");
            return {};
        }

        constexpr auto ForceLinearGamma = false;
        RenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, ForceLinearGamma);
        RenderTarget->ClearColor = FLinearColor::Black;

        auto* Component = CaptureScope.Component;

        // Explicitly one capture, driven by the press: every-frame capture on a component nobody ticks would render
        // the scene again for a render target already read.
        Component->bCaptureEveryFrame = false;
        Component->bCaptureOnMovement = false;
        Component->bAlwaysPersistRenderingState = true;
        Component->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Component->FOVAngle = ProjectionView.FOV;
        Component->TextureTarget = RenderTarget;

        Component->RegisterComponentWithWorld(InWorld);
        Component->SetWorldLocationAndRotation(ProjectionView.Location, ProjectionView.Rotation.Quaternion());
        Component->CaptureScene();

        auto* Resource = RenderTarget->GameThread_GetRenderTargetResource();

        if (Resource == nullptr)
        {
            OutFailureReason = TEXT("The capture render target has no resource to read back.");
            return {};
        }

        auto Pixels = TArray<FColor>{};

        // Flushes rendering, which is exactly what an explicit press is allowed to do — and the reason capture is
        // not offered as anything automatic.
        if (NOT Resource->ReadPixels(Pixels) || Pixels.Num() != Width * Height)
        {
            OutFailureReason = TEXT("Reading the captured image back from the GPU failed.");
            return {};
        }

        const auto ImageView = FImageView{Pixels.GetData(), Width, Height, ERawImageFormat::BGRA8};

        if (NOT FImageUtils::CompressImage(Snapshot.ColorPng, TEXT("png"), ImageView))
        {
            OutFailureReason = TEXT("Compressing the captured image failed.");
            return {};
        }

        // ---- Notes ----
        auto Notes = TArray<FString>{};

        if (ExcludedCount > 0)
        {
            Notes.Add(ck::Format_UE(TEXT("{} primitive(s) excluded — landscape, BSP, effects and anything without "
                    "render data are in the picture but cannot be identified"), ExcludedCount));
        }

        if (TruncatedCount > 0)
        {
            Notes.Add(ck::Format_UE(TEXT("{} further primitive(s) beyond the {} nearest were not captured"),
                TruncatedCount, Snapshot.Prims.Num()));
        }

        Snapshot.CaptureNotes = FString::Join(Notes, TEXT(". "));
        Snapshot.UnidentifiedPixelCount = 0;

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------
