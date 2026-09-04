#include "CkNavmeshDebugDraw/Rendering/CkNavmeshDebugDraw_MeshComponent.h"

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Get_TriangleCount() const -> int32
{
    auto TriangleCount = 0;
    for (const auto& AreaMesh : _AreaMeshes)
    {
        TriangleCount += AreaMesh._TriangleVertices.Num() / 3;
    }
    return TriangleCount;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Get_AreaPreviewTriangleCount() const -> int32
{
    auto TriangleCount = 0;
    for (const auto& AreaMesh : _AreaPreviewMeshes)
    {
        TriangleCount += AreaMesh._TriangleVertices.Num() / 3;
    }
    return TriangleCount;
}

#if WITH_CK_NAVMESH_DEBUG_DRAW

#include "CkCore/Ensure/CkEnsure.h"

#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "GameTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "Misc/Crc.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "StaticMeshResources.h"
#include "Engine/World.h"

namespace ck_navmesh_debug_draw_mesh_component
{
    constexpr auto EdgeColor = FLinearColor{1.0f, 1.0f, 1.0f, 0.9f};
    constexpr auto StaleEdgeColor = FLinearColor{1.0f, 0.45f, 0.05f, 0.9f};
    constexpr auto StaleFillTint = FLinearColor{1.0f, 0.45f, 0.05f, 1.0f};
    constexpr auto StaleFillTintWeight = 0.16f;
    constexpr auto StaleFillAlphaScale = 0.6f;
    constexpr auto GhostFadeDurationSeconds = 2.0;
    constexpr auto GhostFadeCohortSeconds = 0.25;
    constexpr auto GhostFillInitialAlpha = 0.88f;
    constexpr auto GhostEdgeInitialAlpha = 0.62f;
    const auto GhostDrawLift = FVector3f{0.0f, 0.0f, 1.0f};
    const auto AreaPreviewDrawLift = FVector3f{0.0f, 0.0f, 1.5f};
    constexpr auto TriangleQuantizationScale = 10.0;

    struct FQuantizedVertex
    {
        int64 _X = 0;
        int64 _Y = 0;
        int64 _Z = 0;

        auto operator==(const FQuantizedVertex& InOther) const -> bool = default;
    };

    struct FTriangleKey
    {
        FQuantizedVertex _A;
        FQuantizedVertex _B;
        FQuantizedVertex _C;

        auto operator==(const FTriangleKey& InOther) const -> bool = default;
    };

    struct FTriangleRecord
    {
        FTriangleKey _Key;
        FVector3f _A = FVector3f::ZeroVector;
        FVector3f _B = FVector3f::ZeroVector;
        FVector3f _C = FVector3f::ZeroVector;
        double _FadeStartRealTimeSeconds = 0.0;
    };

    auto QuantizeVertex(const FVector3f& InVertex) -> FQuantizedVertex
    {
        return FQuantizedVertex{
            FMath::RoundToInt64(static_cast<double>(InVertex.X) * TriangleQuantizationScale),
            FMath::RoundToInt64(static_cast<double>(InVertex.Y) * TriangleQuantizationScale),
            FMath::RoundToInt64(static_cast<double>(InVertex.Z) * TriangleQuantizationScale)};
    }

    auto IsVertexLess(const FQuantizedVertex& InLeft, const FQuantizedVertex& InRight) -> bool
    {
        if (InLeft._X != InRight._X)
        {
            return InLeft._X < InRight._X;
        }
        if (InLeft._Y != InRight._Y)
        {
            return InLeft._Y < InRight._Y;
        }
        return InLeft._Z < InRight._Z;
    }

    auto IsTriangleKeyLess(const FTriangleKey& InLeft, const FTriangleKey& InRight) -> bool
    {
        if (InLeft._A != InRight._A)
        {
            return IsVertexLess(InLeft._A, InRight._A);
        }
        if (InLeft._B != InRight._B)
        {
            return IsVertexLess(InLeft._B, InRight._B);
        }
        return IsVertexLess(InLeft._C, InRight._C);
    }

    auto BuildTriangleRecord(
        const FVector3f& InA,
        const FVector3f& InB,
        const FVector3f& InC) -> FTriangleRecord
    {
        auto QuantizedA = QuantizeVertex(InA);
        auto QuantizedB = QuantizeVertex(InB);
        auto QuantizedC = QuantizeVertex(InC);
        if (IsVertexLess(QuantizedB, QuantizedA))
        {
            Swap(QuantizedA, QuantizedB);
        }
        if (IsVertexLess(QuantizedC, QuantizedB))
        {
            Swap(QuantizedB, QuantizedC);
        }
        if (IsVertexLess(QuantizedB, QuantizedA))
        {
            Swap(QuantizedA, QuantizedB);
        }

        return FTriangleRecord{
            FTriangleKey{QuantizedA, QuantizedB, QuantizedC},
            InA,
            InB,
            InC,
            0.0};
    }

    auto AppendTriangleRecords(
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaMeshes,
        TArray<FTriangleRecord>& OutRecords) -> void
    {
        for (const auto& AreaMesh : InAreaMeshes)
        {
            for (auto TriangleStart = 0;
                 TriangleStart < AreaMesh._TriangleVertices.Num();
                 TriangleStart += 3)
            {
                OutRecords.Add(BuildTriangleRecord(
                    AreaMesh._TriangleVertices[TriangleStart],
                    AreaMesh._TriangleVertices[TriangleStart + 1],
                    AreaMesh._TriangleVertices[TriangleStart + 2]));
            }
        }
    }

    auto AppendTriangleRecords(
        const TArray<FCk_NavmeshDebugDraw_GhostTriangle>& InGhostTriangles,
        TArray<FTriangleRecord>& OutRecords) -> void
    {
        for (const auto& GhostTriangle : InGhostTriangles)
        {
            auto Record = BuildTriangleRecord(GhostTriangle._A, GhostTriangle._B, GhostTriangle._C);
            Record._FadeStartRealTimeSeconds = GhostTriangle._FadeStartRealTimeSeconds;
            OutRecords.Add(MoveTemp(Record));
        }
    }

    auto SortTriangleRecords(TArray<FTriangleRecord>& InOutRecords) -> void
    {
        InOutRecords.Sort([](const FTriangleRecord& InLeft, const FTriangleRecord& InRight)
        {
            return IsTriangleKeyLess(InLeft._Key, InRight._Key);
        });
    }

    auto BuildRemovedTriangleRecords(
        const TArray<FTriangleRecord>& InOldRecords,
        const TArray<FTriangleRecord>& InNewRecords) -> TArray<FTriangleRecord>
    {
        auto RemovedRecords = TArray<FTriangleRecord>{};
        auto OldIndex = 0;
        auto NewIndex = 0;
        while (OldIndex < InOldRecords.Num())
        {
            if (NewIndex >= InNewRecords.Num())
            {
                for (; OldIndex < InOldRecords.Num(); ++OldIndex)
                {
                    RemovedRecords.Add(InOldRecords[OldIndex]);
                }
                break;
            }

            const auto& OldRecord = InOldRecords[OldIndex];
            const auto& NewRecord = InNewRecords[NewIndex];
            if (OldRecord._Key == NewRecord._Key)
            {
                ++OldIndex;
                ++NewIndex;
            }
            else if (IsTriangleKeyLess(OldRecord._Key, NewRecord._Key))
            {
                RemovedRecords.Add(OldRecord);
                ++OldIndex;
            }
            else
            {
                ++NewIndex;
            }
        }
        return RemovedRecords;
    }

    auto BuildGhostTriangles(
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InOldAreaMeshes,
        const TArray<FCk_NavmeshDebugDraw_GhostTriangle>& InExistingGhostTriangles,
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InNewAreaMeshes,
        const int32 InMaxGhostTriangleCount,
        const double InFadeStartRealTimeSeconds) -> TArray<FCk_NavmeshDebugDraw_GhostTriangle>
    {
        auto OldRecords = TArray<FTriangleRecord>{};
        auto NewRecords = TArray<FTriangleRecord>{};
        auto GhostCandidates = TArray<FTriangleRecord>{};
        AppendTriangleRecords(InOldAreaMeshes, OldRecords);
        AppendTriangleRecords(InNewAreaMeshes, NewRecords);
        AppendTriangleRecords(InExistingGhostTriangles, GhostCandidates);
        SortTriangleRecords(OldRecords);
        SortTriangleRecords(NewRecords);
        auto RemovedRecords = BuildRemovedTriangleRecords(OldRecords, NewRecords);
        for (auto& RemovedRecord : RemovedRecords)
        {
            RemovedRecord._FadeStartRealTimeSeconds = InFadeStartRealTimeSeconds;
        }
        GhostCandidates.Append(MoveTemp(RemovedRecords));
        SortTriangleRecords(GhostCandidates);

        auto UniqueCandidates = TArray<FTriangleRecord>{};
        UniqueCandidates.Reserve(GhostCandidates.Num());
        for (const auto& Candidate : GhostCandidates)
        {
            if (UniqueCandidates.IsEmpty() || UniqueCandidates.Last()._Key != Candidate._Key)
            {
                UniqueCandidates.Add(Candidate);
            }
            else if (Candidate._FadeStartRealTimeSeconds <
                     UniqueCandidates.Last()._FadeStartRealTimeSeconds)
            {
                UniqueCandidates.Last() = Candidate;
            }
        }

        auto GhostTriangles = TArray<FCk_NavmeshDebugDraw_GhostTriangle>{};
        GhostTriangles.Reserve(FMath::Min(InMaxGhostTriangleCount, UniqueCandidates.Num()));
        auto CandidateIndex = 0;
        auto NewIndex = 0;
        while (CandidateIndex < UniqueCandidates.Num() &&
               GhostTriangles.Num() < InMaxGhostTriangleCount)
        {
            const auto& Candidate = UniqueCandidates[CandidateIndex];
            while (NewIndex < NewRecords.Num() &&
                   IsTriangleKeyLess(NewRecords[NewIndex]._Key, Candidate._Key))
            {
                ++NewIndex;
            }

            if (NewIndex >= NewRecords.Num() || NewRecords[NewIndex]._Key != Candidate._Key)
            {
                GhostTriangles.Add(FCk_NavmeshDebugDraw_GhostTriangle{
                    Candidate._A,
                    Candidate._B,
                    Candidate._C,
                    Candidate._FadeStartRealTimeSeconds});
            }
            ++CandidateIndex;
        }
        return GhostTriangles;
    }

    auto QuantizeGhostFadeStartRealTime(const double InRealTimeSeconds) -> double
    {
        return FMath::FloorToDouble(InRealTimeSeconds / GhostFadeCohortSeconds) *
            GhostFadeCohortSeconds;
    }

    auto ResolveGhostFadeScale(
        const double InFadeStartRealTimeSeconds,
        const double InCurrentRealTimeSeconds) -> float
    {
        const auto FadeProgress = FMath::Clamp(
            (InCurrentRealTimeSeconds - InFadeStartRealTimeSeconds) / GhostFadeDurationSeconds,
            0.0,
            1.0);
        const auto SmoothedProgress = FadeProgress * FadeProgress * (3.0 - 2.0 * FadeProgress);
        return static_cast<float>(1.0 - SmoothedProgress);
    }

    auto ResolveGhostFillColor(
        const double InFadeStartRealTimeSeconds,
        const double InCurrentRealTimeSeconds) -> FLinearColor
    {
        constexpr auto BaseColor = FLinearColor{0.07f, 0.04f, 0.02f, GhostFillInitialAlpha};
        auto Color = BaseColor;
        Color.A *= ResolveGhostFadeScale(InFadeStartRealTimeSeconds, InCurrentRealTimeSeconds);
        return Color;
    }

    auto ResolveGhostEdgeColor(
        const double InFadeStartRealTimeSeconds,
        const double InCurrentRealTimeSeconds) -> FLinearColor
    {
        constexpr auto BaseColor = FLinearColor{0.14f, 0.07f, 0.025f, GhostEdgeInitialAlpha};
        auto Color = BaseColor;
        Color.A *= ResolveGhostFadeScale(InFadeStartRealTimeSeconds, InCurrentRealTimeSeconds);
        return Color;
    }

    auto ResolveFillColor(const FColor InAreaColor, const bool InIsStale) -> FLinearColor
    {
        auto Color = FLinearColor::FromSRGBColor(InAreaColor);
        if (InIsStale)
        {
            Color.R = FMath::Lerp(Color.R, StaleFillTint.R, StaleFillTintWeight);
            Color.G = FMath::Lerp(Color.G, StaleFillTint.G, StaleFillTintWeight);
            Color.B = FMath::Lerp(Color.B, StaleFillTint.B, StaleFillTintWeight);
            Color.A *= StaleFillAlphaScale;
        }
        return Color;
    }

    auto IsFinite(const FVector3f& InVertex) -> bool
    {
        return FMath::IsFinite(InVertex.X)
            && FMath::IsFinite(InVertex.Y)
            && FMath::IsFinite(InVertex.Z);
    }

    auto HasNonDegenerateTriangles(const TArray<FVector3f>& InTriangleVertices) -> bool
    {
        for (int32 TriangleStart = 0; TriangleStart < InTriangleVertices.Num(); TriangleStart += 3)
        {
            const auto Edge01 = InTriangleVertices[TriangleStart + 1] - InTriangleVertices[TriangleStart];
            const auto Edge02 = InTriangleVertices[TriangleStart + 2] - InTriangleVertices[TriangleStart];
            if (FVector3f::CrossProduct(Edge01, Edge02).SizeSquared() <= SMALL_NUMBER)
            { return false; }
        }

        return true;
    }

    auto HasValidEdges(const TArray<FVector3f>& InEdgeVertices) -> bool
    {
        if (InEdgeVertices.Num() % 2 != 0)
        {
            return false;
        }

        for (auto EdgeIndex = 0; EdgeIndex < InEdgeVertices.Num(); EdgeIndex += 2)
        {
            if (NOT IsFinite(InEdgeVertices[EdgeIndex]) || NOT IsFinite(InEdgeVertices[EdgeIndex + 1]) ||
                FVector3f::DistSquared(InEdgeVertices[EdgeIndex], InEdgeVertices[EdgeIndex + 1]) <= SMALL_NUMBER)
            {
                return false;
            }
        }
        return true;
    }

    auto HasValidAreaMeshes(const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaMeshes) -> bool
    {
        return NOT InAreaMeshes.IsEmpty() &&
            NOT InAreaMeshes.ContainsByPredicate([](const FCk_NavmeshDebugDraw_AreaMesh& InAreaMesh)
        {
            const auto& TriangleVertices = InAreaMesh._TriangleVertices;
            return TriangleVertices.IsEmpty() ||
                TriangleVertices.Num() % 3 != 0 ||
                TriangleVertices.ContainsByPredicate([](const FVector3f& InVertex)
                {
                    return NOT IsFinite(InVertex);
                }) ||
                NOT HasNonDegenerateTriangles(TriangleVertices);
        });
    }

    auto BuildBounds(
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaMeshes,
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaPreviewMeshes,
        const TArray<FVector3f>& InEdgeVertices,
        const TArray<FCk_NavmeshDebugDraw_GhostTriangle>& InGhostTriangles) -> FBox
    {
        auto Bounds = FBox{ForceInit};
        for (const auto& AreaMesh : InAreaMeshes)
        {
            for (const auto& Vertex : AreaMesh._TriangleVertices)
            {
                Bounds += FVector{Vertex};
            }
        }
        for (const auto& AreaPreviewMesh : InAreaPreviewMeshes)
        {
            for (const auto& Vertex : AreaPreviewMesh._TriangleVertices)
            {
                Bounds += FVector{Vertex + AreaPreviewDrawLift};
            }
        }
        for (const auto& Vertex : InEdgeVertices)
        {
            Bounds += FVector{Vertex};
        }
        for (const auto& GhostTriangle : InGhostTriangles)
        {
            Bounds += FVector{GhostTriangle._A + GhostDrawLift};
            Bounds += FVector{GhostTriangle._B + GhostDrawLift};
            Bounds += FVector{GhostTriangle._C + GhostDrawLift};
        }

        return Bounds;
    }

    auto BuildContentSignature(
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaMeshes,
        const TArray<FCk_NavmeshDebugDraw_AreaMesh>& InAreaPreviewMeshes,
        const TArray<FVector3f>& InEdgeVertices,
        const TArray<FCk_NavmeshDebugDraw_GhostTriangle>& InGhostTriangles) -> uint32
    {
        auto Signature = uint32{0};
        for (const auto& AreaMesh : InAreaMeshes)
        {
            Signature = FCrc::MemCrc32(&AreaMesh._Color, sizeof(AreaMesh._Color), Signature);
            Signature = FCrc::MemCrc32(&AreaMesh._AreaId, sizeof(AreaMesh._AreaId), Signature);
            Signature = FCrc::MemCrc32(
                AreaMesh._TriangleVertices.GetData(),
                AreaMesh._TriangleVertices.Num() * sizeof(FVector3f),
                Signature);
        }
        for (const auto& AreaPreviewMesh : InAreaPreviewMeshes)
        {
            Signature = FCrc::MemCrc32(&AreaPreviewMesh._Color, sizeof(AreaPreviewMesh._Color), Signature);
            Signature = FCrc::MemCrc32(&AreaPreviewMesh._AreaId, sizeof(AreaPreviewMesh._AreaId), Signature);
            Signature = FCrc::MemCrc32(
                AreaPreviewMesh._TriangleVertices.GetData(),
                AreaPreviewMesh._TriangleVertices.Num() * sizeof(FVector3f),
                Signature);
        }
        Signature = FCrc::MemCrc32(
            InEdgeVertices.GetData(),
            InEdgeVertices.Num() * sizeof(FVector3f),
            Signature);
        for (const auto& GhostTriangle : InGhostTriangles)
        {
            Signature = FCrc::MemCrc32(&GhostTriangle._A, sizeof(GhostTriangle._A), Signature);
            Signature = FCrc::MemCrc32(&GhostTriangle._B, sizeof(GhostTriangle._B), Signature);
            Signature = FCrc::MemCrc32(&GhostTriangle._C, sizeof(GhostTriangle._C), Signature);
            Signature = FCrc::MemCrc32(
                &GhostTriangle._FadeStartRealTimeSeconds,
                sizeof(GhostTriangle._FadeStartRealTimeSeconds),
                Signature);
        }
        return Signature;
    }

    struct FGhostFadeCohort
    {
        double _FadeStartRealTimeSeconds = 0.0;
        TArray<FVector3f> _TriangleVertices;
    };

    auto BuildGhostFadeCohorts(
        const TArray<FCk_NavmeshDebugDraw_GhostTriangle>& InGhostTriangles)
        -> TArray<FGhostFadeCohort>
    {
        auto Cohorts = TArray<FGhostFadeCohort>{};
        for (const auto& GhostTriangle : InGhostTriangles)
        {
            auto* Cohort = Cohorts.FindByPredicate([&GhostTriangle](const FGhostFadeCohort& InCohort)
            {
                return InCohort._FadeStartRealTimeSeconds ==
                    GhostTriangle._FadeStartRealTimeSeconds;
            });
            if (Cohort == nullptr)
            {
                Cohort = &Cohorts.AddDefaulted_GetRef();
                Cohort->_FadeStartRealTimeSeconds = GhostTriangle._FadeStartRealTimeSeconds;
            }
            Cohort->_TriangleVertices.Append({
                GhostTriangle._A + GhostDrawLift,
                GhostTriangle._B + GhostDrawLift,
                GhostTriangle._C + GhostDrawLift});
        }
        return Cohorts;
    }
} // namespace ck_navmesh_debug_draw_mesh_component

// --------------------------------------------------------------------------------------------------------------------

class FCk_NavmeshDebugDraw_MeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
    explicit FCk_NavmeshDebugDraw_MeshSceneProxy(const UCk_NavmeshDebugDraw_MeshComponent_UE* InComponent)
        : FPrimitiveSceneProxy{InComponent}
        , _VertexFactory{GetScene().GetFeatureLevel(), "FCk_NavmeshDebugDraw_MeshSceneProxy"}
    {
        auto VertexCount = InComponent->_EdgeVertices.Num() + InComponent->_GhostTriangles.Num() * 9;
        for (const auto& AreaMesh : InComponent->_AreaMeshes)
        {
            VertexCount += AreaMesh._TriangleVertices.Num();
        }
        for (const auto& AreaPreviewMesh : InComponent->_AreaPreviewMeshes)
        {
            VertexCount += AreaPreviewMesh._TriangleVertices.Num();
        }

        auto Vertices = TArray<FDynamicMeshVertex>{};
        Vertices.Reserve(VertexCount);
        const auto GhostFadeCohorts =
            ck_navmesh_debug_draw_mesh_component::BuildGhostFadeCohorts(InComponent->_GhostTriangles);
        const auto GhostFadeBatchCount = GhostFadeCohorts.Num();
        _FillElements.Reserve(
            InComponent->_AreaMeshes.Num() + InComponent->_AreaPreviewMeshes.Num() + GhostFadeBatchCount);
        _FillMaterialProxies.Reserve(
            InComponent->_AreaMeshes.Num() + InComponent->_AreaPreviewMeshes.Num() + GhostFadeBatchCount);
        _FillGhostFadeStartRealTimeSeconds.Reserve(
            InComponent->_AreaMeshes.Num() + InComponent->_AreaPreviewMeshes.Num() + GhostFadeBatchCount);

        const auto* FillMaterial = GEngine != nullptr && GEngine->DebugMeshMaterial != nullptr
            ? static_cast<UMaterialInterface*>(GEngine->DebugMeshMaterial)
            : UMaterial::GetDefaultMaterial(MD_Surface);
        _MaterialRelevance = FillMaterial->GetRelevance_Concurrent(GetScene().GetShaderPlatform());

        const auto AppendFillGeometry = [this, &Vertices, FillMaterial](
            const TArray<FVector3f>& InTriangleVertices,
            const FLinearColor& InFillColor,
            const double InGhostFadeStartRealTimeSeconds)
        {
            const auto VertexStart = Vertices.Num();
            auto Element = FMeshBatchElement{};
            Element.FirstIndex = _TriangleIndexBuffer.Indices.Num();
            Element.NumPrimitives = InTriangleVertices.Num() / 3;
            Element.MinVertexIndex = VertexStart;
            Element.MaxVertexIndex = VertexStart + InTriangleVertices.Num() - 1;
            Element.IndexBuffer = &_TriangleIndexBuffer;
            _FillElements.Add(Element);
            _FillMaterialProxies.Add(MakeUnique<FColoredMaterialRenderProxy>(
                FillMaterial->GetRenderProxy(),
                InFillColor));
            _FillGhostFadeStartRealTimeSeconds.Add(InGhostFadeStartRealTimeSeconds);

            for (int32 TriangleStart = 0; TriangleStart < InTriangleVertices.Num(); TriangleStart += 3)
            {
                const auto Normal = FVector3f::CrossProduct(
                    InTriangleVertices[TriangleStart + 1] - InTriangleVertices[TriangleStart],
                    InTriangleVertices[TriangleStart + 2] - InTriangleVertices[TriangleStart])
                    .GetSafeNormal();
                const auto TangentX =
                    (InTriangleVertices[TriangleStart + 1] - InTriangleVertices[TriangleStart])
                    .GetSafeNormal();
                const auto TangentY = FVector3f::CrossProduct(Normal, TangentX).GetSafeNormal();

                for (int32 VertexOffset = 0; VertexOffset < 3; ++VertexOffset)
                {
                    const auto VertexIndex = Vertices.Num();
                    auto& Vertex = Vertices.AddDefaulted_GetRef();
                    Vertex.Position = InTriangleVertices[TriangleStart + VertexOffset];
                    Vertex.Color = InFillColor.ToFColor(true);
                    Vertex.SetTangents(TangentX, TangentY, Normal);
                    _TriangleIndexBuffer.Indices.Add(VertexIndex);
                }
            }
        };

        for (const auto& AreaMesh : InComponent->_AreaMeshes)
        {
            AppendFillGeometry(
                AreaMesh._TriangleVertices,
                ck_navmesh_debug_draw_mesh_component::ResolveFillColor(
                    AreaMesh._Color,
                    InComponent->_IsStale),
                -1.0);
        }
        for (const auto& AreaPreviewMesh : InComponent->_AreaPreviewMeshes)
        {
            auto PreviewTriangleVertices = AreaPreviewMesh._TriangleVertices;
            for (auto& Vertex : PreviewTriangleVertices)
            {
                Vertex += ck_navmesh_debug_draw_mesh_component::AreaPreviewDrawLift;
            }
            AppendFillGeometry(
                PreviewTriangleVertices,
                FLinearColor::FromSRGBColor(AreaPreviewMesh._Color),
                -1.0);
        }
        for (const auto& GhostFadeCohort : GhostFadeCohorts)
        {
            AppendFillGeometry(
                GhostFadeCohort._TriangleVertices,
                ck_navmesh_debug_draw_mesh_component::ResolveGhostFillColor(
                    GhostFadeCohort._FadeStartRealTimeSeconds,
                    GhostFadeCohort._FadeStartRealTimeSeconds),
                GhostFadeCohort._FadeStartRealTimeSeconds);
        }

        const auto EdgeColor = InComponent->_IsStale
            ? ck_navmesh_debug_draw_mesh_component::StaleEdgeColor
            : ck_navmesh_debug_draw_mesh_component::EdgeColor;
        auto EdgeColors = TArray<FLinearColor>{};
        _EdgeElements.Reserve(1 + GhostFadeBatchCount);
        EdgeColors.Reserve(1 + GhostFadeBatchCount);
        _EdgeGhostFadeStartRealTimeSeconds.Reserve(1 + GhostFadeBatchCount);
        _EdgeIndexBuffer.Indices.Reserve(
            InComponent->_EdgeVertices.Num() + InComponent->_GhostTriangles.Num() * 6);

        const auto AppendEdgeGeometry = [this, &Vertices, &EdgeColors](
            const TArray<FVector3f>& InEdgeVertices,
            const FLinearColor& InEdgeColor,
            const double InGhostFadeStartRealTimeSeconds)
        {
            if (InEdgeVertices.IsEmpty())
            {
                return;
            }

            const auto EdgeVertexStart = Vertices.Num();
            auto Element = FMeshBatchElement{};
            Element.FirstIndex = _EdgeIndexBuffer.Indices.Num();
            Element.NumPrimitives = InEdgeVertices.Num() / 2;
            Element.MinVertexIndex = EdgeVertexStart;
            Element.MaxVertexIndex = EdgeVertexStart + InEdgeVertices.Num() - 1;
            Element.IndexBuffer = &_EdgeIndexBuffer;
            _EdgeElements.Add(Element);
            EdgeColors.Add(InEdgeColor);
            _EdgeGhostFadeStartRealTimeSeconds.Add(InGhostFadeStartRealTimeSeconds);
            for (const auto& EdgeVertex : InEdgeVertices)
            {
                const auto VertexIndex = Vertices.Num();
                auto& Vertex = Vertices.AddDefaulted_GetRef();
                Vertex.Position = EdgeVertex;
                Vertex.Color = InEdgeColor.ToFColor(true);
                Vertex.SetTangents(FVector3f::XAxisVector, FVector3f::YAxisVector, FVector3f::ZAxisVector);
                _EdgeIndexBuffer.Indices.Add(VertexIndex);
            }
        };

        AppendEdgeGeometry(InComponent->_EdgeVertices, EdgeColor, -1.0);
        for (const auto& GhostFadeCohort : GhostFadeCohorts)
        {
            auto GhostEdgeVertices = TArray<FVector3f>{};
            GhostEdgeVertices.Reserve(GhostFadeCohort._TriangleVertices.Num() * 2);
            for (auto TriangleStart = 0;
                 TriangleStart < GhostFadeCohort._TriangleVertices.Num();
                 TriangleStart += 3)
            {
                const auto& A = GhostFadeCohort._TriangleVertices[TriangleStart];
                const auto& B = GhostFadeCohort._TriangleVertices[TriangleStart + 1];
                const auto& C = GhostFadeCohort._TriangleVertices[TriangleStart + 2];
                GhostEdgeVertices.Append({A, B, B, C, C, A});
            }
            AppendEdgeGeometry(
                GhostEdgeVertices,
                ck_navmesh_debug_draw_mesh_component::ResolveGhostEdgeColor(
                    GhostFadeCohort._FadeStartRealTimeSeconds,
                    GhostFadeCohort._FadeStartRealTimeSeconds),
                GhostFadeCohort._FadeStartRealTimeSeconds);
        }

        _VertexBuffers.InitFromDynamicVertex(&_VertexFactory, Vertices);
        BeginInitResource(&_VertexBuffers.PositionVertexBuffer);
        BeginInitResource(&_VertexBuffers.StaticMeshVertexBuffer);
        BeginInitResource(&_VertexBuffers.ColorVertexBuffer);
        BeginInitResource(&_TriangleIndexBuffer);
        if (NOT _EdgeIndexBuffer.Indices.IsEmpty())
        {
            BeginInitResource(&_EdgeIndexBuffer);
        }
        BeginInitResource(&_VertexFactory);

        const auto* EdgeMaterial = GEngine != nullptr && GEngine->LevelColorationUnlitMaterial != nullptr
            ? static_cast<UMaterialInterface*>(GEngine->LevelColorationUnlitMaterial)
            : FillMaterial;
        _MaterialRelevance |= EdgeMaterial->GetRelevance_Concurrent(GetScene().GetShaderPlatform());
        _EdgeMaterialProxies.Reserve(EdgeColors.Num());
        for (const auto& BatchColor : EdgeColors)
        {
            _EdgeMaterialProxies.Add(MakeUnique<FColoredMaterialRenderProxy>(
                EdgeMaterial->GetRenderProxy(),
                BatchColor));
        }
    }

    virtual ~FCk_NavmeshDebugDraw_MeshSceneProxy() override
    {
        _VertexBuffers.PositionVertexBuffer.ReleaseResource();
        _VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
        _VertexBuffers.ColorVertexBuffer.ReleaseResource();
        _TriangleIndexBuffer.ReleaseResource();
        _EdgeIndexBuffer.ReleaseResource();
        _VertexFactory.ReleaseResource();
    }

    virtual auto GetTypeHash() const -> SIZE_T override
    {
        static size_t UniquePointer;
        return reinterpret_cast<SIZE_T>(&UniquePointer);
    }

    virtual auto GetDynamicMeshElements(
        const TArray<const FSceneView*>& InViews,
        const FSceneViewFamily& InViewFamily,
        uint32 InVisibilityMap,
        FMeshElementCollector& OutCollector) const -> void override
    {
        static_cast<void>(InViewFamily);
        const auto CurrentRealTimeSeconds =
            FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds();
        for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ++ViewIndex)
        {
            if ((InVisibilityMap & (1 << ViewIndex)) == 0)
            {
                continue;
            }

            auto& DynamicUniformBuffer =
                OutCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
            auto Builder = FPrimitiveUniformShaderParametersBuilder{};
            BuildUniformShaderParameters(Builder);
            DynamicUniformBuffer.Set(OutCollector.GetRHICommandList(), Builder);

            for (auto FillIndex = 0; FillIndex < _FillElements.Num(); ++FillIndex)
            {
                auto* FillMaterialProxy = _FillMaterialProxies[FillIndex].Get();
                const auto GhostFadeStartRealTimeSeconds =
                    _FillGhostFadeStartRealTimeSeconds[FillIndex];
                if (GhostFadeStartRealTimeSeconds >= 0.0)
                {
                    FillMaterialProxy = &OutCollector.AllocateOneFrameResource<
                        FColoredMaterialRenderProxy>(
                            FillMaterialProxy,
                            ck_navmesh_debug_draw_mesh_component::ResolveGhostFillColor(
                                GhostFadeStartRealTimeSeconds,
                                CurrentRealTimeSeconds));
                }

                auto& Mesh = OutCollector.AllocateMesh();
                Mesh.Elements[0] = _FillElements[FillIndex];
                Mesh.Elements[0].PrimitiveUniformBufferResource = &DynamicUniformBuffer.UniformBuffer;
                Mesh.VertexFactory = &_VertexFactory;
                Mesh.MaterialRenderProxy = FillMaterialProxy;
                Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                Mesh.bDisableBackfaceCulling = true;
                Mesh.Type = PT_TriangleList;
                Mesh.DepthPriorityGroup = SDPG_World;
                Mesh.CastShadow = false;
                Mesh.bCanApplyViewModeOverrides = false;
                OutCollector.AddMesh(ViewIndex, Mesh);
            }

            for (auto EdgeIndex = 0; EdgeIndex < _EdgeElements.Num(); ++EdgeIndex)
            {
                auto* EdgeMaterialProxy = _EdgeMaterialProxies[EdgeIndex].Get();
                const auto GhostFadeStartRealTimeSeconds =
                    _EdgeGhostFadeStartRealTimeSeconds[EdgeIndex];
                if (GhostFadeStartRealTimeSeconds >= 0.0)
                {
                    EdgeMaterialProxy = &OutCollector.AllocateOneFrameResource<
                        FColoredMaterialRenderProxy>(
                            EdgeMaterialProxy,
                            ck_navmesh_debug_draw_mesh_component::ResolveGhostEdgeColor(
                                GhostFadeStartRealTimeSeconds,
                                CurrentRealTimeSeconds));
                }

                auto& EdgeMesh = OutCollector.AllocateMesh();
                EdgeMesh.Elements[0] = _EdgeElements[EdgeIndex];
                EdgeMesh.Elements[0].PrimitiveUniformBufferResource = &DynamicUniformBuffer.UniformBuffer;
                EdgeMesh.VertexFactory = &_VertexFactory;
                EdgeMesh.MaterialRenderProxy = EdgeMaterialProxy;
                EdgeMesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
                EdgeMesh.Type = PT_LineList;
                EdgeMesh.DepthPriorityGroup = SDPG_World;
                EdgeMesh.CastShadow = false;
                EdgeMesh.bCanApplyViewModeOverrides = false;
                OutCollector.AddMesh(ViewIndex, EdgeMesh);
            }
        }
    }

    virtual auto GetViewRelevance(const FSceneView* InView) const -> FPrimitiveViewRelevance override
    {
        auto Result = FPrimitiveViewRelevance{};
        Result.bDrawRelevance = IsShown(InView);
        Result.bDynamicRelevance = true;
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        _MaterialRelevance.SetPrimitiveViewRelevance(Result);
        return Result;
    }

    virtual auto CanBeOccluded() const -> bool override
    { return NOT _MaterialRelevance.bDisableDepthTest; }

    virtual auto GetMemoryFootprint() const -> uint32 override
    { return sizeof(*this) + GetAllocatedSize(); }

    auto GetAllocatedSize() const -> uint32
    { return FPrimitiveSceneProxy::GetAllocatedSize(); }

