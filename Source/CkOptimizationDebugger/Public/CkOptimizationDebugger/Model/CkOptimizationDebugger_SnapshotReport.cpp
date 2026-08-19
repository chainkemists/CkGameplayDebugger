#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotReport.h"

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotLens.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "ImageUtils.h"
#include "Misc/Base64.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_debugger_snapshot_report_impl
{
    /** Names come from artist-authored assets and can carry anything; unescaped they would let one asset name break
     *  the whole report's markup. */
    auto
        Escape_Html(
            const FString& InText)
        -> FString
    {
        auto Escaped = InText;

        Escaped.ReplaceInline(TEXT("&"), TEXT("&amp;"));
        Escaped.ReplaceInline(TEXT("<"), TEXT("&lt;"));
        Escaped.ReplaceInline(TEXT(">"), TEXT("&gt;"));
        Escaped.ReplaceInline(TEXT("\""), TEXT("&quot;"));

        return Escaped;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_IdMapPngBase64(
            const FCkOptimizationDebugger_Snapshot& InSnapshot,
            const TArray<uint32>& InIds)
        -> FString
    {
        using namespace ck_optimization_debugger_snapshot;

        if (NOT InSnapshot.HasIdMap)
        { return {}; }

        const auto& Ids = InIds;
        const auto ExpectedPixels = InSnapshot.Width * InSnapshot.Height;

        if (Ids.Num() != ExpectedPixels)
        { return {}; }

        auto Pixels = TArray<FColor>{};
        Pixels.Reserve(ExpectedPixels);

        for (const auto& Id : Ids)
        { Pixels.Add(Get_PrimIndexColor(Id)); }

        auto Png = TArray64<uint8>{};
        const auto ImageView = FImageView{Pixels.GetData(), InSnapshot.Width, InSnapshot.Height, ERawImageFormat::BGRA8};

        if (NOT FImageUtils::CompressImage(Png, TEXT("png"), ImageView))
        { return {}; }

        return FBase64::Encode(Png.GetData(), Png.Num());
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_report
{
    auto
        Build_SnapshotReportHtml(
            const FCkOptimizationDebugger_Snapshot& InSnapshot,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        using namespace ck_optimization_debugger_model;
        using namespace ck_optimization_debugger_snapshot;
        using namespace ck_optimization_debugger_snapshot_lens;
        using namespace ck_optimization_debugger_snapshot_report_impl;

        const auto Aggregates = Get_SnapshotAggregates(InSnapshot.Prims);

        // Decoded ONCE and shared by the identification image and the coverage columns — a second decode of a map
        // that is one uint32 per pixel would double what a report costs for the same answer.
        const auto DecodedIds = InSnapshot.HasIdMap ? Decode_IdMapRle(InSnapshot.IdMapRle) : TArray<uint32>{};
        const auto Coverage = Get_ScreenCoverage(DecodedIds, InSnapshot.Prims.Num());

        auto Html = FString{};
        Html.Reserve(64 * 1024);

        Html += TEXT("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n");
        Html += ck::Format_UE(TEXT("<title>Snapshot report — {}</title>\n"), Escape_Html(InSnapshot.Label));
        Html += TEXT("<style>\n"
            "body { font-family: 'Segoe UI', sans-serif; background: #1a1a1e; color: #d8d8dc; margin: 24px; }\n"
            "h1, h2 { color: #ffffff; font-weight: 600; }\n"
            "table { border-collapse: collapse; margin: 12px 0; }\n"
            "th, td { border: 1px solid #3a3a40; padding: 4px 10px; text-align: left; font-size: 13px; }\n"
            "th { background: #26262c; }\n"
            "td.num { text-align: right; font-variant-numeric: tabular-nums; }\n"
            "img { max-width: 100%; border: 1px solid #3a3a40; margin: 6px 0; }\n"
            ".dim { color: #8a8a92; font-size: 12px; }\n"
            "</style>\n</head>\n<body>\n");

        // ---- Facts ----
        Html += ck::Format_UE(TEXT("<h1>{}</h1>\n"), Escape_Html(InSnapshot.Label));
        Html += ck::Format_UE(TEXT("<p class=\"dim\">World: {} &middot; {}x{} &middot; captured {} &middot; report generated {}</p>\n"),
            Escape_Html(InSnapshot.WorldName), InSnapshot.Width, InSnapshot.Height,
            InSnapshot.CapturedAt.ToString(), InGeneratedAt.ToString());

        if (NOT InSnapshot.CaptureNotes.IsEmpty())
        { Html += ck::Format_UE(TEXT("<p class=\"dim\">{}</p>\n"), Escape_Html(InSnapshot.CaptureNotes)); }

        // Where it was taken from, and what it was rendered at. Both are what turn a picture into evidence: without
        // the POV nobody can retake this framing, and without the quality preset two captures from two machines are
        // not comparable at all.
        if (Get_HasPov(InSnapshot))
        {
            Html += ck::Format_UE(TEXT("<p class=\"dim\">Camera: {} &middot; {} &middot; FOV {:.1f}&deg;</p>\n"),
                Escape_Html(InSnapshot.CameraLocation.ToCompactString()),
                Escape_Html(InSnapshot.CameraRotation.ToCompactString()),
                InSnapshot.CameraFov);
        }

        if (NOT InSnapshot.ScalabilityPreset.IsEmpty() || NOT InSnapshot.BuildVersion.IsEmpty())
        {
            Html += ck::Format_UE(TEXT("<p class=\"dim\">Quality: {} &middot; screen {:.0f}% &middot; build {}</p>\n"),
                Escape_Html(InSnapshot.ScalabilityPreset),
                InSnapshot.ScreenPercentage,
                Escape_Html(InSnapshot.BuildVersion));
        }

        // ---- Images ----
        Html += TEXT("<h2>Capture</h2>\n");
        Html += ck::Format_UE(TEXT("<img src=\"data:image/png;base64,{}\" alt=\"capture\">\n"),
            FBase64::Encode(InSnapshot.ColorPng.GetData(), InSnapshot.ColorPng.Num()));

        if (const auto IdMapBase64 = Build_IdMapPngBase64(InSnapshot, DecodedIds); NOT IdMapBase64.IsEmpty())
        {
            Html += TEXT("<h2>Mesh identification</h2>\n");
            Html += TEXT("<p class=\"dim\">Each colour is one mesh; black is sky or an excluded primitive. The ")
                TEXT("silhouettes should match the capture exactly.</p>\n");
            Html += ck::Format_UE(TEXT("<img src=\"data:image/png;base64,{}\" alt=\"id map\">\n"), IdMapBase64);
        }

        // ---- Auxiliary views, in capture order ----
        for (const auto& Aux : InSnapshot.AuxImages)
        {
            if (Aux.Png.IsEmpty())
            { continue; }

            Html += ck::Format_UE(TEXT("<h2>{}</h2>\n"), Escape_Html(Aux.Name));
            Html += ck::Format_UE(TEXT("<img src=\"data:image/png;base64,{}\" alt=\"{}\">\n"),
                FBase64::Encode(Aux.Png.GetData(), Aux.Png.Num()), Escape_Html(Aux.Name));
        }

        // ---- Whole-view totals ----
        Html += TEXT("<h2>This view</h2>\n<table>\n");

        const auto AddFactRow = [&Html](const TCHAR* InKey, const FString& InValue) -> void
        {
            Html += ck::Format_UE(TEXT("<tr><th>{}</th><td class=\"num\">{}</td></tr>\n"), InKey, InValue);
        };

        AddFactRow(TEXT("Meshes"), FString::FromInt(InSnapshot.Prims.Num()));
        AddFactRow(TEXT("Triangles (LOD0)"), FString::FromInt(static_cast<int32>(Aggregates.TotalLod0Triangles)));
        AddFactRow(TEXT("&asymp; Draw calls (LOD0 sections)"), FString::FromInt(Aggregates.TotalLod0Sections));
        AddFactRow(TEXT("Instances"), FString::FromInt(Aggregates.TotalInstances));
        AddFactRow(TEXT("Static / Instanced / Skeletal"), ck::Format_UE(TEXT("{} / {} / {}"),
            Aggregates.StaticCount, Aggregates.InstancedCount, Aggregates.SkeletalCount));
        AddFactRow(TEXT("Nanite meshes"), FString::FromInt(Aggregates.NaniteCount));
        AddFactRow(TEXT("Unique materials"), FString::FromInt(InSnapshot.UniqueMaterialCount));
        AddFactRow(TEXT("Unique textures"), FString::FromInt(InSnapshot.UniqueTextureCount));
        AddFactRow(TEXT("Texture memory (resident)"), Format_ByteSize(InSnapshot.TextureResidentBytes));

        Html += TEXT("</table>\n");

        // ---- Per-mesh table, worst first — the list's own convention ----
        auto SortedIndices = TArray<int32>{};
        SortedIndices.Reserve(InSnapshot.Prims.Num());

        for (auto Index = 0; Index < InSnapshot.Prims.Num(); ++Index)
        { SortedIndices.Add(Index); }

        SortedIndices.Sort([&InSnapshot](int32 InLhs, int32 InRhs)
        {
            const auto LhsTris = InSnapshot.Prims[InLhs].Lods.IsEmpty() ? 0 : InSnapshot.Prims[InLhs].Lods[0].Triangles;
            const auto RhsTris = InSnapshot.Prims[InRhs].Lods.IsEmpty() ? 0 : InSnapshot.Prims[InRhs].Lods[0].Triangles;

            if (LhsTris != RhsTris)
            { return LhsTris > RhsTris; }

            // Index as the tie-break: TArray::Sort is unstable, and a report that reordered equal rows between two
            // generations of the same snapshot would fail its own determinism spec.
            return InLhs < InRhs;
        });

        Html += TEXT("<h2>Meshes</h2>\n<table>\n<tr><th>Mesh</th><th>Kind</th><th>LOD0 tris</th><th>Sections</th>")
            TEXT("<th>Verts</th><th>LODs</th><th>Instances</th><th>Distance</th><th>Mesh memory</th>")
            TEXT("<th>Texture memory</th><th>Coverage</th><th>Tris/px</th><th>Slots</th></tr>\n");

        for (const auto& Index : SortedIndices)
        {
            const auto& Prim = InSnapshot.Prims[Index];
            const auto& Lod0 = Prim.Lods.IsEmpty() ? FCkOptimizationDebugger_SnapshotLod{} : Prim.Lods[0];

            const auto KindLabel = [&Prim]() -> const TCHAR*
            {
                switch (Prim.Kind)
                {
                    case ECkOptimizationDebugger_SnapshotPrimKind::InstancedStaticMesh: return TEXT("Instanced");
                    case ECkOptimizationDebugger_SnapshotPrimKind::SkeletalMesh:        return TEXT("Skeletal");
                    default:                                                            return TEXT("Static");
                }
            }();

            // A snapshot with no identification has no coverage to report, and a zero there would read as "this
            // mesh is not visible" rather than "this report cannot tell".
            const auto CoveredPixels = Coverage.IsValidIndex(Index) ? Coverage[Index] : INDEX_NONE;

            const auto CoverageText = CoveredPixels >= 0
                ? ck::Format_UE(TEXT("{}"), CoveredPixels)
                : FString{TEXT("&mdash;")};

            const auto DensityText = CoveredPixels > 0
                ? ck::Format_UE(TEXT("{:.2f}"), Get_TrianglesPerCoveredPixel(Prim, CoveredPixels))
                : FString{TEXT("&mdash;")};

            Html += ck::Format_UE(TEXT("<tr><td>{}{}</td><td>{}</td><td class=\"num\">{}</td><td class=\"num\">{}</td>")
                TEXT("<td class=\"num\">{}</td><td class=\"num\">{}</td><td class=\"num\">{}</td>")
                TEXT("<td class=\"num\">{} cm</td><td class=\"num\">{}</td><td class=\"num\">{}</td>")
                TEXT("<td class=\"num\">{}</td><td class=\"num\">{}</td><td class=\"num\">{}</td></tr>\n"),
                Escape_Html(Prim.DisplayName), Prim.IsNanite ? TEXT(" <b>N</b>") : TEXT(""),
                KindLabel, Lod0.Triangles, Lod0.Sections, Lod0.Vertices, Prim.Lods.Num(), Prim.InstanceCount,
                FMath::RoundToInt(Prim.DistanceFromCamera), Format_ByteSize(Prim.MeshResourceSizeBytes),
                Format_ByteSize(Prim.TextureResidentBytes), CoverageText, DensityText,
                Prim.MaterialSlots.Num());
        }

        Html += TEXT("</table>\n");

        // ---- Material slots, same order as the table above ----
        Html += TEXT("<h2>Materials</h2>\n<table>\n<tr><th>Mesh</th><th>Slot</th><th>Material</th><th>Blend</th>")
            TEXT("<th>Shading</th><th>Two-sided</th><th>Samplers</th><th>Textures</th></tr>\n");

        for (const auto& Index : SortedIndices)
        {
            const auto& Prim = InSnapshot.Prims[Index];

            for (const auto& Slot : Prim.MaterialSlots)
            {
                Html += ck::Format_UE(TEXT("<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td>")
                    TEXT("<td>{}</td><td class=\"num\">{}</td><td class=\"dim\">{}</td></tr>\n"),
                    Escape_Html(Prim.DisplayName), Escape_Html(Slot.SlotName), Escape_Html(Slot.MaterialName),
                    Escape_Html(Slot.BlendMode), Escape_Html(Slot.ShadingModel),
                    Slot.IsTwoSided ? TEXT("yes") : TEXT(""), Slot.UsedTextureCount,
                    Escape_Html(FString::Join(Slot.UsedTextureNames, TEXT(", "))));
            }
        }

        Html += TEXT("</table>\n</body>\n</html>\n");

        return Html;
    }
}

// --------------------------------------------------------------------------------------------------------------------
