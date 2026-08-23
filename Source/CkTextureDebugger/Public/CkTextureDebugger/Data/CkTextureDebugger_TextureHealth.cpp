#include "CkTextureDebugger/Data/CkTextureDebugger_TextureHealth.h"

#include "CkCore/Macros/CkMacros.h"

#include "ContentStreaming.h"
#include "Engine/StreamableRenderAsset.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget.h"
#include "PixelFormat.h"
#include "ProfilingDebugging/ResourceSize.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_texture_health
{
    auto
        Get_TextureGroupLabel(
            TextureGroup InGroup) -> FString
    {
        auto Label = FString{UTexture::GetTextureGroupString(InGroup)};
        Label.RemoveFromStart(TEXT("TEXTUREGROUP_"), ESearchCase::CaseSensitive);
        return Label;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::health
{
    auto
        Get_StreamingAvailability() -> ECkTextureDebugger_StreamingAvailability
    {
        // `Get()` would create a collection. A diagnostic must not alter the session it measures.
        const auto* Collection = IStreamingManager::Get_Concurrent();
        if (Collection == nullptr)
        { return ECkTextureDebugger_StreamingAvailability::ManagerUnavailable; }

        return Collection->IsTextureStreamingEnabled()
            ? ECkTextureDebugger_StreamingAvailability::Available
            : ECkTextureDebugger_StreamingAvailability::StreamingDisabled;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Describe(
            UTexture* InTexture,
            ECkTextureDebugger_StreamingAvailability InStreamingAvailability) -> FCkTextureDebugger_TextureHealth
    {
        auto Result = FCkTextureDebugger_TextureHealth{};
        Result.StreamingAvailability = InStreamingAvailability;

        if (InTexture == nullptr)
        { return Result; }

        Result.AssetPath = FSoftObjectPath{InTexture};
        Result.DisplayName = InTexture->GetName();
        Result.ClassName = InTexture->GetClass()->GetName();
        Result.CookedWidth = static_cast<int32>(InTexture->GetSurfaceWidth());
        Result.CookedHeight = static_cast<int32>(InTexture->GetSurfaceHeight());
        Result.LodGroupName = ck_texture_debugger_texture_health::Get_TextureGroupLabel(InTexture->LODGroup);

        auto PixelFormat = EPixelFormat::PF_Unknown;
        if (const auto* Texture2D = Cast<UTexture2D>(InTexture))
        {
            // These access cached platform data. Do not call GetPlatformData: it can block on a build.
            PixelFormat = Texture2D->GetPixelFormat();
            Result.MipCount = Texture2D->GetNumMips();
        }
        else if (const auto* RenderTarget = Cast<UTextureRenderTarget>(InTexture))
        {
            PixelFormat = RenderTarget->GetFormat();
        }

        if (PixelFormat != EPixelFormat::PF_Unknown)
        { Result.FormatName = FString{GetPixelFormatString(PixelFormat)}; }

        auto Size = FResourceSizeEx{EResourceSizeMode::Exclusive};
        InTexture->GetResourceSizeEx(Size);
        Result.ResidentBytes = static_cast<int64>(Size.GetTotalMemoryBytes());
        Result.DedicatedVideoBytes = static_cast<int64>(Size.GetDedicatedVideoMemoryBytes());

        const auto& State = InTexture->GetStreamableResourceState();
        Result.IsStreamable = InTexture->IsStreamable();

        if (Result.MipCount == 0 && State.IsValid())
        { Result.MipCount = static_cast<int32>(State.MaxNumLODs); }

        if (InStreamingAvailability != ECkTextureDebugger_StreamingAvailability::Available)
        { return Result; }

        if (NOT Result.IsStreamable)
        {
            Result.StreamingAvailability = ECkTextureDebugger_StreamingAvailability::NotStreamable;
            return Result;
        }

        if (NOT State.IsValid())
        {
            Result.StreamingAvailability = ECkTextureDebugger_StreamingAvailability::ResourceNotCreated;
            return Result;
        }

        Result.HasStreamingMetrics = true;
        Result.SupportsVirtualStreaming = State.bSupportsVirtualStreaming != 0;
        Result.ResidentMipCount = static_cast<int32>(State.NumResidentLODs);
        Result.RequestedMipCount = static_cast<int32>(State.NumRequestedLODs);
        Result.MaxMipCount = static_cast<int32>(State.MaxNumLODs);
        Result.AssetLodBias = static_cast<int32>(State.AssetLODBias);

        return Result;
    }
}