private:
    FStaticMeshVertexBuffers _VertexBuffers;
    FDynamicMeshIndexBuffer32 _TriangleIndexBuffer;
    FDynamicMeshIndexBuffer32 _EdgeIndexBuffer;
    FLocalVertexFactory _VertexFactory;
    FMaterialRelevance _MaterialRelevance;
    TArray<FMeshBatchElement> _FillElements;
    TArray<FMeshBatchElement> _EdgeElements;
    TArray<TUniquePtr<FColoredMaterialRenderProxy>> _FillMaterialProxies;
    TArray<TUniquePtr<FColoredMaterialRenderProxy>> _EdgeMaterialProxies;
    TArray<double> _FillGhostFadeStartRealTimeSeconds;
    TArray<double> _EdgeGhostFadeStartRealTimeSeconds;
};

// --------------------------------------------------------------------------------------------------------------------

UCk_NavmeshDebugDraw_MeshComponent_UE::UCk_NavmeshDebugDraw_MeshComponent_UE()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCanEverAffectNavigation(false);
    SetCastShadow(false);
    SetMobility(EComponentMobility::Movable);
    bUseAsOccluder = false;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::IsSnapshotValid(
    const FCk_NavmeshDebugDraw_MeshSnapshot& InSnapshot) -> bool
{
    return ck_navmesh_debug_draw_mesh_component::HasValidAreaMeshes(InSnapshot._AreaMeshes) &&
        ck_navmesh_debug_draw_mesh_component::HasValidEdges(InSnapshot._EdgeVertices);
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::TryApplySnapshot(
    FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
    const int32 InMaxGhostTriangleCount) -> bool
{
    const auto SnapshotIsValid = IsSnapshotValid(InSnapshot);
    CK_ENSURE_IF_NOT(SnapshotIsValid,
        TEXT("Navmesh mesh snapshot must contain valid area triangles and paired finite edges"))
    { return false; }

    ApplyValidatedSnapshot(MoveTemp(InSnapshot), InMaxGhostTriangleCount);
    return true;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::ApplyValidatedSnapshot(
    FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
    const int32 InMaxGhostTriangleCount) -> void
{
    check(IsSnapshotValid(InSnapshot));

    const auto GhostFadeStartRealTimeSeconds =
        ck_navmesh_debug_draw_mesh_component::QuantizeGhostFadeStartRealTime(
            FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());
    auto NewGhostTriangles = ck_navmesh_debug_draw_mesh_component::BuildGhostTriangles(
        _AreaMeshes,
        _GhostTriangles,
        InSnapshot._AreaMeshes,
        FMath::Max(0, InMaxGhostTriangleCount),
        GhostFadeStartRealTimeSeconds);
    const auto NewContentSignature = ck_navmesh_debug_draw_mesh_component::BuildContentSignature(
        InSnapshot._AreaMeshes,
        {},
        InSnapshot._EdgeVertices,
        NewGhostTriangles);
    const auto ContentIsUnchanged = _ContentSignature == NewContentSignature
        && _AreaMeshes == InSnapshot._AreaMeshes
        && _EdgeVertices == InSnapshot._EdgeVertices
        && _GhostTriangles == NewGhostTriangles
        && _AreaPreviewMeshes.IsEmpty();
    if (ContentIsUnchanged)
    {
        const auto StaleStateChanged = _IsStale;
        _Revision = InSnapshot._Revision;
        _IsStale = false;
        _LastApplyRecreatedProxy = StaleStateChanged;
        if (StaleStateChanged)
        {
            MarkRenderStateDirty();
        }
        return;
    }

    const auto NewLocalBounds = ck_navmesh_debug_draw_mesh_component::BuildBounds(
        InSnapshot._AreaMeshes,
        {},
        InSnapshot._EdgeVertices,
        NewGhostTriangles);
    _AreaMeshes = MoveTemp(InSnapshot._AreaMeshes);
    _AreaPreviewMeshes.Reset();
    _EdgeVertices = MoveTemp(InSnapshot._EdgeVertices);
    _GhostTriangles = MoveTemp(NewGhostTriangles);
    _LocalBounds = NewLocalBounds;
    _Revision = InSnapshot._Revision;
    _ContentSignature = NewContentSignature;
    _IsStale = false;
    _LastApplyRecreatedProxy = true;
    UpdateBounds();
    MarkRenderStateDirty();
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::TryApplyAreaPreview(
    TArray<FCk_NavmeshDebugDraw_AreaMesh> InAreaPreviewMeshes) -> bool
{
    const auto PreviewIsValid =
        ck_navmesh_debug_draw_mesh_component::HasValidAreaMeshes(InAreaPreviewMeshes);
    CK_ENSURE_IF_NOT(PreviewIsValid,
        TEXT("Navmesh area preview must contain valid non-empty triangles"))
    {}
    if (NOT PreviewIsValid || _AreaPreviewMeshes == InAreaPreviewMeshes)
    {
        _LastApplyRecreatedProxy = false;
        return false;
    }

    _AreaPreviewMeshes = MoveTemp(InAreaPreviewMeshes);
    _LocalBounds = ck_navmesh_debug_draw_mesh_component::BuildBounds(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _ContentSignature = ck_navmesh_debug_draw_mesh_component::BuildContentSignature(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _LastApplyRecreatedProxy = true;
    UpdateBounds();
    MarkRenderStateDirty();
    return true;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::ClearAreaPreview() -> bool
{
    if (_AreaPreviewMeshes.IsEmpty())
    {
        _LastApplyRecreatedProxy = false;
        return false;
    }

    _AreaPreviewMeshes.Reset();
    _LocalBounds = ck_navmesh_debug_draw_mesh_component::BuildBounds(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _ContentSignature = ck_navmesh_debug_draw_mesh_component::BuildContentSignature(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _LastApplyRecreatedProxy = true;
    UpdateBounds();
    MarkRenderStateDirty();
    return true;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Remove_ExpiredGhostTriangles(
    const double InCurrentRealTimeSeconds) -> bool
{
    if (_GhostTriangles.IsEmpty())
    {
        _LastApplyRecreatedProxy = false;
        return false;
    }

    const auto RemovedCount = _GhostTriangles.RemoveAll(
        [InCurrentRealTimeSeconds](const FCk_NavmeshDebugDraw_GhostTriangle& InGhostTriangle)
    {
        return InCurrentRealTimeSeconds - InGhostTriangle._FadeStartRealTimeSeconds >=
            ck_navmesh_debug_draw_mesh_component::GhostFadeDurationSeconds;
    });
    if (RemovedCount == 0)
    {
        _LastApplyRecreatedProxy = false;
        return false;
    }

    _LocalBounds = ck_navmesh_debug_draw_mesh_component::BuildBounds(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _ContentSignature = ck_navmesh_debug_draw_mesh_component::BuildContentSignature(
        _AreaMeshes,
        _AreaPreviewMeshes,
        _EdgeVertices,
        _GhostTriangles);
    _LastApplyRecreatedProxy = true;
    UpdateBounds();
    MarkRenderStateDirty();
    return true;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Get_EarliestGhostExpiryRealTimeSeconds() const -> double
{
    auto EarliestExpiryRealTimeSeconds = -1.0;
    for (const auto& GhostTriangle : _GhostTriangles)
    {
        const auto ExpiryRealTimeSeconds = GhostTriangle._FadeStartRealTimeSeconds +
            ck_navmesh_debug_draw_mesh_component::GhostFadeDurationSeconds;
        EarliestExpiryRealTimeSeconds = EarliestExpiryRealTimeSeconds < 0.0
            ? ExpiryRealTimeSeconds
            : FMath::Min(EarliestExpiryRealTimeSeconds, ExpiryRealTimeSeconds);
    }
    return EarliestExpiryRealTimeSeconds;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Set_IsStale(const bool InIsStale) -> bool
{
    if (_IsStale == InIsStale || _AreaMeshes.IsEmpty())
    {
        return false;
    }

    _IsStale = InIsStale;
    MarkRenderStateDirty();
    return true;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Clear() -> void
{
    if (_AreaMeshes.IsEmpty() && _AreaPreviewMeshes.IsEmpty() && _GhostTriangles.IsEmpty())
    {
        _Revision = 0;
        _ContentSignature = 0;
        _GhostTriangles.Reset();
        _IsStale = false;
        _LastApplyRecreatedProxy = false;
        return;
    }

    _AreaMeshes.Reset();
    _AreaPreviewMeshes.Reset();
    _EdgeVertices.Reset();
    _GhostTriangles.Reset();
    _LocalBounds = FBox{ForceInit};
    _Revision = 0;
    _ContentSignature = 0;
    _IsStale = false;
    _LastApplyRecreatedProxy = true;
    UpdateBounds();
    MarkRenderStateDirty();
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::CreateSceneProxy() -> FPrimitiveSceneProxy*
{
    return _AreaMeshes.IsEmpty() && _AreaPreviewMeshes.IsEmpty() && _GhostTriangles.IsEmpty()
        ? nullptr
        : new FCk_NavmeshDebugDraw_MeshSceneProxy{this};
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::CalcBounds(const FTransform& InLocalToWorld) const -> FBoxSphereBounds
{
    return _LocalBounds.IsValid
        ? FBoxSphereBounds{_LocalBounds}.TransformBy(InLocalToWorld)
        : FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0f};
}

#else

UCk_NavmeshDebugDraw_MeshComponent_UE::UCk_NavmeshDebugDraw_MeshComponent_UE()
{
    PrimaryComponentTick.bCanEverTick = false;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::IsSnapshotValid(
    const FCk_NavmeshDebugDraw_MeshSnapshot& InSnapshot) -> bool
{
    static_cast<void>(InSnapshot);
    return false;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    TryApplySnapshot(
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        const int32 InMaxGhostTriangleCount)
    -> bool
{
    static_cast<void>(InSnapshot);
    static_cast<void>(InMaxGhostTriangleCount);
    return false;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    ApplyValidatedSnapshot(
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        const int32 InMaxGhostTriangleCount)
    -> void
{
    static_cast<void>(InSnapshot);
    static_cast<void>(InMaxGhostTriangleCount);
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    TryApplyAreaPreview(
        TArray<FCk_NavmeshDebugDraw_AreaMesh> InAreaPreviewMeshes)
    -> bool
{
    static_cast<void>(InAreaPreviewMeshes);
    return false;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::ClearAreaPreview() -> bool
{
    return false;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    Remove_ExpiredGhostTriangles(
        const double InCurrentRealTimeSeconds)
    -> bool
{
    static_cast<void>(InCurrentRealTimeSeconds);
    return false;
}

auto UCk_NavmeshDebugDraw_MeshComponent_UE::Get_EarliestGhostExpiryRealTimeSeconds() const -> double
{
    return -1.0;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    Set_IsStale(
        const bool InIsStale)
    -> bool
{
    static_cast<void>(InIsStale);
    return false;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    Clear()
    -> void
{
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    CreateSceneProxy()
    -> FPrimitiveSceneProxy*
{
    return nullptr;
}

auto
    UCk_NavmeshDebugDraw_MeshComponent_UE::
    CalcBounds(
        const FTransform& InLocalToWorld) const
    -> FBoxSphereBounds
{
    static_cast<void>(InLocalToWorld);
    return FBoxSphereBounds{FVector::ZeroVector, FVector::ZeroVector, 0.0f};
}

#endif // WITH_CK_NAVMESH_DEBUG_DRAW
