#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"

#include "CkNavmeshDebugDraw_MeshComponent.generated.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_NavmeshDebugDraw_AreaMesh
{
    FColor _Color = FColor::White;
    TArray<FVector3f> _TriangleVertices;
    uint8 _AreaId = 0;

    auto operator==(const FCk_NavmeshDebugDraw_AreaMesh& InOther) const -> bool
    {
        return _Color == InOther._Color &&
            _TriangleVertices == InOther._TriangleVertices &&
            _AreaId == InOther._AreaId;
    }
};

struct FCk_NavmeshDebugDraw_MeshSnapshot
{
    uint64 _Revision = 0;
    TArray<FCk_NavmeshDebugDraw_AreaMesh> _AreaMeshes;
    TArray<FVector3f> _EdgeVertices;
};

struct FCk_NavmeshDebugDraw_GhostTriangle
{
    FVector3f _A = FVector3f::ZeroVector;
    FVector3f _B = FVector3f::ZeroVector;
    FVector3f _C = FVector3f::ZeroVector;
    double _FadeStartRealTimeSeconds = 0.0;

    auto operator==(const FCk_NavmeshDebugDraw_GhostTriangle& InOther) const -> bool = default;
};

class FCk_NavmeshDebugDraw_MeshSceneProxy;
class UCk_NavmeshDebugDraw_Subsystem_UE;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Transient)
class CKNAVMESHDEBUGDRAW_API UCk_NavmeshDebugDraw_MeshComponent_UE final : public UMeshComponent
{
    GENERATED_BODY()

public:
    UCk_NavmeshDebugDraw_MeshComponent_UE();

    static auto IsSnapshotValid(const FCk_NavmeshDebugDraw_MeshSnapshot& InSnapshot) -> bool;
    auto TryApplySnapshot(
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        int32 InMaxGhostTriangleCount = MAX_int32) -> bool;
    auto TryApplyAreaPreview(TArray<FCk_NavmeshDebugDraw_AreaMesh> InAreaPreviewMeshes) -> bool;
    auto ClearAreaPreview() -> bool;
    auto Remove_ExpiredGhostTriangles(double InCurrentRealTimeSeconds) -> bool;
    auto Set_IsStale(bool InIsStale) -> bool;
    auto Clear() -> void;

    auto Get_ContentSignature() const -> uint32 { return _ContentSignature; }
    auto Get_Revision() const -> uint64 { return _Revision; }
    auto Get_TriangleCount() const -> int32;
    auto Get_AreaPreviewTriangleCount() const -> int32;
    auto Get_GhostTriangleCount() const -> int32 { return _GhostTriangles.Num(); }
    auto Get_EarliestGhostExpiryRealTimeSeconds() const -> double;
    auto Get_EdgeCount() const -> int32 { return _EdgeVertices.Num() / 2; }
    auto Get_AreaMeshCount() const -> int32 { return _AreaMeshes.Num(); }
    auto Get_IsStale() const -> bool { return _IsStale; }
    auto DidLastApplyRecreateProxy() const -> bool { return _LastApplyRecreatedProxy; }

private:
    friend class FCk_NavmeshDebugDraw_MeshSceneProxy;
    friend class UCk_NavmeshDebugDraw_Subsystem_UE;

    auto ApplyValidatedSnapshot(
        FCk_NavmeshDebugDraw_MeshSnapshot InSnapshot,
        int32 InMaxGhostTriangleCount) -> void;

    virtual auto CreateSceneProxy() -> FPrimitiveSceneProxy* override;
    virtual auto CalcBounds(const FTransform& InLocalToWorld) const -> FBoxSphereBounds override;
    virtual auto GetNumMaterials() const -> int32 override { return 1; }

    TArray<FCk_NavmeshDebugDraw_AreaMesh> _AreaMeshes;
    TArray<FCk_NavmeshDebugDraw_AreaMesh> _AreaPreviewMeshes;
    TArray<FVector3f> _EdgeVertices;
    TArray<FCk_NavmeshDebugDraw_GhostTriangle> _GhostTriangles;
    FBox _LocalBounds = FBox{ForceInit};
    uint64 _Revision = 0;
    uint32 _ContentSignature = 0;
    bool _IsStale = false;
    bool _LastApplyRecreatedProxy = false;
};
