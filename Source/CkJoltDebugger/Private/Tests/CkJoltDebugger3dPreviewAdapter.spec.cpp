#include "CkJoltDebugger/Viewport/CkJoltDebugger_3dPreviewAdapter.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkJoltDebugger3dPreviewAdapter_Contract,
                                 "Ck.JoltDebugger.Viewport3dPreviewAdapter.Contract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkJoltDebugger3dPreviewAdapter_Contract::
    RunTest(const FString&)
    -> bool
{
    auto Adapter = FCkJoltDebugger_3dPreviewAdapter{};
    TestTrue(TEXT("Jolt advertises every Common selection/draw capability"),
             EnumHasAllFlags(Adapter.Get_Capabilities(), ECkDebug3dViewportCapability::Labels |
                                                             ECkDebug3dViewportCapability::DirectionGlyphScale |
                                                             ECkDebug3dViewportCapability::FollowSelection |
                                                             ECkDebug3dViewportCapability::IsolateSelection |
                                                             ECkDebug3dViewportCapability::FrameSelection));
    constexpr auto InformEngineOfWorld = false;
    auto* World = UWorld::CreateWorld(EWorldType::Game, InformEngineOfWorld);
    if (NOT TestNotNull(TEXT("transient preview world exists"), World))
    {
        return false;
    }

    auto Target = TSharedPtr<FCk_Jolt_DebugDrawTarget>{MakeShared<FCk_Jolt_DebugDrawTarget>(World)};
    Adapter.Set_Target(Target);

    auto GridChanges = 0;
    Adapter.Set_OnGridChanged([&GridChanges](bool) { ++GridChanges; });
    TestTrue(TEXT("grid is retained before a target arrives"), Adapter.Get_ShowGrid());
    constexpr auto HideGrid = false;
    Adapter.Set_ShowGrid(HideGrid);
    Adapter.Set_ShowGrid(HideGrid);
    TestFalse(TEXT("grid change remains adapter-owned"), Adapter.Get_ShowGrid());
    TestEqual(TEXT("a no-op grid write does not persist twice"), GridChanges, 1);

    auto RenderModeChanges = 0;
    Adapter.Set_OnRenderModeChanged([&RenderModeChanges](ECk_Jolt_DebugDraw_RenderMode) { ++RenderModeChanges; });
    TestEqual(TEXT("Jolt solid maps to Common none"), Adapter.Get_RenderMode(), ECkDebug3dRenderMode::None);
    Adapter.Set_RenderMode(ECkDebug3dRenderMode::TransparentOnly);
    TestEqual(TEXT("Common transparent-only maps to sensor wireframe"), Target->Get_RenderMode(),
              ECk_Jolt_DebugDraw_RenderMode::SensorWireframe);
    Adapter.Set_RenderMode(ECkDebug3dRenderMode::All);
    TestEqual(TEXT("Common all maps to full wireframe"), Target->Get_RenderMode(),
              ECk_Jolt_DebugDraw_RenderMode::Wireframe);
    Adapter.Set_RenderMode(ECkDebug3dRenderMode::None);
    TestEqual(TEXT("Common none maps back to solid"), Target->Get_RenderMode(), ECk_Jolt_DebugDraw_RenderMode::Solid);
    Adapter.Set_RenderMode(ECkDebug3dRenderMode::None);
    TestEqual(TEXT("render changes persist once per transition"), RenderModeChanges, 3);

    Target->Set_DrawFlags(ECk_Jolt_DebugDrawFlags::Shape | ECk_Jolt_DebugDrawFlags::Velocity);
    auto LabelChanges = 0;
    Adapter.Set_OnLabelsChanged([&LabelChanges](bool) { ++LabelChanges; });
    constexpr auto ShowLabels = true;
    constexpr auto HideLabels = false;
    Adapter.Set_ShowLabels(ShowLabels);
    TestTrue(TEXT("labels toggle on"), Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Labels));
    TestTrue(TEXT("labels preserve shape"), Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Shape));
    TestTrue(TEXT("labels preserve velocity"), Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Velocity));
    Adapter.Set_ShowLabels(ShowLabels);
    Adapter.Set_ShowLabels(HideLabels);
    TestFalse(TEXT("labels toggle off"), Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Labels));
    TestEqual(TEXT("no-op label writes do not persist twice"), LabelChanges, 2);

    auto GlyphChanges = 0;
    Adapter.Set_OnDirectionGlyphScaleChanged([&GlyphChanges](float) { ++GlyphChanges; });
    Adapter.Set_DirectionGlyphScale(2.5f);
    Adapter.Set_DirectionGlyphScale(2.5f);
    TestEqual(TEXT("glyph scale maps to Jolt"), Target->Get_DirectionGlyphScale(), 2.5f);
    TestEqual(TEXT("no-op glyph write does not persist twice"), GlyphChanges, 1);

    constexpr auto HighKeyA = uint64{0xfedcba9876543210};
    constexpr auto HighKeyB = uint64{0x8123456789abcdef};
    auto IsolateChanges = 0;
    Adapter.Set_OnIsolatedKeysChanged([&IsolateChanges](const TArray<uint64>&) { ++IsolateChanges; });
    Adapter.Set_IsolatedKeys({HighKeyA, HighKeyB});
    TestTrue(TEXT("isolate preserves first full-width identity"), Target->Get_IsolatedBodies().Contains(HighKeyA));
    TestTrue(TEXT("isolate preserves second full-width identity"), Target->Get_IsolatedBodies().Contains(HighKeyB));
    Adapter.Set_IsolatedKeys({HighKeyA, HighKeyB});
    Adapter.Set_IsolatedKeys({});
    TestTrue(TEXT("empty isolate clears Jolt"), Target->Get_IsolatedBodies().IsEmpty());
    TestEqual(TEXT("isolate callback fires only on changes"), IsolateChanges, 2);

    auto PickCallbackCount = 0;
    auto PickWasAdditive = false;
    auto PickHadKey = true;
    Adapter.Set_OnPick(
        [&](TOptional<uint64> InKey, bool InIsAdditive)
        {
            ++PickCallbackCount;
            PickWasAdditive = InIsAdditive;
            PickHadKey = InKey.IsSet();
        });
    Adapter.On_Pick(FCkDebug3dCursorRay{FVector::ZeroVector, FVector::ForwardVector, true});
    TestEqual(TEXT("empty-space pick still reports once"), PickCallbackCount, 1);
    TestTrue(TEXT("additive selection intent survives the Common ray"), PickWasAdditive);
    TestFalse(TEXT("empty-space pick reports no key"), PickHadKey);

    Adapter.On_ViewportTeardown();
    Adapter.On_Pick(FCkDebug3dCursorRay{});
    TestEqual(TEXT("teardown clears callback and weak target"), PickCallbackCount, 1);
    Target.Reset();
    constexpr auto InformEngineOfWorldDestruction = false;
    World->DestroyWorld(InformEngineOfWorldDestruction);
    return true;
}
