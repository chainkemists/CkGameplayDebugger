#include "../../CkTextureDebugger_Module.h"

#if WITH_EDITOR

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "ModuleDescriptor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Cook_AssetCatalog,
    "Ck.TextureDebugger.Cook.AssetCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Cook_AssetCatalog::RunTest(const FString& Parameters)
{
    auto ExpectedPackages = TSet<FName>{
        TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_2K"),
        TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_4K"),
        TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_GoldGray_4K"),
        TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_RoundedSpectrum_4K"),
        TEXT("/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_DirectionalMono_4K"),
        TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker"),
    };

    const auto& RegisteredPackages = FCkTextureDebuggerModule::Get_CookPackageNames();
    TestEqual(TEXT("Exactly six checker packages are registered for explicit cook"), RegisteredPackages.Num(), ExpectedPackages.Num());
    for (const auto& PackageName : RegisteredPackages)
    {
        TestTrue(*FString::Printf(TEXT("Cook package is expected: %s"), *PackageName.ToString()),
            ExpectedPackages.Remove(PackageName) == 1);
    }
    TestEqual(TEXT("Every expected checker package is registered"), ExpectedPackages.Num(), 0);

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TestTrue(TEXT("CkDebugger plugin is discoverable"), Plugin.IsValid());
    if (NOT Plugin.IsValid())
    {
        return false;
    }

    const auto* Module = Plugin->GetDescriptor().Modules.FindByPredicate([](const FModuleDescriptor& InDescriptor)
        { return InDescriptor.Name == TEXT("CkTextureDebugger"); });
    TestNotNull(TEXT("CkTextureDebugger module descriptor is present"), Module);
    if (Module == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("CkTextureDebugger remains a Development/DebugGame DeveloperTool"), Module->Type, EHostType::DeveloperTool);
    TestTrue(TEXT("Descriptor includes Win64 Game Development with developer tools and cooked data"),
        Module->IsCompiledInConfiguration(
            TEXT("Win64"), EBuildConfiguration::Development, TEXT("CkPlugins"), EBuildTargetType::Game, true, true));
    TestTrue(TEXT("Descriptor excludes Win64 Game Shipping"),
        NOT Module->IsCompiledInConfiguration(
            TEXT("Win64"), EBuildConfiguration::Shipping, TEXT("CkPlugins"), EBuildTargetType::Game, false, true));
    return ExpectedPackages.Num() == 0;
}

#endif
