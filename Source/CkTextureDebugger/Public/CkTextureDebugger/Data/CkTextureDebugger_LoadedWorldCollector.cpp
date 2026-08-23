#include "CkTextureDebugger/Data/CkTextureDebugger_LoadedWorldCollector.h"

#include "CkTextureDebugger/Data/CkTextureDebugger_TextureHealth.h"

#include "CkCore/Macros/CkMacros.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/Texture.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_loaded_world_collector
{
    auto
        Get_ComponentKind(
            const UPrimitiveComponent* InComponent) -> ECkTextureDebugger_ComponentKind
    {
        if (InComponent == nullptr)
        { return ECkTextureDebugger_ComponentKind::OtherPrimitive; }

        if (InComponent->IsA<UFoliageInstancedStaticMeshComponent>())
        { return ECkTextureDebugger_ComponentKind::FoliageInstancedStaticMesh; }

        if (InComponent->IsA<UHierarchicalInstancedStaticMeshComponent>())
        { return ECkTextureDebugger_ComponentKind::HierarchicalInstancedStaticMesh; }

        if (InComponent->IsA<UInstancedStaticMeshComponent>())
        { return ECkTextureDebugger_ComponentKind::InstancedStaticMesh; }

        if (InComponent->IsA<USkeletalMeshComponent>())
        { return ECkTextureDebugger_ComponentKind::SkeletalMesh; }

        if (InComponent->IsA<UStaticMeshComponent>())
        { return ECkTextureDebugger_ComponentKind::StaticMesh; }

        return ECkTextureDebugger_ComponentKind::OtherPrimitive;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_MaterialSlot(
            UPrimitiveComponent* InComponent,
            int32 InSlotIndex,
            const TArray<FName>& InSlotNames,
            ECkTextureDebugger_StreamingAvailability InStreamingAvailability) -> FCkTextureDebugger_MaterialSlotRow
    {
        auto Result = FCkTextureDebugger_MaterialSlotRow{};
        Result.SlotIndex = InSlotIndex;
        Result.SlotName = InSlotNames.IsValidIndex(InSlotIndex) ? InSlotNames[InSlotIndex] : NAME_None;

        auto* Material = InComponent->GetMaterial(InSlotIndex);
        Result.NavigationTarget = Material;

        if (Material == nullptr)
        {
            Result.DisplayName = TEXT("(empty)");
            return Result;
        }

        Result.MaterialPath = FSoftObjectPath{Material};
        Result.DisplayName = Material->GetName();

        auto UsedTextures = TArray<UTexture*>{};
        Material->GetUsedTextures(UsedTextures);

        auto SeenTextures = TSet<FSoftObjectPath>{};
        for (auto* Texture : UsedTextures)
        {
            if (Texture == nullptr)
            { continue; }

            const auto TexturePath = FSoftObjectPath{Texture};
            if (SeenTextures.Contains(TexturePath))
            { continue; }

            SeenTextures.Add(TexturePath);

            auto TextureRow = FCkTextureDebugger_TextureRow{};
            TextureRow.NavigationTarget = Texture;
            TextureRow.Health = ck::texture_debugger::health::Describe(Texture, InStreamingAvailability);
            Result.Textures.Add(MoveTemp(TextureRow));
        }

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Component(
            AActor* InActor,
            UPrimitiveComponent* InComponent,
            ECkTextureDebugger_StreamingAvailability InStreamingAvailability) -> FCkTextureDebugger_ComponentRow
    {
        auto Result = FCkTextureDebugger_ComponentRow{};
        Result.NavigationTarget = InComponent;
        Result.ActorPath = FSoftObjectPath{InActor};
        Result.ActorDisplayName = InActor->GetActorNameOrLabel();
        Result.ComponentDisplayName = InComponent->GetName();
        Result.ComponentClassName = InComponent->GetClass()->GetName();
        Result.Kind = Get_ComponentKind(InComponent);

        if (const auto* Instanced = Cast<UInstancedStaticMeshComponent>(InComponent))
        { Result.InstanceCount = Instanced->GetInstanceCount(); }

        const auto* MeshComponent = Cast<UMeshComponent>(InComponent);
        Result.SupportsCheckerOverride = MeshComponent != nullptr;
        Result.HasComponentSlotOverlay = MeshComponent != nullptr &&
            NOT MeshComponent->GetComponentMaterialSlotsOverlayMaterial().IsEmpty();

        const auto SlotNames = InComponent->GetMaterialSlotNames();
        const auto MaterialCount = InComponent->GetNumMaterials();
        for (auto SlotIndex = 0; SlotIndex < MaterialCount; ++SlotIndex)
        {
            Result.MaterialSlots.Add(Build_MaterialSlot(
                InComponent, SlotIndex, SlotNames, InStreamingAvailability));
        }

        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::collector
{
    auto
        Collect_LoadedWorld(
            UWorld* InWorld) -> FCkTextureDebugger_LoadedWorldSnapshot
    {
        auto Result = FCkTextureDebugger_LoadedWorldSnapshot{};
        Result.World = InWorld;
        Result.StreamingAvailability = ck::texture_debugger::health::Get_StreamingAvailability();

        if (InWorld == nullptr)
        { return Result; }

        for (auto ActorIt = TActorIterator<AActor>{InWorld}; ActorIt; ++ActorIt)
        {
            auto* Actor = *ActorIt;
            if (Actor == nullptr || Actor->IsActorBeingDestroyed())
            { continue; }

            for (auto* Component : TInlineComponentArray<UPrimitiveComponent*>{Actor})
            {
                if (Component == nullptr || NOT Component->IsRegistered())
                { continue; }

                Result.Components.Add(ck_texture_debugger_loaded_world_collector::Build_Component(
                    Actor, Component, Result.StreamingAvailability));
            }
        }

        return Result;
    }
}
