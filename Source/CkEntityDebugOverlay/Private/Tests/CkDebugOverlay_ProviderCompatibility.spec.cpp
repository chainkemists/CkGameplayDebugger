#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "../Providers/CkDebugOverlay_Provider_Goap.h"
#include "../Providers/CkDebugOverlay_Provider_PathNetworkFollower.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_ProviderCompatibility_Test,
    "Ck.DebugOverlay.Provider.LegacyFieldCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_ProviderCompatibility_Test::RunTest(const FString&)
{
    const FCk_DebugOverlay_Provider_Goap Goap;
    const FCk_DebugOverlay_Provider_PathNetworkFollower Path;
    const auto Has = [](const TArray<FCk_DebugOverlay_FieldDesc>& Fields, const TCHAR* Name)
    {
        return Fields.ContainsByPredicate([Name](const FCk_DebugOverlay_FieldDesc& Field)
        { return Field.Tag.ToString() == Name && !Field.DefaultEnabled; });
    };
    TestTrue(TEXT("GOAP Goal alias remains resolvable but disabled by default"),
        Has(Goap.Get_FieldTags(), TEXT("Ck.OnScreenDebugger.Provider.Goap.Goal")));
    TestTrue(TEXT("GOAP Action alias remains resolvable but disabled by default"),
        Has(Goap.Get_FieldTags(), TEXT("Ck.OnScreenDebugger.Provider.Goap.Action")));
    TestTrue(TEXT("PathNetwork Route alias remains resolvable but disabled by default"),
        Has(Path.Get_FieldTags(), TEXT("Ck.OnScreenDebugger.Provider.PathNetworkFollower.Route")));
    return true;
}

#endif
