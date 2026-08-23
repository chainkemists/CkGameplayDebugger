#include "CkDebuggerCommon/Behavior/CkDebug_BehaviorOverrideRegistry.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugBehaviorOverrideRegistry_StaleGenerationCannotRemoveReplacement,
    "Ck.DebuggerCommon.BehaviorOverrides.StaleGenerationCannotRemoveReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugBehaviorOverrideRegistry_StaleGenerationCannotRemoveReplacement::RunTest(const FString& Parameters)
{
    constexpr auto OverrideId = TEXT("Test.Common.GenerationSafeOverride");
    auto& Registry = FCkDebug_BehaviorOverrideRegistry::Get();
    auto ActiveValue = false;

    const auto MakeDescriptor = [&ActiveValue, OverrideId]()
    {
        return FCkDebug_BehaviorOverrideDescriptor{
            TEXT("CkDebuggerCommon"),
            OverrideId,
            FText::FromString(TEXT("Test override")),
            FText::FromString(TEXT("Exercises generation-safe replacement.")),
            10,
            FCkDebug_QueryBehaviorOverride::CreateLambda([&ActiveValue]()
            { return FCkDebug_BehaviorOverrideState{true, ActiveValue, FText::GetEmpty()}; }),
            FCkDebug_SetBehaviorOverride::CreateLambda([&ActiveValue](bool InActive)
            {
                ActiveValue = InActive;
                return FCkDebug_BehaviorOverrideSetResult{true, FText::GetEmpty()};
            })};
    };

    const auto FirstGeneration = Registry.Register(MakeDescriptor());
    const auto SecondGeneration = Registry.Register(MakeDescriptor());
    TestNotEqual(TEXT("Replacement receives a new generation"), FirstGeneration, SecondGeneration);

    Registry.Unregister(OverrideId, FirstGeneration);
    const auto Replacement = Registry.Find(OverrideId);
    TestTrue(TEXT("Stale unregister leaves replacement intact"), Replacement.IsSet());
    if (Replacement.IsSet())
    {
        TestTrue(TEXT("Replacement setter succeeds"), Replacement->Set(true).Succeeded);
        TestTrue(TEXT("Replacement query observes the active state"), Replacement->Query().IsActive);
    }

    Registry.Unregister(OverrideId, SecondGeneration);
    TestFalse(TEXT("Matching generation unregisters replacement"), Registry.Find(OverrideId).IsSet());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
