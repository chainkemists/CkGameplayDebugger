#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"

// --------------------------------------------------------------------------------------------------------------------
// Test-only tags — natively registered so they resolve without erroring.
// C++ symbol names are unique (Validate_-prefixed) even though the tag string
// "Ck.OnScreenDebugger.Provider.GOAP" is shared with CkDebugOverlay_Resolve.spec.cpp;
// UE's tag manager coalesces duplicate native registrations by string, so the duplicate
// string is safe.

UE_DEFINE_GAMEPLAY_TAG_STATIC(Validate_TestTag_Known,   "Ck.OnScreenDebugger.Provider.GOAP");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Validate_TestTag_Unknown, "Ck.OnScreenDebugger.Provider.DoesNotExist");

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_Validate_Test,
    "Ck.DebugOverlay.Resolve.ValidateLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_Validate_Test::RunTest(const FString&)
{
    FGameplayTagContainer Known; Known.AddTag(Validate_TestTag_Known);
    FCk_DebugOverlay_Layout L; L.EnabledProviders.AddTag(Validate_TestTag_Unknown);
    const auto Problems = ck_debugoverlay::Validate_Layout(L, Known);
    TestEqual(TEXT("one problem"), Problems.Num(), 1);
    TestTrue(TEXT("names offender"), Problems.Num() == 1 && Problems[0].Contains(TEXT("DoesNotExist")));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
