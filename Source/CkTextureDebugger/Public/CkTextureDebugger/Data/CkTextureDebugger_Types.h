#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UMaterialInterface;
class UPrimitiveComponent;
class UTexture;

// --------------------------------------------------------------------------------------------------------------------

/** Why streaming figures on a texture row are, or are not, meaningful. */
enum class ECkTextureDebugger_StreamingAvailability : uint8
{
    Available,
    ManagerUnavailable,
    StreamingDisabled,
    NotStreamable,
    ResourceNotCreated
};
/** Renderable component families the checker workflow understands. */
enum class ECkTextureDebugger_ComponentKind : uint8
{
    StaticMesh,
    SkeletalMesh,
    InstancedStaticMesh,
    HierarchicalInstancedStaticMesh,
    FoliageInstancedStaticMesh,
    OtherPrimitive
};

// --------------------------------------------------------------------------------------------------------------------

/** Copied health facts. Navigation targets stay weak on their owning rows; this row never owns a UObject. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_TextureHealth
{
    FSoftObjectPath AssetPath;
    FString         DisplayName;
    FString         ClassName;
    FString         FormatName;
    FString         LodGroupName;

    int32 CookedWidth = 0;
    int32 CookedHeight = 0;
    int32 MipCount = 0;
    int64 ResidentBytes = 0;
    int64 DedicatedVideoBytes = 0;

    ECkTextureDebugger_StreamingAvailability StreamingAvailability =
        ECkTextureDebugger_StreamingAvailability::ManagerUnavailable;

    bool IsStreamable = false;
    bool SupportsVirtualStreaming = false;
    bool HasStreamingMetrics = false;

    int32 ResidentMipCount = 0;
    int32 RequestedMipCount = 0;
    int32 MaxMipCount = 0;
    int32 AssetLodBias = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/** One texture referenced by the component-resolved material for a slot. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_TextureRow
{
    TWeakObjectPtr<UTexture> NavigationTarget;
    FCkTextureDebugger_TextureHealth Health;
};

/** One component-resolved material slot. A null material remains an explicit row. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_MaterialSlotRow
{
    int32 SlotIndex = INDEX_NONE;
    FName SlotName;

    TWeakObjectPtr<UMaterialInterface> NavigationTarget;
    FSoftObjectPath MaterialPath;
    FString DisplayName;

    TArray<FCkTextureDebugger_TextureRow> Textures;
};

/** A value-first description of a loaded primitive. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_ComponentRow
{
    TWeakObjectPtr<UPrimitiveComponent> NavigationTarget;

    FSoftObjectPath ActorPath;
    FString ActorDisplayName;
    FString ComponentDisplayName;
    FString ComponentClassName;

    ECkTextureDebugger_ComponentKind Kind = ECkTextureDebugger_ComponentKind::OtherPrimitive;
    int32 InstanceCount = 0;
    bool SupportsCheckerOverride = false;
    bool HasComponentSlotOverlay = false;

    TArray<FCkTextureDebugger_MaterialSlotRow> MaterialSlots;
};

/** Complete result of a loaded-world walk. It never causes a streaming load. */
struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_LoadedWorldSnapshot
{
    TWeakObjectPtr<UWorld> World;
    ECkTextureDebugger_StreamingAvailability StreamingAvailability =
        ECkTextureDebugger_StreamingAvailability::ManagerUnavailable;
    TArray<FCkTextureDebugger_ComponentRow> Components;
};
