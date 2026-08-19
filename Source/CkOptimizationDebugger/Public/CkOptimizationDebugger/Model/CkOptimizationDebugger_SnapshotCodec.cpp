#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotCodec.h"

#include "CkCore/Macros/CkMacros.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_debugger_snapshot_codec_impl
{
    constexpr auto k_Magic = static_cast<uint32>(0x434B534E);  // 'CKSN'

    // Far above any real snapshot, far below anything that could amplify a hostile count into an allocation bomb.
    constexpr auto k_MaxSaneCount = 1'000'000;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Write_Bytes64(
            FMemoryWriter& InWriter,
            const TArray64<uint8>& InBytes)
        -> void
    {
        auto Count = InBytes.Num();
        InWriter << Count;

        if (Count > 0)
        { InWriter.Serialize(const_cast<uint8*>(InBytes.GetData()), Count); }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Read_Bytes64(
            FMemoryReader& InReader,
            TArray64<uint8>& OutBytes)
        -> bool
    {
        auto Count = static_cast<int64>(0);
        InReader << Count;

        if (InReader.IsError() || Count < 0 || Count > InReader.TotalSize() - InReader.Tell())
        { return false; }

        OutBytes.SetNumUninitialized(Count);

        if (Count > 0)
        { InReader.Serialize(OutBytes.GetData(), Count); }

        return NOT InReader.IsError();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** A count that gates a loop of further reads. Bounded by the remaining bytes as well as the sanity cap, so a
     *  corrupted count fails the decode instead of spinning a near-empty reader through a million iterations. */
    auto
        Read_Count(
            FMemoryReader& InReader,
            int32& OutCount)
        -> bool
    {
        InReader << OutCount;

        return NOT InReader.IsError()
            && OutCount >= 0
            && OutCount <= k_MaxSaneCount
            && OutCount <= InReader.TotalSize() - InReader.Tell();
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_codec
{
    auto
        Encode_SnapshotFile(
            const FCkOptimizationDebugger_Snapshot& InSnapshot)
        -> TArray<uint8>
    {
        using namespace ck_optimization_debugger_snapshot_codec_impl;

        auto Bytes = TArray<uint8>{};
        auto Writer = FMemoryWriter{Bytes};

        auto Magic = k_Magic;
        auto Version = k_SnapshotFileVersion;
        Writer << Magic;
        Writer << Version;

        auto Snapshot = InSnapshot;

        Writer << Snapshot.Id;
        Writer << Snapshot.Label;

        auto CapturedAtTicks = Snapshot.CapturedAt.GetTicks();
        Writer << CapturedAtTicks;

        Writer << Snapshot.WorldName;
        Writer << Snapshot.Width;
        Writer << Snapshot.Height;

        Write_Bytes64(Writer, Snapshot.ColorPng);

        Writer << Snapshot.HasIdMap;
        Writer << Snapshot.IdMapRle;
        Writer << Snapshot.UnidentifiedPixelCount;
        Writer << Snapshot.CaptureNotes;
        Writer << Snapshot.UniqueMaterialCount;
        Writer << Snapshot.UniqueTextureCount;
        Writer << Snapshot.TextureResidentBytes;

        auto PrimCount = Snapshot.Prims.Num();
        Writer << PrimCount;

        for (auto& Prim : Snapshot.Prims)
        {
            Writer << Prim.DisplayName;
            Writer << Prim.MeshDisplayName;

            auto MeshPath = Prim.MeshAssetPath.ToString();
            Writer << MeshPath;

            auto Kind = static_cast<uint8>(Prim.Kind);
            Writer << Kind;

            Writer << Prim.IsNanite;
            Writer << Prim.InstanceCount;
            Writer << Prim.DistanceFromCamera;
            Writer << Prim.MeshResourceSizeBytes;

            auto LodCount = Prim.Lods.Num();
            Writer << LodCount;

            for (auto& Lod : Prim.Lods)
            {
                Writer << Lod.Triangles;
                Writer << Lod.Sections;
                Writer << Lod.Vertices;
                Writer << Lod.ScreenSize;
            }

            auto SlotCount = Prim.MaterialSlots.Num();
            Writer << SlotCount;

            for (auto& Slot : Prim.MaterialSlots)
            {
                Writer << Slot.SlotName;
                Writer << Slot.MaterialName;

                auto MaterialPath = Slot.MaterialPath.ToString();
                Writer << MaterialPath;

                Writer << Slot.BlendMode;
                Writer << Slot.ShadingModel;
                Writer << Slot.IsTwoSided;
                Writer << Slot.UsedTextureCount;
                Writer << Slot.UsedTextureNames;
            }
        }

        // SORTED, not set order: iteration order of a TSet is an implementation detail, and a file that hashed
        // differently for the same selection would fail its own round-trip determinism.
        auto Selection = Snapshot.SelectedPrims.Array();
        Selection.Sort();
        Writer << Selection;

        return Bytes;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Decode_SnapshotFile(
            const TArray<uint8>& InBytes)
        -> TOptional<FCkOptimizationDebugger_Snapshot>
    {
        using namespace ck_optimization_debugger_snapshot_codec_impl;

        auto Reader = FMemoryReader{InBytes};

        auto Magic = static_cast<uint32>(0);
        auto Version = static_cast<uint32>(0);
        Reader << Magic;
        Reader << Version;

        if (Reader.IsError() || Magic != k_Magic || Version != k_SnapshotFileVersion)
        { return {}; }

        auto Snapshot = FCkOptimizationDebugger_Snapshot{};

        Reader << Snapshot.Id;
        Reader << Snapshot.Label;

        auto CapturedAtTicks = static_cast<int64>(0);
        Reader << CapturedAtTicks;
        Snapshot.CapturedAt = FDateTime{CapturedAtTicks};

        Reader << Snapshot.WorldName;
        Reader << Snapshot.Width;
        Reader << Snapshot.Height;

        if (NOT Read_Bytes64(Reader, Snapshot.ColorPng))
        { return {}; }

        Reader << Snapshot.HasIdMap;
        Reader << Snapshot.IdMapRle;
        Reader << Snapshot.UnidentifiedPixelCount;
        Reader << Snapshot.CaptureNotes;
        Reader << Snapshot.UniqueMaterialCount;
        Reader << Snapshot.UniqueTextureCount;
        Reader << Snapshot.TextureResidentBytes;

        auto PrimCount = 0;

        if (NOT Read_Count(Reader, PrimCount))
        { return {}; }

        Snapshot.Prims.Reserve(PrimCount);

        for (auto PrimIndex = 0; PrimIndex < PrimCount; ++PrimIndex)
        {
            auto Prim = FCkOptimizationDebugger_SnapshotPrim{};

            Reader << Prim.DisplayName;
            Reader << Prim.MeshDisplayName;

            auto MeshPath = FString{};
            Reader << MeshPath;
            Prim.MeshAssetPath = FSoftObjectPath{MeshPath};

            auto Kind = static_cast<uint8>(0);
            Reader << Kind;
            Prim.Kind = static_cast<ECkOptimizationDebugger_SnapshotPrimKind>(Kind);

            Reader << Prim.IsNanite;
            Reader << Prim.InstanceCount;
            Reader << Prim.DistanceFromCamera;
            Reader << Prim.MeshResourceSizeBytes;

            auto LodCount = 0;

            if (NOT Read_Count(Reader, LodCount))
            { return {}; }

            Prim.Lods.Reserve(LodCount);

            for (auto LodIndex = 0; LodIndex < LodCount; ++LodIndex)
            {
                auto Lod = FCkOptimizationDebugger_SnapshotLod{};

                Reader << Lod.Triangles;
                Reader << Lod.Sections;
                Reader << Lod.Vertices;
                Reader << Lod.ScreenSize;

                Prim.Lods.Add(Lod);
            }

            auto SlotCount = 0;

            if (NOT Read_Count(Reader, SlotCount))
            { return {}; }

            Prim.MaterialSlots.Reserve(SlotCount);

            for (auto SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
            {
                auto Slot = FCkOptimizationDebugger_SnapshotMaterialSlot{};

                Reader << Slot.SlotName;
                Reader << Slot.MaterialName;

                auto MaterialPath = FString{};
                Reader << MaterialPath;
                Slot.MaterialPath = FSoftObjectPath{MaterialPath};

                Reader << Slot.BlendMode;
                Reader << Slot.ShadingModel;
                Reader << Slot.IsTwoSided;
                Reader << Slot.UsedTextureCount;
                Reader << Slot.UsedTextureNames;

                if (Reader.IsError())
                { return {}; }

                Prim.MaterialSlots.Add(MoveTemp(Slot));
            }

            if (Reader.IsError())
            { return {}; }

            Snapshot.Prims.Add(MoveTemp(Prim));
        }

        auto Selection = TArray<int32>{};
        Reader << Selection;

        if (Reader.IsError())
        { return {}; }

        for (const auto& PrimIndex : Selection)
        {
            // A selection index the table cannot back is dropped here rather than trusted — the same distrust every
            // click already applies, moved to the one other place a selection can enter from.
            if (Snapshot.Prims.IsValidIndex(PrimIndex))
            { Snapshot.SelectedPrims.Add(PrimIndex); }
        }

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------
