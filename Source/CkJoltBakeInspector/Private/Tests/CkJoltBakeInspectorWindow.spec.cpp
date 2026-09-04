#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"
#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"
#include "CkJoltBakeInspector/CkJoltBakeInspector_Policy.h"

#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspectorWindow_Constructs,
    "Ck.Jolt.Cook.Inspector.Window.Constructs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspectorWindow_Constructs::RunTest(const FString&) -> bool
{
    const auto Window = SNew(SCkJoltBakeInspectorWindow);
    Window->SlatePrepass();
    TestTrue(TEXT("Bake inspector has a non-empty layout"), Window->GetDesiredSize().Y > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspectorPreview_RendersValueOnlyAuditTriangles,
    "Ck.Jolt.Cook.Inspector.Preview.RendersValueOnlyAuditTriangles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspectorPreview_RendersValueOnlyAuditTriangles::RunTest(const FString&) -> bool
{
    const auto Preview = SNew(SCkJoltBakeInspectorPreview);
    Preview->SlatePrepass();
    TestNotNull(TEXT("preview owns a world"), Preview->Get_PreviewWorld());

    auto Audit = ck::jolt::cook::FCk_Jolt_MeshShapeAuditResult{};
    Audit._SourcePreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
        FVector{0.0, 0.0, 0.0}, FVector{100.0, 0.0, 0.0}, FVector{0.0, 100.0, 0.0}});
    Audit._NormalizedPreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
        FVector{0.0, 0.0, 0.0}, FVector{0.0, 100.0, 0.0}, FVector{100.0, 0.0, 0.0}});
    Audit._CookedPreviewTriangles.Emplace(ck::jolt::cook::FCk_Jolt_MeshShapeAuditTriangle{
        FVector{0.0, 0.0, 0.0}, FVector{100.0, 0.0, 0.0}, FVector{0.0, 0.0, 100.0}});
    Audit._bCookedPreviewUnavailable = false;
    Preview->Show_Audit(Audit);
    TestTrue(TEXT("source, normalized, and current cooked audit triangles give the adapter valid frame bounds"),
        Preview->Get_RenderedBounds().IsValid != 0);
    Preview->Clear();
    TestFalse(TEXT("clearing the adapter removes its preview-world components before widget teardown"),
        Preview->Get_RenderedBounds().IsValid != 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspectorPolicy_RepairableActionsExcludeUnsafeRows,
    "Ck.Jolt.Cook.Inspector.Policy.RepairableActionsExcludeUnsafeRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspectorPolicy_RepairableActionsExcludeUnsafeRows::RunTest(const FString&) -> bool
{
    using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction;
    TestTrue(TEXT("missing output is eligible"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(CookMissing, false));
    TestTrue(TEXT("corrupt output is eligible"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(RebuildCorrupt, false));
    TestTrue(TEXT("inside-out output is eligible"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(RebuildInsideOut, false));
    TestFalse(TEXT("source repair is never submitted to bake all"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(FixSource, false));
    TestFalse(TEXT("orphan deletion is never submitted to bake all"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(DeleteOrphan, false));
    TestFalse(TEXT("a predicted failure is never submitted even when stale"), ck::jolt_bake_inspector::Get_IsRepairableBakeAction(RebuildStale, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltBakeInspectorAnalysisQueue_OnePerTickAndCancel,
    "Ck.Jolt.Cook.Inspector.AnalysisQueue.OnePerTickAndCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltBakeInspectorAnalysisQueue_OnePerTickAndCancel::RunTest(const FString&) -> bool
{
    auto Queue = ck::jolt_bake_inspector::FCkJoltBakeInspectorAnalysisQueue{};
    Queue.Start(3);
    TestTrue(TEXT("started queue is active"), Queue.IsActive());
    const auto First = Queue.TryTakeNext();
    TestTrue(TEXT("first tick claims work"), First.IsSet());
    if (First.IsSet()) { TestEqual(TEXT("first tick claims exactly the first item"), *First, 0); }
    TestEqual(TEXT("one item is processed after one tick"), Queue.Get_Processed(), 1);
    const auto Second = Queue.TryTakeNext();
    TestTrue(TEXT("second tick claims work"), Second.IsSet());
    if (Second.IsSet()) { TestEqual(TEXT("second tick claims exactly one next item"), *Second, 1); }
    Queue.Cancel();
    TestFalse(TEXT("cancel stops future asset work"), Queue.IsActive());
    TestFalse(TEXT("cancelled queue produces no next item"), Queue.TryTakeNext().IsSet());
    TestEqual(TEXT("cancel releases the recorded work total"), Queue.Get_Total(), 0);
    return true;
}

#endif
