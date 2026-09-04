#include "CkJoltBakeInspector/Viewport/CkJoltBakeInspectorPreviewAdapter.h"

#include "CkDebugScene/CkDebugScene_Mesh.h"
#include "CkDebugScene/CkDebugScene_Materials.h"
#include "CkDebugScene/CkDebugScene_Target.h"

namespace ck_jolt_bake_inspector_preview
{
    constexpr uint64 SourceItemKey = 1;
    constexpr uint64 NormalizedItemKey = 2;
    constexpr uint64 CookedItemKey = 3;
    const FName LabelChannel{TEXT("JoltBakeInspectorLabels")};

    auto MakeMesh(const TArray<ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle>& InTriangles)
        -> TSharedPtr<FCk_DebugScene_Mesh>
    {
        auto Triangles = TArray<FCk_DebugScene_Triangle>{};
        Triangles.Reserve(InTriangles.Num());
        for (const auto& Triangle : InTriangles)
        { Triangles.Emplace(FCk_DebugScene_Triangle{Triangle._A, Triangle._B, Triangle._C}); }
        return Triangles.IsEmpty() ? TSharedPtr<FCk_DebugScene_Mesh>{}
                                   : FCk_DebugScene_Mesh::Create_FromTriangles(MoveTemp(Triangles));
    }

    auto GetBounds(const TArray<ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle>& InTriangles) -> FBox
    {
        auto Bounds = FBox{ForceInit};
        for (const auto& Triangle : InTriangles)
        { Bounds += Triangle._A; Bounds += Triangle._B; Bounds += Triangle._C; }
        return Bounds;
    }

    auto MakeInstance(TSharedPtr<FCk_DebugScene_Mesh> InMesh, FVector InOffset, FLinearColor InColor)
        -> FCk_DebugScene_Instance
    {
        auto Appearance = FCk_DebugScene_Appearance{};
        Appearance.Set_BaseMaterial(ck::debug_scene::materials::TryGet_Opaque())
            .Set_RenderClass(ECk_DebugScene_RenderClass::Opaque).Set_Color(InColor);
        return FCk_DebugScene_Instance{}.Set_Mesh(MoveTemp(InMesh)).Set_Transform(FTransform{FQuat::Identity, InOffset}).Set_Appearance(MoveTemp(Appearance));
    }
}

auto FCkJoltBakeInspectorPreviewAdapter::Set_Target(TSharedPtr<FCk_DebugScene_Target> InTarget) -> void
{ _Target = MoveTemp(InTarget); }

