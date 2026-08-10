#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "../Providers/CkDebugOverlay_Provider_Goap.h"
#include "../Providers/CkDebugOverlay_Provider_PathNetworkFollower.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

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

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_ProviderMergeBehavior_Test,
    "Ck.DebugOverlay.Provider.MergeBehaviorCensus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_ProviderMergeBehavior_Test::RunTest(const FString&)
{
    // The policy table from _Design/2026-08-09-provider-merge-policy.md. Anything absent must
    // stay on the MergeWithinSource default — the point of the census is that a NEW provider
    // either matches the conservative default or lands here as a deliberate decision.
    const auto Expected = TMap<FString, ECk_DebugOverlay_MergeBehavior>{
        { TEXT("Label"),        ECk_DebugOverlay_MergeBehavior::MergeAcrossSources },
        { TEXT("Team"),         ECk_DebugOverlay_MergeBehavior::MergeAcrossSources },
        { TEXT("Goap"),         ECk_DebugOverlay_MergeBehavior::CondensePerSource  },
        { TEXT("StateMachine"), ECk_DebugOverlay_MergeBehavior::CondensePerSource  },
    };

    const auto Providers = FCk_DebugOverlay_Registry::Get().CreateAll();
    TestTrue(TEXT("the registry yields providers"), Providers.Num() > 0);

    auto Seen = TSet<FString>{};

    for (const auto& Provider : Providers)
    {
        if (NOT Provider.IsValid())
        { continue; }

        const auto Leaf = ck_debugoverlay::Get_LeafName(Provider->Get_ProviderTag());
        Seen.Add(Leaf);

        const auto* Want     = Expected.Find(Leaf);
        const auto  Behavior = Want != nullptr ? *Want : ECk_DebugOverlay_MergeBehavior::MergeWithinSource;

        TestEqual(*(Leaf + TEXT(" declares the merge behavior the design assigned it")),
            static_cast<int32>(Provider->Get_MergeBehavior()),
            static_cast<int32>(Behavior));
    }

    // A silently-unregistered provider would make its row above vacuously pass.
    for (const auto& Entry : Expected)
    {
        TestTrue(*(Entry.Key + TEXT(" is registered")), Seen.Contains(Entry.Key));
    }

    return true;
}

#endif
