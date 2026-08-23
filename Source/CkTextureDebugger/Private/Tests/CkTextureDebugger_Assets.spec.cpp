#include "AssetGeneration/CkTextureDebugger_AssetGeneration.h"

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Assets_Validate,
    "Ck.TextureDebugger.Assets.Validate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Assets_Validate::RunTest(const FString& Parameters)
{
    using namespace ck::texture_debugger::asset_generation;

    const auto Result = Run(EMode::ValidateOnly);
    for (const auto& Error : Result.Errors)
    {
        AddError(Error);
    }
    TestTrue(TEXT("All committed checker textures and the checker material validate"), Result.Succeeded);
    return Result.Succeeded;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Assets_Bootstrap,
    "Ck.TextureDebugger.Assets.Bootstrap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Assets_Bootstrap::RunTest(const FString& Parameters)
{
    using namespace ck::texture_debugger::asset_generation;

    // Asset generation writes tracked binary assets. It is opt-in even when this test is selected,
    // so ordinary focused runs remain validate-only and cannot rewrite a developer's checkout.
    const auto EnvironmentOptIn = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_TEXTURE_DEBUGGER_BOOTSTRAP_ASSETS"));
    if (NOT Parameters.Equals(TEXT("bootstrap"), ESearchCase::IgnoreCase) && EnvironmentOptIn != TEXT("1"))
    {
        AddInfo(TEXT("Bootstrap is opt-in. Pass parameter 'bootstrap' or set CK_TEXTURE_DEBUGGER_BOOTSTRAP_ASSETS=1 only in a checkout with no generated checker assets."));
        return true;
    }

    const auto Result = Run(EMode::Bootstrap);
    for (const auto& Error : Result.Errors)
    {
        AddError(Error);
    }
    TestTrue(TEXT("Bootstrap imports all checker textures and creates the checker material"), Result.Succeeded);
    return Result.Succeeded;
}

#endif
