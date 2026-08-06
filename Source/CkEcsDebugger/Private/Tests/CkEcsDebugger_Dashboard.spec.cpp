#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Pages/CkDebuggerPage_Dashboard.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerDashboard_ContentRebuildInvalidatesPresentation,
    "Ck.EcsDebugger.Dashboard.ContentRebuildInvalidatesPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerDashboard_ContentRebuildInvalidatesPresentation::RunTest(const FString&)
{
    auto Dashboard = FCkDebuggerPage_Dashboard{};

    Dashboard.FamilyCache.Add(TEXT("Internals"));
    Dashboard.PresentedFamilies.Add(TEXT("Internals"));
    Dashboard.CardCache.Add(TEXT("SceneNode"));
    Dashboard.PresentedCardKeys.Add(TEXT("SceneNode"));
    Dashboard.PresentedSingletonKeys.Add(TEXT("Actor"));

    Dashboard.Build_Content(FCkDebuggerPageContext{});

    TestTrue(TEXT("Content rebuild creates a new family container"), Dashboard.FamilyBox.IsValid());
    TestTrue(TEXT("Content rebuild creates a new card container"), Dashboard.CardsGrid.IsValid());
    TestTrue(TEXT("Content rebuild creates a new singleton container"), Dashboard.SingletonsBox.IsValid());
    TestTrue(TEXT("Content rebuild drops family widgets owned by the previous tree"), Dashboard.FamilyCache.IsEmpty());
    TestTrue(TEXT("Content rebuild invalidates the presented family keys"), Dashboard.PresentedFamilies.IsEmpty());
    TestTrue(TEXT("Content rebuild drops card widgets owned by the previous tree"), Dashboard.CardCache.IsEmpty());
    TestTrue(TEXT("Content rebuild invalidates the presented card keys"), Dashboard.PresentedCardKeys.IsEmpty());
    TestTrue(TEXT("Content rebuild invalidates the presented singleton keys"), Dashboard.PresentedSingletonKeys.IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