auto FCkJoltBakeInspectorPreviewAdapter::Reconcile(const ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult& InAudit) -> void
{
    if (NOT _Target.IsValid()) { return; }

    const auto SourceBounds = ck_jolt_bake_inspector_preview::GetBounds(InAudit._SourcePreviewTriangles);
    const auto NormalizedBounds = ck_jolt_bake_inspector_preview::GetBounds(InAudit._NormalizedPreviewTriangles);
    const auto CookedBounds = ck_jolt_bake_inspector_preview::GetBounds(InAudit._CookedPreviewTriangles);
    auto Bounds = SourceBounds;
    if (NormalizedBounds.IsValid)
    {
        Bounds += NormalizedBounds.Min;
        Bounds += NormalizedBounds.Max;
    }
    if (CookedBounds.IsValid)
    {
        Bounds += CookedBounds.Min;
        Bounds += CookedBounds.Max;
    }
    const auto HalfWidth = Bounds.IsValid ? FMath::Max(100.0, Bounds.GetExtent().X + 50.0) : 100.0;

    _Target->Begin_Reconcile();
    if (const auto SourceMesh = ck_jolt_bake_inspector_preview::MakeMesh(InAudit._SourcePreviewTriangles); SourceMesh.IsValid())
    {
        const auto Instances = TArray<FCk_DebugScene_Instance>{ck_jolt_bake_inspector_preview::MakeInstance(
            SourceMesh, FVector{-2.0 * HalfWidth, 0.0, 0.0}, FLinearColor{0.90f, 0.20f, 0.20f})};
        if (NOT _Target->Upsert_Item(ck_jolt_bake_inspector_preview::SourceItemKey, Instances))
        { _Target->Abort_Reconcile(); return; }
    }
    if (const auto NormalizedMesh = ck_jolt_bake_inspector_preview::MakeMesh(InAudit._NormalizedPreviewTriangles); NormalizedMesh.IsValid())
    {
        const auto Instances = TArray<FCk_DebugScene_Instance>{ck_jolt_bake_inspector_preview::MakeInstance(
            NormalizedMesh, FVector::ZeroVector, FLinearColor{0.15f, 0.75f, 0.85f})};
        if (NOT _Target->Upsert_Item(ck_jolt_bake_inspector_preview::NormalizedItemKey, Instances))
        { _Target->Abort_Reconcile(); return; }
    }
    if (const auto CookedMesh = ck_jolt_bake_inspector_preview::MakeMesh(InAudit._CookedPreviewTriangles); CookedMesh.IsValid())
    {
        const auto Instances = TArray<FCk_DebugScene_Instance>{ck_jolt_bake_inspector_preview::MakeInstance(
            CookedMesh, FVector{2.0 * HalfWidth, 0.0, 0.0}, FLinearColor{0.30f, 0.90f, 0.35f})};
        if (NOT _Target->Upsert_Item(ck_jolt_bake_inspector_preview::CookedItemKey, Instances))
        { _Target->Abort_Reconcile(); return; }
    }
    if (NOT _Target->End_Reconcile()) { return; }

    auto Labels = TArray<FCk_DebugScene_Label>{};
    Labels.Emplace(FCk_DebugScene_Label{FVector{-2.0 * HalfWidth, 0.0, 0.0},
        InAudit._bSourcePreviewTruncated ? TEXT("SOURCE (TRUNCATED)") : TEXT("SOURCE"), FLinearColor{0.90f, 0.20f, 0.20f}});
    Labels.Emplace(FCk_DebugScene_Label{FVector::ZeroVector,
        InAudit._bNormalizedPreviewTruncated ? TEXT("NORMALIZED JOLT CANDIDATE (TRUNCATED)") : TEXT("NORMALIZED JOLT CANDIDATE"),
        FLinearColor{0.15f, 0.75f, 0.85f}});
    Labels.Emplace(FCk_DebugScene_Label{FVector{2.0 * HalfWidth, 0.0, 0.0},
        InAudit._bCookedPreviewUnavailable ? TEXT("CURRENT COOKED JOLT SHAPE (UNAVAILABLE)")
        : InAudit._bCookedPreviewTruncated ? TEXT("CURRENT COOKED JOLT SHAPE (TRUNCATED)") : TEXT("CURRENT COOKED JOLT SHAPE"),
        FLinearColor{0.30f, 0.90f, 0.35f}});
    _Target->Set_LabelChannel(ck_jolt_bake_inspector_preview::LabelChannel, MoveTemp(Labels));
}

auto FCkJoltBakeInspectorPreviewAdapter::Reset() -> void
{
    if (NOT _Target.IsValid()) { return; }
    _Target->Begin_Reconcile();
    _Target->End_Reconcile();
    _Target->Clear_LabelChannel(ck_jolt_bake_inspector_preview::LabelChannel);
}

auto FCkJoltBakeInspectorPreviewAdapter::Get_FrameBounds(ECkDebug3dFrameTarget) const -> FBox
{ return _Target.IsValid() ? _Target->Get_ContentBounds() : FBox{ForceInit}; }

auto FCkJoltBakeInspectorPreviewAdapter::Get_SelectionCenter() const -> TOptional<FVector>
{
    const auto Bounds = Get_FrameBounds(ECkDebug3dFrameTarget::All);
    return Bounds.IsValid ? TOptional<FVector>{Bounds.GetCenter()} : TOptional<FVector>{};
}

auto FCkJoltBakeInspectorPreviewAdapter::Get_Capabilities() const -> ECkDebug3dViewportCapability
{ return ECkDebug3dViewportCapability::None; }

auto FCkJoltBakeInspectorPreviewAdapter::On_ViewportTeardown() -> void
{
    Reset();
    _Target.Reset();
}
