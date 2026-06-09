#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"

// --------------------------------------------------------------------------------------------------------------------
// Test-only tags — natively registered so they resolve without erroring.
// UE_DEFINE_GAMEPLAY_TAG_STATIC registers at DLL load (before the test runner fires).
// Note: TestTag_Goap / TestTag_GoalLeaf are defined in CkDebugOverlay_Tags.spec.cpp;
// we define them here with distinct local names to keep this TU self-contained.

UE_DEFINE_GAMEPLAY_TAG_STATIC(Resolve_TestTag_Goap, "Ck.OnScreenDebugger.Provider.GOAP");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Resolve_TestTag_Goal, "Ck.OnScreenDebugger.Provider.GOAP.Goal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Resolve_TestTag_Cost, "Ck.OnScreenDebugger.Provider.GOAP.Cost");

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_Resolve_Test,
    "Ck.DebugOverlay.Resolve.EnabledFieldsAndStyle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_Resolve_Test::RunTest(const FString&)
{
    const FGameplayTag Prov = Resolve_TestTag_Goap;
    const FGameplayTag Goal = Resolve_TestTag_Goal;
    const FGameplayTag Cost = Resolve_TestTag_Cost;

    TArray<FCk_DebugOverlay_FieldDesc> Fields { {Goal, true}, {Cost, false} };

    // --------------------------------------------------
    // Case 1: provider in EnabledProviders (no Entry).
    // Expected: Goal on (DefaultEnabled=true), Cost off (DefaultEnabled=false).
    {
        FCk_DebugOverlay_Layout L;
        L.EnabledProviders.AddTag(Prov);
        const auto En = ck_debugoverlay::Resolve_EnabledFields(L, Prov, Fields);
        TestTrue (TEXT("goal on"),  En.HasTagExact(Goal));
        TestFalse(TEXT("cost off"), En.HasTagExact(Cost));
    }

    // --------------------------------------------------
    // Case 2: Entry with explicit EnabledFields = {Cost} (no EnabledProviders entry).
    // Expected: Cost on (explicitly listed), Goal off (not in explicit list).
    {
        FCk_DebugOverlay_Layout L2;
        FCk_DebugOverlay_ProviderEntry E;
        E.ProviderTag = Prov;
        E.EnabledFields.AddTag(Cost);
        L2.Entries.Add(E);
        const auto En2 = ck_debugoverlay::Resolve_EnabledFields(L2, Prov, Fields);
        TestTrue (TEXT("cost on"),  En2.HasTagExact(Cost));
        TestFalse(TEXT("goal off"), En2.HasTagExact(Goal));
    }

    // --------------------------------------------------
    // Case 3: style cascade — Layout default Ultra, Entry overrides to Compact.
    {
        FCk_DebugOverlay_Layout L3;
        L3.DefaultStyle.Density = ECk_DebugOverlay_Density::Ultra;
        FCk_DebugOverlay_ProviderEntry E3;
        E3.ProviderTag      = Prov;
        E3.bOverrideStyle   = true;
        E3.StyleOverride.Density = ECk_DebugOverlay_Density::Compact;
        L3.Entries.Add(E3);
        TestEqual(TEXT("style override"),
            (int)ck_debugoverlay::Resolve_Style(L3, Prov).Density,
            (int)ECk_DebugOverlay_Density::Compact);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
