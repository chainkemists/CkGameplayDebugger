#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_MemoryScan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "ContentStreaming.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableRenderAsset.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget.h"

#include "PixelFormat.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named anonymous helper in another .cpp would
// collide in the merged translation unit.
namespace ck_optimization_debugger_memoryscan
{
    /** Whether an object is one the reader can act on at all.
     *
     *  ONE rule, and it is package-scoped: templates are skipped (a CDO's texture is not resident content), and so is
     *  anything whose package is the transient package or lives under `/Engine/Transient`. That second test is what
     *  removes editor thumbnails, asset-preview render targets and every other piece of scratch the editor keeps
     *  loaded — they are all parented to a transient package, which IS the `RF_Transient` outer.
     *
     *  Deliberately NOT a test of the object's own `RF_Transient` flag: an object flagged transient inside a real
     *  package is still a thing with a path the reader can navigate to, and dropping it would make the row count
     *  disagree with what the Content Browser shows for no reason the reader could see. The consequence — a row
     *  always names a real asset path — is what makes the table's double-click action meaningful. */
    auto
        Should_Skip(
            const UObject* InObject)
        -> bool
    {
        if (ck::Is_NOT_Valid(InObject))
        { return true; }

        if (InObject->IsTemplate())
        { return true; }

        const auto* Package = InObject->GetPackage();

        if (ck::Is_NOT_Valid(Package))
        { return true; }

        if (Package == GetTransientPackage())
        { return true; }

        static const auto k_TransientPathPrefix = FString{TEXT("/Engine/Transient")};

        return Package->GetName().StartsWith(k_TransientPathPrefix, ESearchCase::CaseSensitive);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The two size figures every row carries.
     *
     *  `EResourceSizeMode::Exclusive` is the mode, deliberately: it counts what the object is costing RIGHT NOW —
     *  for a texture, the resident mips — where `EstimatedTotal` counts what it would cost fully loaded plus its
     *  serialized `UObject` graph. This page answers "what is resident", so the estimate would be the wrong number
     *  in the one place the reader is most likely to trust it.
     *
     *  The GPU figure is the video-memory bucket of the same accumulator, which only textures fill in; a mesh and a
     *  render target both fold their whole cost into the untagged bucket. `HasSeparableGpuSize` is what stops the
     *  column reading "0 bytes on the GPU" for those two — a claim neither of them made.
     *
     *  That flag is a property of the row's TABLE, never of the number that came back. Deriving it from
     *  `GpuSizeBytes > 0` conflated two different statements: "this type reports no separable figure" and "this
     *  instance currently has none resident". A fully streamed-out texture reports zero dedicated bytes and is
     *  still a type that reports them — and on a page whose whole claim is "what is resident", `0 B` is the true
     *  answer there, where an em dash would say the measurement was impossible.
     *
     *  Requires `OutRow.Table` to be set before this is called; both builders do. */
    auto
        Apply_ResourceSize(
            UObject* InObject,
            FCkOptimizationDebugger_MemoryRow& OutRow)
        -> void
    {
        if (ck::Is_NOT_Valid(InObject))
        { return; }

        auto Size = FResourceSizeEx{EResourceSizeMode::Exclusive};
        InObject->GetResourceSizeEx(Size);

        OutRow.ResourceSizeBytes = static_cast<int64>(Size.GetTotalMemoryBytes());
        OutRow.GpuSizeBytes = static_cast<int64>(Size.GetDedicatedVideoMemoryBytes());
        OutRow.HasSeparableGpuSize =
            ck_optimization_debugger_model::Has_SeparableGpuSize(OutRow.Table);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The resident / wanted LOD pair, when there is one to read.
     *
     *  The numbers themselves come off the ASSET (`GetStreamableResourceState`), not off the manager — but they are
     *  only meaningful while a streamer is actually running, which is what the availability argument gates. With
     *  streaming off, `NumResidentLODs` describes a state nothing is maintaining. */
    auto
        Apply_StreamingState(
            const UStreamableRenderAsset* InAsset,
            ECkOptimizationDebugger_StreamingAvailability InAvailability,
            FCkOptimizationDebugger_MemoryRow& OutRow)
        -> void
    {
        if (ck::Is_NOT_Valid(InAsset))
        { return; }

        OutRow.IsStreamable = InAsset->IsStreamable();

        if (InAvailability != ECkOptimizationDebugger_StreamingAvailability::Available)
        { return; }

        if (NOT OutRow.IsStreamable)
        { return; }

        const auto& State = InAsset->GetStreamableResourceState();

        // An invalid state is the ordinary answer for an asset whose render resources have not been created yet —
        // reporting its zeroes as a measurement would say "nothing is resident" about something nobody asked.
        if (NOT State.IsValid())
        { return; }

        OutRow.HasStreamingMetrics = true;
        OutRow.ResidentMipCount = static_cast<int32>(State.NumResidentLODs);
        OutRow.RequestedMipCount = static_cast<int32>(State.NumRequestedLODs);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** "World" rather than "TEXTUREGROUP_World" — the engine's own string with its enum prefix removed, so the
     *  column reads as a texture group and not as a C identifier. */
    auto
        Get_TextureGroupLabel(
            TextureGroup InGroup)
        -> FString
    {
        auto Label = FString{UTexture::GetTextureGroupString(InGroup)};
        Label.RemoveFromStart(TEXT("TEXTUREGROUP_"), ESearchCase::CaseSensitive);

        return Label;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_TextureRow(
            UTexture* InTexture,
            ECkOptimizationDebugger_StreamingAvailability InAvailability)
        -> FCkOptimizationDebugger_MemoryRow
    {
        auto Row = FCkOptimizationDebugger_MemoryRow{};

        auto* RenderTarget = Cast<UTextureRenderTarget>(InTexture);

        Row.Table = RenderTarget != nullptr
            ? ECkOptimizationDebugger_MemoryTable::RenderTargets
            : ECkOptimizationDebugger_MemoryTable::Textures;

        Row.AssetPath = InTexture->GetPathName();
        Row.DisplayName = InTexture->GetName();
        Row.ClassName = InTexture->GetClass()->GetName();

        // The generic surface accessors, which every texture type implements — a cube map and a volume texture answer
        // the same two questions a 2D texture does without this needing to know which it got.
        Row.Width = static_cast<int32>(InTexture->GetSurfaceWidth());
        Row.Height = static_cast<int32>(InTexture->GetSurfaceHeight());

        auto Format = EPixelFormat::PF_Unknown;

        if (RenderTarget != nullptr)
        {
            Format = RenderTarget->GetFormat();
        }
        else if (const auto* Texture2D = Cast<UTexture2D>(InTexture))
        {
            // `GetPixelFormat` / `GetNumMips` read the platform data pointer directly. `GetPlatformData()` — the
            // accessor they superficially resemble — BLOCKS on an in-flight texture build, and an audit tool must
            // never make the editor finish compiling a texture in order to describe it.
            Format = Texture2D->GetPixelFormat();
            Row.MipCount = Texture2D->GetNumMips();
        }

        if (Format != EPixelFormat::PF_Unknown)
        { Row.FormatName = FString{GetPixelFormatString(Format)}; }

        const auto& State = InTexture->GetStreamableResourceState();

        // Cube and volume textures have no `GetNumMips` of their own on this path; the streamer's own LOD ceiling is
        // the same count, and it is zero when there is no render resource, which prints as no mip figure at all.
        if (Row.MipCount == 0 && State.IsValid())
        { Row.MipCount = static_cast<int32>(State.MaxNumLODs); }

        Row.LodGroupName = Get_TextureGroupLabel(InTexture->LODGroup);

        Apply_ResourceSize(InTexture, Row);
        Apply_StreamingState(InTexture, InAvailability, Row);

        return Row;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_StaticMeshRow(
            UStaticMesh* InMesh,
            ECkOptimizationDebugger_StreamingAvailability InAvailability)
        -> FCkOptimizationDebugger_MemoryRow
    {
        auto Row = FCkOptimizationDebugger_MemoryRow{};

        Row.Table = ECkOptimizationDebugger_MemoryTable::StaticMeshes;

        Row.AssetPath = InMesh->GetPathName();
        Row.DisplayName = InMesh->GetName();
        Row.ClassName = InMesh->GetClass()->GetName();

        // Both read through the render data with their own validity checks and answer 0 when there is none — a mesh
        // whose render data has not been built yet reports zeros rather than costing this scan a build.
        Row.LodCount = InMesh->GetNumLODs();
        Row.Lod0TriangleCount = InMesh->GetNumTriangles(0);

        Apply_ResourceSize(InMesh, Row);
        Apply_StreamingState(InMesh, InAvailability, Row);

        return Row;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_memory
{
    auto
        Get_StreamingAvailability()
        -> ECkOptimizationDebugger_StreamingAvailability
    {
        // `Get_Concurrent` returns null both before the collection is allocated and after `IStreamingManager::
        // Shutdown` has poisoned it. `Get()` would CONSTRUCT one instead — a tool that summoned the subsystem it was
        // asked to describe would be reporting on its own side effect.
        const auto* Collection = IStreamingManager::Get_Concurrent();

        if (Collection == nullptr)
        { return ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable; }

        return Collection->IsTextureStreamingEnabled()
            ? ECkOptimizationDebugger_StreamingAvailability::Available
            : ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_MemoryScan()
        -> FCkOptimizationDebugger_MemoryScanResult
    {
        using namespace ck_optimization_debugger_memoryscan;

        auto Result = FCkOptimizationDebugger_MemoryScanResult{};

        // Asked ONCE, before the walk. Asking per row would let a manager that shut down half way through produce a
        // table whose first half claims a measurement its second half says was impossible.
        Result.StreamingAvailability = Get_StreamingAvailability();

        // Class default objects and archetypes are excluded by the iterator itself; `Should_Skip` handles the rest.
        // One pass per family rather than one over `UObject`: the iterator's own class filter is what makes each
        // pass cheap, and the three families do not overlap.
        for (auto It = TObjectIterator<UTexture>{RF_ClassDefaultObject | RF_ArchetypeObject}; It; ++It)
        {
            auto* Texture = *It;

            if (Should_Skip(Texture))
            { continue; }

            Result.Rows.Add(Build_TextureRow(Texture, Result.StreamingAvailability));
        }

        for (auto It = TObjectIterator<UStaticMesh>{RF_ClassDefaultObject | RF_ArchetypeObject}; It; ++It)
        {
            auto* Mesh = *It;

            if (Should_Skip(Mesh))
            { continue; }

            Result.Rows.Add(Build_StaticMeshRow(Mesh, Result.StreamingAvailability));
        }

        // Sorted by path before anything renders. Object-iteration order follows the allocation order of the UObject
        // array, which two identical sessions do not share — and a table that came out in a different order every
        // time would make the sort the reader chose look like it had not been applied.
        Result.Rows.Sort([](const FCkOptimizationDebugger_MemoryRow& InLhs,
                            const FCkOptimizationDebugger_MemoryRow& InRhs)
        {
            return InLhs.AssetPath.Compare(InRhs.AssetPath, ESearchCase::CaseSensitive) < 0;
        });

        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------
