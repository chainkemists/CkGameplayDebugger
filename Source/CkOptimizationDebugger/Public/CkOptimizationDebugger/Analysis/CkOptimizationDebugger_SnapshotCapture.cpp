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
#include "CkUsf/LookDefinition/CkUsf_LookDefinition_Naming.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "Misc/Paths.h"
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

    /** What a component's custom-depth state was before the capture touched it. Every mutated component gets one, and
     *  the guard that holds them restores on EVERY exit path — success, failure and early return alike. A capture that
     *  left the world's stencil state rewritten would break whatever was using it (CkUsf outlines and cel shading both
     *  are) in a way the reader would blame on anything but a debugger screenshot. */
    struct FLedgerEntry
    {
        TWeakObjectPtr<UPrimitiveComponent> Component;
        bool RenderCustomDepth = false;
        int32 StencilValue = 0;
    };

    // ----------------------------------------------------------------------------------------------------------------

    struct FStencilLedger
    {
        FStencilLedger() = default;

        FStencilLedger(const FStencilLedger&) = delete;
        auto operator=(const FStencilLedger&) -> FStencilLedger& = delete;

        ~FStencilLedger()
        {
            for (const auto& Entry : Entries)
            {
                auto* Component = Entry.Component.Get();

                if (Component == nullptr)
                { continue; }

                Component->SetCustomDepthStencilValue(Entry.StencilValue);
                Component->SetRenderCustomDepth(Entry.RenderCustomDepth);
            }

            if (PriorCustomDepthMode.IsSet())
            {
                if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth")))
                { CVar->Set(PriorCustomDepthMode.GetValue(), ECVF_SetByCode); }
            }
        }

        auto Record(UPrimitiveComponent* InComponent) -> void
        {
            if (InComponent == nullptr)
            { return; }

            auto Entry = FLedgerEntry{};
            Entry.Component = InComponent;
            Entry.RenderCustomDepth = InComponent->bRenderCustomDepth;
            Entry.StencilValue = InComponent->CustomDepthStencilValue;

            Entries.Add(MoveTemp(Entry));
        }

        TArray<FLedgerEntry> Entries;
        TOptional<int32> PriorCustomDepthMode;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /** The generated master for the `StencilId` look, or null when it has not been generated yet. Resolved by the
     *  naming helper rather than a literal path, so the debugger and CkUsf cannot disagree about where looks live. */
    auto
        TryLoad_StencilVisMaterial()
        -> UMaterialInterface*
    {
        const auto ObjectPath = ck::usf::Get_GeneratedMasterObjectPath(FName{TEXT("StencilId")});

        return Cast<UMaterialInterface>(FSoftObjectPath{ObjectPath}.TryLoad());
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One identity pass read back. Kept beside the colour readback rather than sharing it: this one wants only the
     *  red channel and must not be tempted into the PNG path. */
    auto
        Resource_ReadIdPass(
            UTextureRenderTarget2D* InRenderTarget,
            TArray<FColor>& OutPixels,
            int32 InWidth,
            int32 InHeight)
        -> bool
    {
        auto* Resource = InRenderTarget->GameThread_GetRenderTargetResource();

        if (Resource == nullptr)
        { return false; }

        return Resource->ReadPixels(OutPixels) && OutPixels.Num() == InWidth * InHeight;
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

    /** The capture-wide dedup the snapshot's unique-material / unique-texture / resident-bytes facts come from.
     *  Filled as a side effect of building slots, because the paths that make dedup possible exist only here — the
     *  slot itself keeps names, which is all a row or a report prints. */
    struct FMaterialCensus
    {
        TSet<FSoftObjectPath> Materials;
        TSet<FSoftObjectPath> Textures;
        int64 TextureResidentBytes = 0;

        // Reset per prim by the fill functions. The snapshot-wide sets answer "what does this VIEW cost", which
        // counts a shared texture once; a lens colouring one mesh has to answer "what does THIS mesh cost", which
        // counts it for every mesh that samples it. Two questions, two accumulators.
        TSet<FSoftObjectPath> PrimTextures;
        int64 PrimTextureResidentBytes = 0;

        auto Begin_Prim() -> void
        {
            PrimTextures.Reset();
            PrimTextureResidentBytes = 0;
        }
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_MaterialSlot(
            const FName& InSlotName,
            UMaterialInterface* InMaterial,
            FMaterialCensus& InOutCensus)
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

        InOutCensus.Materials.Add(Slot.MaterialPath);

        // The same sampler proxy `Material.SamplerBudget` uses, and the same call shape, so the two numbers cannot
        // disagree about one material.
        auto UsedTextures = TArray<UTexture*>{};
        InMaterial->GetUsedTextures(UsedTextures);

        auto UniqueTextures = TSet<FSoftObjectPath>{};

        for (auto* Texture : UsedTextures)
        {
            if (Texture == nullptr)
            { continue; }

            const auto TexturePath = FSoftObjectPath{Texture};

            if (NOT UniqueTextures.Contains(TexturePath))
            {
                UniqueTextures.Add(TexturePath);
                Slot.UsedTextureNames.Add(Texture->GetName());
            }

            // Resident size, exclusive — the memory analyzer's own API and mode, so these totals and its table agree.
            auto Size = FResourceSizeEx{EResourceSizeMode::Exclusive};
            Texture->GetResourceSizeEx(Size);

            const auto ResidentBytes = static_cast<int64>(Size.GetTotalMemoryBytes());

            if (NOT InOutCensus.PrimTextures.Contains(TexturePath))
            {
                InOutCensus.PrimTextures.Add(TexturePath);
                InOutCensus.PrimTextureResidentBytes += ResidentBytes;
            }

            if (InOutCensus.Textures.Contains(TexturePath))
            { continue; }

            InOutCensus.Textures.Add(TexturePath);
            InOutCensus.TextureResidentBytes += ResidentBytes;
        }

        Slot.UsedTextureCount = UniqueTextures.Num();

        return Slot;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The scalability preset as one printable line. Read at capture time rather than stored per cvar, because what
     *  a reader needs from it is "were these two captures taken at the same quality" — one string compares, ten
     *  numbers do not. A cvar this build does not have prints `?` rather than a zero that would read as Low. */
    auto
        Build_ScalabilityPresetText()
        -> FString
    {
        const auto Groups = TArray<TPair<const TCHAR*, const TCHAR*>>{
            {TEXT("View"),    TEXT("sg.ViewDistanceQuality")},
            {TEXT("AA"),      TEXT("sg.AntiAliasingQuality")},
            {TEXT("Shadow"),  TEXT("sg.ShadowQuality")},
            {TEXT("GI"),      TEXT("sg.GlobalIlluminationQuality")},
            {TEXT("Refl"),    TEXT("sg.ReflectionQuality")},
            {TEXT("PP"),      TEXT("sg.PostProcessQuality")},
            {TEXT("Tex"),     TEXT("sg.TextureQuality")},
            {TEXT("FX"),      TEXT("sg.EffectsQuality")},
            {TEXT("Foliage"), TEXT("sg.FoliageQuality")},
            {TEXT("Shading"), TEXT("sg.ShadingQuality")}};

        auto Parts = TArray<FString>{};
        Parts.Reserve(Groups.Num());

        for (const auto& Group : Groups)
        {
            const auto* CVar = IConsoleManager::Get().FindConsoleVariable(Group.Value);

            Parts.Add(CVar != nullptr
                ? ck::Format_UE(TEXT("{} {}"), Group.Key, CVar->GetInt())
                : ck::Format_UE(TEXT("{} ?"), Group.Key));
        }

        return FString::Join(Parts, TEXT(" · "));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ScreenPercentage()
        -> float
    {
        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));

        return CVar != nullptr ? CVar->GetFloat() : 0.0f;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Fill_StaticMeshPrim(
            UStaticMeshComponent* InComponent,
            FCkOptimizationDebugger_SnapshotPrim& OutPrim,
            FMaterialCensus& InOutCensus)
        -> bool
    {
        // `.Get()` because the accessor hands back a TObjectPtr; validity is asked of the raw pointer.
        InOutCensus.Begin_Prim();

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
            Lod.Vertices = Resource.GetNumVertices();
            Lod.ScreenSize = RenderData->ScreenSize[LodIndex].GetValue();

            OutPrim.Lods.Add(Lod);
        }

        auto ResourceSize = FResourceSizeEx{EResourceSizeMode::Exclusive};
        Mesh->GetResourceSizeEx(ResourceSize);
        OutPrim.MeshResourceSizeBytes = static_cast<int64>(ResourceSize.GetTotalMemoryBytes());

        const auto& StaticMaterials = Mesh->GetStaticMaterials();

        for (auto SlotIndex = 0; SlotIndex < StaticMaterials.Num(); ++SlotIndex)
        {
            // The COMPONENT's material, not the mesh's: an override is what actually renders, and a row printing the
            // asset default would describe a material this placement does not use.
            OutPrim.MaterialSlots.Add(Build_MaterialSlot(
                StaticMaterials[SlotIndex].MaterialSlotName, InComponent->GetMaterial(SlotIndex), InOutCensus));
        }

        OutPrim.TextureResidentBytes = InOutCensus.PrimTextureResidentBytes;

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
            FCkOptimizationDebugger_SnapshotPrim& OutPrim,
            FMaterialCensus& InOutCensus)
        -> bool
    {
        InOutCensus.Begin_Prim();

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
            Lod.Vertices = static_cast<int32>(LodData.GetNumVertices());

            if (const auto* LodInfo = Mesh->GetLODInfo(LodIndex))
            { Lod.ScreenSize = LodInfo->ScreenSize.GetValue(); }

            for (const auto& Section : LodData.RenderSections)
            { Lod.Triangles += static_cast<int32>(Section.NumTriangles); }

            OutPrim.Lods.Add(Lod);
        }

        auto ResourceSize = FResourceSizeEx{EResourceSizeMode::Exclusive};
        Mesh->GetResourceSizeEx(ResourceSize);
        OutPrim.MeshResourceSizeBytes = static_cast<int64>(ResourceSize.GetTotalMemoryBytes());

        const auto& Materials = Mesh->GetMaterials();

        for (auto SlotIndex = 0; SlotIndex < Materials.Num(); ++SlotIndex)
        {
            OutPrim.MaterialSlots.Add(Build_MaterialSlot(
                Materials[SlotIndex].MaterialSlotName, InComponent->GetMaterial(SlotIndex), InOutCensus));
        }

        OutPrim.TextureResidentBytes = InOutCensus.PrimTextureResidentBytes;

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
        using namespace ck_optimization_debugger_snapshot;

        OutFailureReason.Reset();

        if (ck::Is_NOT_Valid(InWorld))
        {
            OutFailureReason = TEXT("There is no world to capture.");
            return {};
        }

        // The override is a REPLAY of a stored point of view, so it wins over the live camera outright — asking the
        // world first and substituting afterwards would make a recapture depend on there being a camera it ignores.
        const auto View = InParams.ViewOverride.IsSet() ? InParams.ViewOverride : TryGet_CaptureView(InWorld);

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

        // The PROJECTION view, not the raw one: it carries the aspect the picture was actually rasterized with, so
        // replaying it reproduces this framing rather than a differently-cropped version of it.
        Snapshot.CameraLocation = ProjectionView.Location;
        Snapshot.CameraRotation = ProjectionView.Rotation;
        Snapshot.CameraFov = ProjectionView.FOV;

        Snapshot.ScalabilityPreset = Build_ScalabilityPresetText();
        Snapshot.ScreenPercentage = Get_ScreenPercentage();
        Snapshot.BuildVersion = FApp::GetBuildVersion();

        auto MaterialCensus = FMaterialCensus{};

        for (const auto& Candidate : Candidates)
        {
            auto Prim = FCkOptimizationDebugger_SnapshotPrim{};
            Prim.DistanceFromCamera = Candidate.Distance;

            const auto Filled = [&]() -> bool
            {
                if (auto* Skeletal = Cast<USkeletalMeshComponent>(Candidate.Component))
                { return Fill_SkeletalMeshPrim(Skeletal, Prim, MaterialCensus); }

                if (auto* Static = Cast<UStaticMeshComponent>(Candidate.Component))
                { return Fill_StaticMeshPrim(Static, Prim, MaterialCensus); }

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

        Snapshot.UniqueMaterialCount = MaterialCensus.Materials.Num();
        Snapshot.UniqueTextureCount = MaterialCensus.Textures.Num();
        Snapshot.TextureResidentBytes = MaterialCensus.TextureResidentBytes;

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

        // ---- Auxiliary views ----
        // The SAME component, the same point of view and the same game-thread scope: only the capture SOURCE
        // changes, so an auxiliary image is this frame described differently rather than a second capture of a world
        // that moved in between. Both sources here are GBuffer reads that need no debug viewmode, which is what
        // keeps them available in a packaged Development build.
        const auto AuxSources = TArray<TPair<const TCHAR*, ESceneCaptureSource>>{
            {TEXT("Base colour"), ESceneCaptureSource::SCS_BaseColor},
            {TEXT("World normal"), ESceneCaptureSource::SCS_Normal}};

        auto FailedAuxCount = 0;

        for (const auto& AuxSource : AuxSources)
        {
            Component->CaptureSource = AuxSource.Value;
            Component->CaptureScene();

            auto AuxPixels = TArray<FColor>{};

            if (NOT Resource->ReadPixels(AuxPixels) || AuxPixels.Num() != Width * Height)
            {
                // An auxiliary view that failed is simply not offered. It never costs the picture, which is the
                // same degradation rule the identity map follows.
                ++FailedAuxCount;
                continue;
            }

            auto AuxPng = TArray64<uint8>{};
            const auto AuxImageView = FImageView{AuxPixels.GetData(), Width, Height, ERawImageFormat::BGRA8};

            if (NOT FImageUtils::CompressImage(AuxPng, TEXT("png"), AuxImageView))
            {
                ++FailedAuxCount;
                continue;
            }

            auto Aux = FCkOptimizationDebugger_SnapshotAuxImage{};
            Aux.Name = FString{AuxSource.Key};
            Aux.Png = MoveTemp(AuxPng);

            Snapshot.AuxImages.Add(MoveTemp(Aux));
        }

        // Back to the colour source before anything else uses the component: the identity passes below read this
        // same component, and a leftover GBuffer source would make every stencil pass a picture of normals.
        Component->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

        // ---- Notes ----
        auto Notes = TArray<FString>{};

        if (FailedAuxCount > 0)
        {
            Notes.Add(ck::Format_UE(TEXT("{} auxiliary view(s) could not be captured and are not offered"),
                FailedAuxCount));
        }

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

        // ---- Identity ----
        // A failure here NEVER discards the colour snapshot and never retries silently: the reader gets the picture
        // plus a sentence saying why it cannot be clicked into. Same rule the cancelled scan follows.
        const auto IdMapFailure = [&Snapshot](const FString& InReason) -> void
        {
            Snapshot.HasIdMap = false;
            Snapshot.CaptureNotes = Snapshot.CaptureNotes.IsEmpty()
                ? InReason
                : ck::Format_UE(TEXT("{}. {}"), Snapshot.CaptureNotes, InReason);
        };

        auto* StencilVisMaterial = TryLoad_StencilVisMaterial();

        if (ck::Is_NOT_Valid(StencilVisMaterial))
        {
            IdMapFailure(TEXT("mesh identification unavailable: the StencilId look master has not been generated "
                "(run Ck_Usf_GenerateLooks StencilId in the editor and commit the result)"));

            return Snapshot;
        }

        if (Snapshot.Prims.IsEmpty())
        {
            IdMapFailure(TEXT("mesh identification skipped: nothing identifiable is in frame"));
            return Snapshot;
        }

        // Declared BEFORE the first mutation, so every exit path below restores what this touched.
        auto Ledger = FStencilLedger{};

        // Stencil has to be written at all. BusterBlock ships with it on for CkUsf's per-object patterns, so this
        // branch is rare — but a capture that silently produced an empty ID map would look like a broken feature.
        if (auto* CustomDepthCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
            CustomDepthCVar != nullptr && CustomDepthCVar->GetInt() < 3)
        {
            Ledger.PriorCustomDepthMode = CustomDepthCVar->GetInt();
            CustomDepthCVar->Set(3, ECVF_SetByCode);
        }

        // Every candidate renders custom depth for the WHOLE capture; batch membership decides the stencil VALUE
        // only. Gating the flag per batch instead would let an out-of-batch occluder go undepth-tested, so a hidden
        // primitive would stamp its id wherever the occluder is visible — the single most likely way to build this
        // wrong, and it looks plausible until you compare silhouettes.
        auto CandidateComponents = TSet<UPrimitiveComponent*>{};

        for (const auto& Candidate : Candidates)
        { CandidateComponents.Add(Candidate.Component); }

        for (auto ActorIt = TActorIterator<AActor>{InWorld}; ActorIt; ++ActorIt)
        {
            auto* Actor = *ActorIt;

            if (ck::Is_NOT_Valid(Actor))
            { continue; }

            for (auto* WorldPrimitive : TInlineComponentArray<UPrimitiveComponent*>{Actor})
            {
                if (WorldPrimitive == nullptr)
                { continue; }

                const auto IsCandidate = CandidateComponents.Contains(WorldPrimitive);

                // A foreign custom-depth user — a CkUsf outline or cel-shade subject — would stamp its own stencil
                // into every pass and be read back as whichever primitive happens to share its value.
                if (NOT IsCandidate && NOT WorldPrimitive->bRenderCustomDepth)
                { continue; }

                Ledger.Record(WorldPrimitive);

                if (IsCandidate)
                {
                    WorldPrimitive->SetRenderCustomDepth(true);
                    continue;
                }

                WorldPrimitive->SetRenderCustomDepth(false);
            }
        }

        auto* IdRenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());

        if (ck::Is_NOT_Valid(IdRenderTarget))
        {
            IdMapFailure(TEXT("mesh identification unavailable: the identity render target could not be created"));
            return Snapshot;
        }

        // LINEAR, unlike the colour target: the stencil byte is data, and an sRGB encode on the way out would warp
        // the values nearest the batch boundaries into their neighbours.
        constexpr auto ForceLinearGammaForIds = true;
        IdRenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, ForceLinearGammaForIds);
        IdRenderTarget->ClearColor = FLinearColor::Black;

        Component->TextureTarget = IdRenderTarget;
        Component->PostProcessBlendWeight = 1.0f;
        Component->PostProcessSettings.WeightedBlendables.Array.Add(
            FWeightedBlendable{1.0f, StencilVisMaterial});

        const auto PassCount = Get_StencilPassCount(Snapshot.Prims.Num());

        auto PerPassStencil = TArray<TArray<uint8>>{};
        PerPassStencil.Reserve(PassCount);

        // Every pass runs in THIS scope with no frame yielded between them: a skeletal pose or a world-position
        // offset that advanced mid-sequence would silhouette differently per pass, and the disagreement would read
        // as a conflict rather than as time having moved.
        for (auto PassIndex = 0; PassIndex < PassCount; ++PassIndex)
        {
            for (auto PrimIndex = 0; PrimIndex < Snapshot.Prims.Num(); ++PrimIndex)
            {
                if (NOT Candidates.IsValidIndex(PrimIndex) || Candidates[PrimIndex].Component == nullptr)
                { continue; }

                const auto Slot = Get_StencilSlot(PrimIndex);

                Candidates[PrimIndex].Component->SetCustomDepthStencilValue(
                    Slot.PassIndex == PassIndex ? static_cast<int32>(Slot.StencilValue) : 0);
            }

            Component->CaptureScene();

            auto PassPixels = TArray<FColor>{};

            if (NOT Resource_ReadIdPass(IdRenderTarget, PassPixels, Width, Height))
            {
                IdMapFailure(ck::Format_UE(TEXT("mesh identification unavailable: reading identity pass {} back "
                    "from the GPU failed"), PassIndex));

                return Snapshot;
            }

            auto PassValues = TArray<uint8>{};
            PassValues.Reserve(PassPixels.Num());

            for (const auto& Pixel : PassPixels)
            { PassValues.Add(Pixel.R); }

            PerPassStencil.Add(MoveTemp(PassValues));
        }

        // ---- Resolve ----
        auto Ids = TArray<uint32>{};
        Ids.Reserve(Width * Height);

        auto ConflictCount = 0;
        auto UnidentifiedCount = 0;
        auto PixelPassValues = TArray<uint8>{};
        PixelPassValues.SetNum(PassCount);

        for (auto PixelIndex = 0; PixelIndex < Width * Height; ++PixelIndex)
        {
            for (auto PassIndex = 0; PassIndex < PassCount; ++PassIndex)
            {
                PixelPassValues[PassIndex] = PerPassStencil[PassIndex].IsValidIndex(PixelIndex)
                    ? PerPassStencil[PassIndex][PixelIndex]
                    : 0;
            }

            const auto Resolved = Resolve_PrimFromPassValues(PixelPassValues, Snapshot.Prims.Num(), ConflictCount);

            if (NOT Resolved.IsSet())
            {
                ++UnidentifiedCount;
                Ids.Add(k_NoPrim);
                continue;
            }

            Ids.Add(static_cast<uint32>(Resolved.GetValue()));
        }

        Snapshot.IdMapRle = Encode_IdMapRle(Ids);
        Snapshot.UnidentifiedPixelCount = UnidentifiedCount;
        Snapshot.HasIdMap = true;

        if (ConflictCount > 0)
        {
            // Reported rather than swallowed: inside one game-thread scope this should be impossible, so a non-zero
            // count is the capture telling on itself.
            Snapshot.CaptureNotes = Snapshot.CaptureNotes.IsEmpty()
                ? ck::Format_UE(TEXT("{} pixel(s) were claimed by more than one identity pass"), ConflictCount)
                : ck::Format_UE(TEXT("{}. {} pixel(s) were claimed by more than one identity pass"),
                    Snapshot.CaptureNotes, ConflictCount);
        }

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_capture
{
    auto
        Dump_DebugImages(
            const FCkOptimizationDebugger_Snapshot& InSnapshot,
            FString& OutFailureReason)
        -> bool
    {
        using namespace ck_optimization_debugger_snapshot;

        OutFailureReason.Reset();

        if (InSnapshot.Width <= 0 || InSnapshot.Height <= 0 || InSnapshot.ColorPng.IsEmpty())
        {
            OutFailureReason = TEXT("the snapshot carries no image to dump");
            return false;
        }

        const auto Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CkOptimizationDebugger"));
        auto& FileManager = IFileManager::Get();

        if (NOT FileManager.MakeDirectory(*Directory, true))
        {
            OutFailureReason = ck::Format_UE(TEXT("could not create {}"), Directory);
            return false;
        }

        // The GUID rather than the label: two snapshots taken in the same second share a label, and a dump that
        // silently overwrote the pair being compared would be the worst possible failure for a debugging aid.
        const auto Stem = InSnapshot.Id.ToString(EGuidFormats::Digits).Left(8);

        const auto ColorPath = FPaths::Combine(Directory, ck::Format_UE(TEXT("Snapshot_{}_Color.png"), Stem));

        if (NOT FFileHelper::SaveArrayToFile(InSnapshot.ColorPng, *ColorPath))
        {
            OutFailureReason = ck::Format_UE(TEXT("could not write {}"), ColorPath);
            return false;
        }

        if (NOT InSnapshot.HasIdMap)
        {
            OutFailureReason = TEXT("wrote the colour image only - this snapshot carries no mesh identification");
            return false;
        }

        const auto Ids = Decode_IdMapRle(InSnapshot.IdMapRle);
        const auto ExpectedPixels = InSnapshot.Width * InSnapshot.Height;

        if (Ids.Num() != ExpectedPixels)
        {
            OutFailureReason = ck::Format_UE(TEXT("the ID map decoded to {} pixel(s) rather than {}"),
                Ids.Num(), ExpectedPixels);

            return false;
        }

        auto IdPixels = TArray<FColor>{};
        IdPixels.Reserve(ExpectedPixels);

        for (const auto& Id : Ids)
        { IdPixels.Add(Get_PrimIndexColor(Id)); }

        auto IdPng = TArray64<uint8>{};
        const auto IdImageView = FImageView{IdPixels.GetData(), InSnapshot.Width, InSnapshot.Height, ERawImageFormat::BGRA8};

        if (NOT FImageUtils::CompressImage(IdPng, TEXT("png"), IdImageView))
        {
            OutFailureReason = TEXT("compressing the ID map image failed");
            return false;
        }

        const auto IdPath = FPaths::Combine(Directory, ck::Format_UE(TEXT("Snapshot_{}_IdMap.png"), Stem));

        if (NOT FFileHelper::SaveArrayToFile(IdPng, *IdPath))
        {
            OutFailureReason = ck::Format_UE(TEXT("could not write {}"), IdPath);
            return false;
        }

        return true;
    }
}
