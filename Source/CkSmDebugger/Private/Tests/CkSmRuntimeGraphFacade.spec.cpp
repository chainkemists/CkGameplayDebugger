#if WITH_DEV_AUTOMATION_TESTS
#include "CkSmDebugger/Graph/CkSmRuntimeGraphFacade.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSmRuntimeGraphFacade_NameDepthTest,
                                 "Ck.SmDebugger.RuntimeGraphFacade.NameDepth",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
auto FCkSmRuntimeGraphFacade_NameDepthTest::RunTest(const FString&) -> bool
{
    auto Info = FCkSmDebugger_SmInfo{};
    Info.States.SetNum(1);
    Info.States[0].StateName = TEXT("CK.State.SubState");
    auto Facade = FCkSmRuntimeGraphFacade{};
    Facade.UpdateFromSmInfo(Info);
    TestEqual(TEXT("Facade exposes name depth from runtime DTO"), Facade.GetMaxNameDepth(), 3);
    auto& LayoutParams = Facade.EditLayoutParams();
    LayoutParams.ExpandTasks = false;
    LayoutParams.UndirectedBFS = true;
    LayoutParams.SpacingX = 420;
    LayoutParams.SpacingY = 160;
    LayoutParams.NameDepth = 2;
    Facade.RequestRelayout();
    Facade.UpdateFromSmInfo(Info);
    TestTrue(TEXT("Facade rebuilds a runtime scene"), Facade.HasScene());
    const auto& AppliedLayoutParams = Facade.GetLayoutParams();
    TestTrue(TEXT("Facade retains task expansion"), NOT AppliedLayoutParams.ExpandTasks);
    TestTrue(TEXT("Facade retains compact layout"), AppliedLayoutParams.UndirectedBFS);
    TestEqual(TEXT("Facade retains horizontal spacing"), AppliedLayoutParams.SpacingX, 420);
    TestEqual(TEXT("Facade retains vertical spacing"), AppliedLayoutParams.SpacingY, 160);
    TestEqual(TEXT("Facade retains name depth"), AppliedLayoutParams.NameDepth, 2);
    Facade.SetScrubHighlight(0, INDEX_NONE);
    TestEqual(TEXT("Facade retains scrub active state"), Facade.GetScrubActiveState(), 0);
    TestEqual(TEXT("Facade retains scrub exited state"), Facade.GetScrubExitedState(), INDEX_NONE);
    const auto& ScrubScene = Facade.GetScene();
    TestTrue(TEXT("Facade marks scrub-active runtime node"),
             ScrubScene.Nodes.ContainsByPredicate(
                 [](const FCkSmRuntimeGraphNode& Node)
                 {
                     return Node.StateIndex == 0 && Node.bScrubActive;
                 }));
    Facade.ClearScrubHighlight();
    TestEqual(TEXT("Facade clears scrub active state"), Facade.GetScrubActiveState(), INDEX_NONE);
    Facade.TickLivePresentation(0.0f, INDEX_NONE, 0, TSet<FString>{});
    Facade.ResetForWorldChange();
    TestFalse(TEXT("Facade drops runtime scene on world reset"), Facade.HasScene());
    TestEqual(TEXT("Facade clears runtime name depth source on world reset"),
              Facade.GetMaxNameDepth(),
              1);
    return true;
}
#endif
