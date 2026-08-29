#include "CkNavmeshDebugDraw/Rendering/CkNavmeshDebugDraw_MeshComponent.h"

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_navmesh_debug_draw_mesh_component_spec
{
    auto MakeValidSnapshot(uint64 InRevision = 1) -> FCk_NavmeshDebugDraw_MeshSnapshot
    {
        auto Snapshot = FCk_NavmeshDebugDraw_MeshSnapshot{};
        Snapshot._Revision = InRevision;
        Snapshot._AreaMeshes.Add(FCk_NavmeshDebugDraw_AreaMesh{
            FColor::Green,
            {
                FVector3f{0.0f, 0.0f, 0.0f},
                FVector3f{100.0f, 0.0f, 0.0f},
                FVector3f{0.0f, 100.0f, 0.0f},
            }});
        Snapshot._EdgeVertices = {
            FVector3f{0.0f, 0.0f, 10.0f},
            FVector3f{100.0f, 0.0f, 10.0f},
        };
        return Snapshot;
    }

    auto MakeTwoTriangleSnapshot(uint64 InRevision = 1) -> FCk_NavmeshDebugDraw_MeshSnapshot
    {
        auto Snapshot = MakeValidSnapshot(InRevision);
        Snapshot._AreaMeshes[0]._TriangleVertices.Append({
            FVector3f{100.0f, 0.0f, 0.0f},
            FVector3f{100.0f, 100.0f, 0.0f},
            FVector3f{0.0f, 100.0f, 0.0f},
        });
        Snapshot._EdgeVertices.Append({
            FVector3f{100.0f, 0.0f, 10.0f},
            FVector3f{100.0f, 100.0f, 10.0f},
            FVector3f{100.0f, 100.0f, 10.0f},
            FVector3f{0.0f, 100.0f, 10.0f},
            FVector3f{0.0f, 100.0f, 10.0f},
            FVector3f{100.0f, 0.0f, 10.0f},
        });
        return Snapshot;
    }
} // namespace ck_navmesh_debug_draw_mesh_component_spec

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_ValidApply,
    "Ck.NavmeshDebugDraw.MeshComponent.ValidApply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_ValidApply::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};

    TestTrue(TEXT("Valid area mesh and edge pair are accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7)));
    TestEqual(TEXT("Accepted snapshot retains one area mesh"), Component->Get_AreaMeshCount(), 1);
    TestEqual(TEXT("Accepted snapshot retains one triangle"), Component->Get_TriangleCount(), 1);
    TestEqual(TEXT("Accepted snapshot retains one edge"), Component->Get_EdgeCount(), 1);
    TestEqual(TEXT("Accepted snapshot records its revision"), Component->Get_Revision(), uint64{7});
    TestNotEqual(TEXT("Accepted snapshot has a content signature"), Component->Get_ContentSignature(), uint32{0});
    TestTrue(TEXT("New content recreates its proxy"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_IdenticalContent,
    "Ck.NavmeshDebugDraw.MeshComponent.IdenticalContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_IdenticalContent::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto Snapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7);
    Component->TryApplySnapshot(Snapshot);
    const auto InitialSignature = Component->Get_ContentSignature();

    auto SameContentNewRevision = Snapshot;
    SameContentNewRevision._Revision = 8;
    TestTrue(TEXT("Identical content is accepted"), Component->TryApplySnapshot(SameContentNewRevision));
    TestEqual(TEXT("Identical content updates revision"), Component->Get_Revision(), uint64{8});
    TestEqual(TEXT("Identical content keeps signature"), Component->Get_ContentSignature(), InitialSignature);
    TestFalse(TEXT("Identical content does not recreate proxy"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_ColorOnlyChange,
    "Ck.NavmeshDebugDraw.MeshComponent.ColorOnlyChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_ColorOnlyChange::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto Snapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7);
    Component->TryApplySnapshot(Snapshot);
    const auto InitialSignature = Component->Get_ContentSignature();

    auto ColorChangedSnapshot = Snapshot;
    ColorChangedSnapshot._Revision = 8;
    ColorChangedSnapshot._AreaMeshes[0]._Color = FColor::Blue;
    TestTrue(TEXT("Color-only change is accepted"), Component->TryApplySnapshot(ColorChangedSnapshot));
    TestNotEqual(TEXT("Color-only change updates content signature"), Component->Get_ContentSignature(), InitialSignature);
    TestTrue(TEXT("Color-only change recreates proxy"), Component->DidLastApplyRecreateProxy());
    TestEqual(TEXT("Color-only change retains one area mesh"), Component->Get_AreaMeshCount(), 1);
    TestEqual(TEXT("Color-only change retains one edge"), Component->Get_EdgeCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_EdgeOnlyChange,
    "Ck.NavmeshDebugDraw.MeshComponent.EdgeOnlyChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_EdgeOnlyChange::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto Snapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7);
    Component->TryApplySnapshot(Snapshot);
    const auto InitialSignature = Component->Get_ContentSignature();

    auto EdgeChangedSnapshot = Snapshot;
    EdgeChangedSnapshot._Revision = 8;
    EdgeChangedSnapshot._EdgeVertices[1] = FVector3f{200.0f, 0.0f, 10.0f};
    TestTrue(TEXT("Edge-only change is accepted"), Component->TryApplySnapshot(EdgeChangedSnapshot));
    TestNotEqual(TEXT("Edge-only change updates content signature"), Component->Get_ContentSignature(), InitialSignature);
    TestTrue(TEXT("Edge-only change recreates proxy"), Component->DidLastApplyRecreateProxy());
    TestEqual(TEXT("Edge-only change retains one area mesh"), Component->Get_AreaMeshCount(), 1);
    TestEqual(TEXT("Edge-only change retains one edge"), Component->Get_EdgeCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_InvalidApplyIsAtomic,
    "Ck.NavmeshDebugDraw.MeshComponent.InvalidApplyIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_InvalidApplyIsAtomic::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7));
    Component->Set_IsStale(true);
    const auto PreviousRevision = Component->Get_Revision();
    const auto PreviousSignature = Component->Get_ContentSignature();
    const auto PreviousTriangleCount = Component->Get_TriangleCount();

    auto InvalidSnapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(8);
    InvalidSnapshot._AreaMeshes[0]._TriangleVertices[2] = InvalidSnapshot._AreaMeshes[0]._TriangleVertices[1];
    AddExpectedError(
        TEXT("Navmesh mesh snapshot must contain valid area triangles and paired finite edges"),
        EAutomationExpectedErrorFlags::Contains,
        2);
    TestFalse(TEXT("Degenerate area mesh is rejected"), Component->TryApplySnapshot(InvalidSnapshot));
    TestEqual(TEXT("Rejected snapshot preserves revision"), Component->Get_Revision(), PreviousRevision);
    TestEqual(TEXT("Rejected snapshot preserves signature"), Component->Get_ContentSignature(), PreviousSignature);
    TestEqual(TEXT("Rejected snapshot preserves geometry"), Component->Get_TriangleCount(), PreviousTriangleCount);
    TestTrue(TEXT("Rejected snapshot preserves stale display state"), Component->Get_IsStale());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_StaleDisplayState,
    "Ck.NavmeshDebugDraw.MeshComponent.StaleDisplayState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_StaleDisplayState::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto Snapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7);
    TestTrue(TEXT("Valid snapshot is accepted"), Component->TryApplySnapshot(Snapshot));
    const auto InitialSignature = Component->Get_ContentSignature();
    const auto InitialAreaMeshCount = Component->Get_AreaMeshCount();
    const auto InitialTriangleCount = Component->Get_TriangleCount();
    const auto InitialEdgeCount = Component->Get_EdgeCount();

    TestFalse(TEXT("Fresh snapshot is not stale"), Component->Get_IsStale());
    TestTrue(TEXT("Stale transition changes state"), Component->Set_IsStale(true));
    TestTrue(TEXT("Stale transition marks display stale"), Component->Get_IsStale());
    TestFalse(TEXT("Repeated stale transition is idempotent"), Component->Set_IsStale(true));
    TestTrue(TEXT("Repeated stale transition retains stale state"), Component->Get_IsStale());

    auto FreshSnapshot = Snapshot;
    FreshSnapshot._Revision = 8;
    TestTrue(TEXT("Identical fresh snapshot is accepted"), Component->TryApplySnapshot(FreshSnapshot));
    TestFalse(TEXT("Fresh snapshot clears stale state"), Component->Get_IsStale());
    TestTrue(TEXT("Fresh snapshot recreates proxy to clear stale display"), Component->DidLastApplyRecreateProxy());
    TestEqual(TEXT("Fresh snapshot updates revision"), Component->Get_Revision(), uint64{8});
    TestEqual(TEXT("Fresh snapshot preserves content signature"), Component->Get_ContentSignature(), InitialSignature);
    TestEqual(TEXT("Fresh snapshot preserves area mesh count"), Component->Get_AreaMeshCount(), InitialAreaMeshCount);
    TestEqual(TEXT("Fresh snapshot preserves triangle count"), Component->Get_TriangleCount(), InitialTriangleCount);
    TestEqual(TEXT("Fresh snapshot preserves edge count"), Component->Get_EdgeCount(), InitialEdgeCount);

    Component->Clear();
    TestFalse(TEXT("Clear leaves stale state disabled"), Component->Get_IsStale());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_PolygonGhostLifecycle,
    "Ck.NavmeshDebugDraw.MeshComponent.PolygonGhostLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_PolygonGhostLifecycle::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    TestTrue(TEXT("Two-triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(7)));
    TestEqual(TEXT("Initial snapshot has two live triangles"), Component->Get_TriangleCount(), 2);
    TestEqual(TEXT("Initial snapshot has no ghost triangles"), Component->Get_GhostTriangleCount(), 0);

    TestTrue(TEXT("One-triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(8)));
    TestEqual(TEXT("Removed triangle leaves one live triangle"), Component->Get_TriangleCount(), 1);
    TestEqual(TEXT("Removed triangle becomes one ghost triangle"), Component->Get_GhostTriangleCount(), 1);
    TestTrue(TEXT("Removal recreates the proxy for the ghost"), Component->DidLastApplyRecreateProxy());

    TestTrue(TEXT("Reappearing triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(9)));
    TestEqual(TEXT("Reappearance restores two live triangles"), Component->Get_TriangleCount(), 2);
    TestEqual(TEXT("Reappearance clears the ghost triangle"), Component->Get_GhostTriangleCount(), 0);
    TestTrue(TEXT("Reappearance recreates the proxy"), Component->DidLastApplyRecreateProxy());

    TestTrue(TEXT("Repeated removal snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(10)));
    TestEqual(TEXT("Repeated removal restores one ghost triangle"), Component->Get_GhostTriangleCount(), 1);
    const auto AgedGhostExpiry = Component->Get_EarliestGhostExpiryRealTimeSeconds();
    TestTrue(TEXT("Repeated omission snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(11)));
    TestEqual(TEXT("Repeated omission retains the aged ghost"), Component->Get_GhostTriangleCount(), 1);
    TestEqual(TEXT("Repeated omission preserves the ghost expiry"),
              Component->Get_EarliestGhostExpiryRealTimeSeconds(), AgedGhostExpiry);
    TestFalse(TEXT("Aged ghost remains before its original expiry"),
              Component->Remove_ExpiredGhostTriangles(AgedGhostExpiry - 0.01));
    TestEqual(TEXT("Aged ghost remains before expiry"), Component->Get_GhostTriangleCount(), 1);
    TestTrue(TEXT("Aged ghost expires just after its original expiry"),
             Component->Remove_ExpiredGhostTriangles(AgedGhostExpiry + 0.01));
    TestEqual(TEXT("Repeated omission does not reset the ghost age"), Component->Get_GhostTriangleCount(), 0);

    TestTrue(TEXT("Final reappearing triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(12)));
    TestTrue(TEXT("Final removal snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(13)));
    TestEqual(TEXT("Final removal restores one ghost triangle"), Component->Get_GhostTriangleCount(), 1);
    Component->Clear();
    TestEqual(TEXT("Clear removes ghost triangles"), Component->Get_GhostTriangleCount(), 0);
    TestTrue(TEXT("Clear recreates the empty proxy state"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_GhostFadeExpiry,
    "Ck.NavmeshDebugDraw.MeshComponent.GhostFadeExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_GhostFadeExpiry::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    TestTrue(TEXT("Two-triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(7)));
    TestTrue(TEXT("One-triangle snapshot creates a ghost"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(8)));
    TestEqual(TEXT("Removal creates one ghost triangle"), Component->Get_GhostTriangleCount(), 1);

    const auto GhostExpiry = Component->Get_EarliestGhostExpiryRealTimeSeconds();
    TestFalse(TEXT("Ghost remains before its two-second expiry"),
              Component->Remove_ExpiredGhostTriangles(GhostExpiry - 0.01));
    TestEqual(TEXT("Ghost remains immediately before expiry"), Component->Get_GhostTriangleCount(), 1);
    TestFalse(TEXT("Pre-expiry removal does not recreate the proxy"), Component->DidLastApplyRecreateProxy());
    TestTrue(TEXT("Ghost expires after its two-second lifetime"),
             Component->Remove_ExpiredGhostTriangles(GhostExpiry + 0.01));
    TestEqual(TEXT("Post-expiry removal clears the ghost"), Component->Get_GhostTriangleCount(), 0);
    TestTrue(TEXT("Expiry recreates the proxy"), Component->DidLastApplyRecreateProxy());
    TestFalse(TEXT("Empty expiry removal does nothing"),
              Component->Remove_ExpiredGhostTriangles(GhostExpiry + 0.02));
    TestFalse(TEXT("Empty expiry removal does not recreate the proxy"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_AreaPreviewPreservesAuthoritativeState,
    "Ck.NavmeshDebugDraw.MeshComponent.AreaPreviewPreservesAuthoritativeState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_AreaPreviewPreservesAuthoritativeState::RunTest(
    const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    TestTrue(TEXT("Two-triangle snapshot is accepted"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(7)));
    TestTrue(TEXT("One-triangle snapshot creates a ghost"),
             Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(8)));
    TestTrue(TEXT("Component enters stale display state"), Component->Set_IsStale(true));
    const auto PreviousRevision = Component->Get_Revision();
    const auto PreviousTriangleCount = Component->Get_TriangleCount();
    const auto PreviousEdgeCount = Component->Get_EdgeCount();
    const auto PreviousGhostCount = Component->Get_GhostTriangleCount();
    const auto PreviousGhostExpiry = Component->Get_EarliestGhostExpiryRealTimeSeconds();
    auto PreviewMeshes = ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(1)._AreaMeshes;
    TestTrue(TEXT("Preview fixture contains one area mesh"), PreviewMeshes.IsValidIndex(0));
    if (NOT PreviewMeshes.IsValidIndex(0))
    {
        return false;
    }
    PreviewMeshes[0]._Color = FColor::Yellow;

    TestTrue(TEXT("Valid non-empty preview is applied"), Component->TryApplyAreaPreview(PreviewMeshes));
    TestEqual(TEXT("Preview has two triangles"), Component->Get_AreaPreviewTriangleCount(), 2);
    TestEqual(TEXT("Preview preserves live geometry"), Component->Get_TriangleCount(), PreviousTriangleCount);
    TestEqual(TEXT("Preview preserves edges"), Component->Get_EdgeCount(), PreviousEdgeCount);
    TestEqual(TEXT("Preview preserves revision"), Component->Get_Revision(), PreviousRevision);
    TestEqual(TEXT("Preview preserves ghost count"), Component->Get_GhostTriangleCount(), PreviousGhostCount);
    TestEqual(TEXT("Preview preserves ghost expiry"),
              Component->Get_EarliestGhostExpiryRealTimeSeconds(), PreviousGhostExpiry);
    TestTrue(TEXT("Preview preserves stale display state"), Component->Get_IsStale());
    TestTrue(TEXT("Preview recreates the proxy"), Component->DidLastApplyRecreateProxy());

    auto InvalidPreviewMeshes = PreviewMeshes;
    InvalidPreviewMeshes[0]._TriangleVertices.Reset();
    AddExpectedError(
        TEXT("Navmesh area preview must contain valid non-empty triangles"),
        EAutomationExpectedErrorFlags::Contains,
        2);
    TestFalse(TEXT("Invalid preview is rejected"), Component->TryApplyAreaPreview(MoveTemp(InvalidPreviewMeshes)));
    TestEqual(TEXT("Rejected preview preserves the prior colored overlay"),
              Component->Get_AreaPreviewTriangleCount(), 2);

    TestFalse(TEXT("Identical preview is idempotent"), Component->TryApplyAreaPreview(PreviewMeshes));
    TestFalse(TEXT("Identical preview does not recreate the proxy"), Component->DidLastApplyRecreateProxy());
    TestTrue(TEXT("Preview clear changes visible state"), Component->ClearAreaPreview());
    TestEqual(TEXT("Preview clear removes preview triangles"), Component->Get_AreaPreviewTriangleCount(), 0);
    TestFalse(TEXT("Repeated preview clear is idempotent"), Component->ClearAreaPreview());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_AuthoritativeSnapshotClearsAreaPreview,
    "Ck.NavmeshDebugDraw.MeshComponent.AuthoritativeSnapshotClearsAreaPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_AuthoritativeSnapshotClearsAreaPreview::RunTest(
    const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto Snapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7);
    TestTrue(TEXT("Initial snapshot is accepted"), Component->TryApplySnapshot(Snapshot));
    TestTrue(TEXT("Preview is applied"), Component->TryApplyAreaPreview(
                 ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(1)._AreaMeshes));
    TestEqual(TEXT("Preview is visible before authoritative apply"), Component->Get_AreaPreviewTriangleCount(), 2);

    auto AuthoritativeSnapshot = Snapshot;
    AuthoritativeSnapshot._Revision = 8;
    TestTrue(TEXT("Authoritative snapshot is accepted"), Component->TryApplySnapshot(AuthoritativeSnapshot));
    TestEqual(TEXT("Authoritative snapshot clears preview in its replacement"),
              Component->Get_AreaPreviewTriangleCount(), 0);
    TestEqual(TEXT("Authoritative snapshot retains live geometry"), Component->Get_TriangleCount(), 1);
    TestEqual(TEXT("Authoritative snapshot updates revision"), Component->Get_Revision(), uint64{8});
    TestTrue(TEXT("Authoritative snapshot recreates the proxy"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_ColorOnlyChangeDoesNotGhost,
    "Ck.NavmeshDebugDraw.MeshComponent.ColorOnlyChangeDoesNotGhost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_ColorOnlyChangeDoesNotGhost::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    const auto InitialSnapshot = ck_navmesh_debug_draw_mesh_component_spec::MakeTwoTriangleSnapshot(7);
    TestTrue(TEXT("Initial two-triangle snapshot is accepted"), Component->TryApplySnapshot(InitialSnapshot));

    auto ColorChangedSnapshot = InitialSnapshot;
    ColorChangedSnapshot._Revision = 8;
    ColorChangedSnapshot._AreaMeshes[0]._Color = FColor::Blue;
    TestTrue(TEXT("Color-only snapshot is accepted"), Component->TryApplySnapshot(ColorChangedSnapshot));
    TestEqual(TEXT("Color-only change retains both live triangles"), Component->Get_TriangleCount(), 2);
    TestEqual(TEXT("Color-only change creates no ghost triangles"), Component->Get_GhostTriangleCount(), 0);
    TestTrue(TEXT("Color-only change recreates the proxy"), Component->DidLastApplyRecreateProxy());

    auto IdenticalSnapshot = ColorChangedSnapshot;
    IdenticalSnapshot._Revision = 9;
    TestTrue(TEXT("Identical snapshot is accepted"), Component->TryApplySnapshot(IdenticalSnapshot));
    TestEqual(TEXT("Identical snapshot preserves zero ghost triangles"), Component->Get_GhostTriangleCount(), 0);
    TestFalse(TEXT("Identical snapshot does not recreate the proxy"), Component->DidLastApplyRecreateProxy());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkNavmeshDebugDraw_MeshComponent_Clear,
    "Ck.NavmeshDebugDraw.MeshComponent.Clear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkNavmeshDebugDraw_MeshComponent_Clear::RunTest(const FString& Parameters)
{
    auto Component = TStrongObjectPtr<UCk_NavmeshDebugDraw_MeshComponent_UE>{NewObject<UCk_NavmeshDebugDraw_MeshComponent_UE>()};
    Component->TryApplySnapshot(ck_navmesh_debug_draw_mesh_component_spec::MakeValidSnapshot(7));

    Component->Clear();
    TestEqual(TEXT("Clear removes all area meshes"), Component->Get_AreaMeshCount(), 0);
    TestEqual(TEXT("Clear removes all triangles"), Component->Get_TriangleCount(), 0);
    TestEqual(TEXT("Clear removes all edges"), Component->Get_EdgeCount(), 0);
    TestEqual(TEXT("Clear removes the content signature"), Component->Get_ContentSignature(), uint32{0});
    TestEqual(TEXT("Clear resets revision"), Component->Get_Revision(), uint64{0});
    TestTrue(TEXT("Clear recreates the empty proxy state"), Component->DidLastApplyRecreateProxy());
    return true;
}

#endif
