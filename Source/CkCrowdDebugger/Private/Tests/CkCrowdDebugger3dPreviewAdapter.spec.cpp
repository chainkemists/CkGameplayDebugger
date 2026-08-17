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
