#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dPreviewAdapter.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"

#include "CkCrowdDebugger/Settings/CkCrowdDebuggerSettings.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dPreviewAdapter_Contract,
                                 "Ck.CrowdDebugger.Viewport3dPreviewAdapter.Contract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
auto
    FCkCrowdDebugger3dPreviewAdapter_Contract::
    RunTest(const FString&)
    -> bool
{
    auto Adapter = FCkCrowdDebugger_3dPreviewAdapter{};
    TestTrue(TEXT("Crowd exposes selection camera capabilities"),
             EnumHasAllFlags(Adapter.Get_Capabilities(), ECkDebug3dViewportCapability::FrameSelection |
                                                             ECkDebug3dViewportCapability::FollowSelection |
                                                             ECkDebug3dViewportCapability::IsolateSelection));
    Adapter.Set_RenderMode(ECkDebug3dRenderMode::TransparentOnly);
    TestEqual(TEXT("transparent-only render mode is retained"), Adapter.Get_RenderMode(),
              ECkDebug3dRenderMode::TransparentOnly);
    Adapter.On_ViewportTeardown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dPreviewAdapter_PathNetworkOpacity,
                                 "Ck.CrowdDebugger.Viewport3dPreviewAdapter.PathNetworkOpacity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
auto
    FCkCrowdDebugger3dPreviewAdapter_PathNetworkOpacity::
    RunTest(const FString&)
    -> bool
{
    auto* Settings = GetMutableDefault<UCkCrowdDebuggerSettings>();
    if (NOT TestNotNull(TEXT("Crowd debugger settings exist"), Settings))
    {
        return false;
    }

    const auto OriginalOpacity = Settings->PathNetworkOpacity;
    const auto Viewport = SNew(SCkCrowdDebugger_3dViewport);
    const auto Ribbons = TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>{
        FCkCrowdDebugger_PathNetworkRibbonSnapshot{{FVector::ZeroVector, FVector{100.0, 0.0, 0.0}}, {25.0f, 25.0f}}};

    Settings->PathNetworkOpacity = 0.27f;
    Viewport->Set_PathNetworkRibbons(Ribbons);
    const auto InitialRevision = Viewport->Get_PathNetworkRevision_ForTests();
    TestEqual(TEXT("configured path-network opacity reaches the copied snapshot"),
              Viewport->Get_PathNetworkOpacity_ForTests(), 0.27f);

    Settings->PathNetworkOpacity = 0.61f;
    Viewport->Set_PathNetworkRibbons(Ribbons);
    TestEqual(TEXT("opacity-only changes update the copied snapshot"), Viewport->Get_PathNetworkOpacity_ForTests(),
              0.61f);
    TestTrue(TEXT("opacity-only changes invalidate retained ribbon appearance"),
             Viewport->Get_PathNetworkRevision_ForTests() > InitialRevision);

    Settings->PathNetworkOpacity = OriginalOpacity;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebugger3dPreviewAdapter_AvoidanceVolumeCopyAndSignature,
                                 "Ck.CrowdDebugger.Viewport3dPreviewAdapter.AvoidanceVolumeCopyAndSignature",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
auto FCkCrowdDebugger3dPreviewAdapter_AvoidanceVolumeCopyAndSignature::RunTest(const FString&) -> bool
{
    auto Viewport = SNew(SCkCrowdDebugger_3dViewport);
    auto Source = TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>{};
    auto Volume = FCkCrowdDebugger_AvoidanceVolumeSnapshot{};
    Volume.Identity = 7001;
    Volume.YawWorldTransform = FTransform{FRotator{0.0f, 35.0f, 0.0f}, FVector{100.0f, 200.0f, 0.0f}};
    Volume.PhysicalWorldHalfExtents = FVector{100.0f, 60.0f, 80.0f};
    Volume.InfluenceWorldHalfExtents = FVector{160.0f, 120.0f, 80.0f};
    Volume.PaintedWorldHalfExtents = FVector{220.0f, 180.0f, 80.0f};
    Volume.State = ECkCrowdDebugger_AvoidanceVolumeState::Confirmed;
	Volume.TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::AvoidIfPossible;
    Volume.HasValidGeometry = true;
    Source.Add(Volume);

    Viewport->Set_AvoidanceVolumeSnapshots(Source);
    const auto FirstRevision = Viewport->Get_AvoidanceVolumeRevision_ForTests();
    TestEqual(TEXT("viewport keeps a copied avoidance-volume count"),
        Viewport->Get_AvoidanceVolumeCount_ForTests(), 1);
    Source.Reset();
    TestEqual(TEXT("clearing producer data cannot clear the viewport copy"),
        Viewport->Get_AvoidanceVolumeCount_ForTests(), 1);

    Viewport->Set_AvoidanceVolumeSnapshots(TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>{Volume});
    TestEqual(TEXT("identical avoidance input preserves its publication revision"),
        Viewport->Get_AvoidanceVolumeRevision_ForTests(), FirstRevision);

    Volume.State = ECkCrowdDebugger_AvoidanceVolumeState::Pending;
    Viewport->Set_AvoidanceVolumeSnapshots(TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>{Volume});
    TestTrue(TEXT("state-only changes invalidate retained avoidance presentation"),
        Viewport->Get_AvoidanceVolumeRevision_ForTests() > FirstRevision);

	const auto StateRevision = Viewport->Get_AvoidanceVolumeRevision_ForTests();
	Volume.TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::HardExclude;
	Viewport->Set_AvoidanceVolumeSnapshots(TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>{Volume});
	TestTrue(TEXT("policy-only changes invalidate retained avoidance presentation"),
		Viewport->Get_AvoidanceVolumeRevision_ForTests() > StateRevision);

    Viewport->Notify_WorldChanged();
    TestEqual(TEXT("world change clears copied avoidance volumes"),
        Viewport->Get_AvoidanceVolumeCount_ForTests(), 0);
    TestEqual(TEXT("world change resets the avoidance publication revision"),
        Viewport->Get_AvoidanceVolumeRevision_ForTests(), uint64{0});
    return true;
}
