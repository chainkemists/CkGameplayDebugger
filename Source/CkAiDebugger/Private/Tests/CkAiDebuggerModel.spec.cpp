#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"
#include "CkAiDebugger_Module.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAiDebuggerModel_AiSectionRecognition,
    "Ck.AiDebugger.Model.AiSectionRecognition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerModel_AiSectionRecognition::RunTest(const FString&)
{
    FCk_DebugOverlay_EntityModel Model;
    FCk_DebugOverlay_Section StateMachine;
    StateMachine.ProviderTag = FGameplayTag::RequestGameplayTag(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), false);
    Model.Sections.Add(StateMachine);
    TestTrue(TEXT("An empty-but-valid State Machine source remains AI-relevant"), SCkAiDebuggerWindow::Is_AiModel(Model));

    Model.Sections.Reset();
    FCk_DebugOverlay_Section Transform;
    Transform.ProviderTag = FGameplayTag::RequestGameplayTag(TEXT("Ck.OnScreenDebugger.Provider.Transform"), false);
    Transform.Rows.Add(FCk_DebugOverlay_Row{});
    Model.Sections.Add(Transform);
    TestFalse(TEXT("Transform-only data does not make a model AI-relevant"), SCkAiDebuggerWindow::Is_AiModel(Model));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAiDebuggerLifecycle_OpenCloseReleasesTab,
    "Ck.AiDebugger.Lifecycle.OpenCloseReleasesTab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerLifecycle_OpenCloseReleasesTab::RunTest(const FString&)
{
    auto& Module = FCkAiDebuggerModule::Get();

    // Construct through the same Common factory used for dock-into-existing-window. Invoking the global nomad tab
    // manager in a -nullrhi commandlet asks FGenericWindow for restored OS dimensions and is intentionally fatal;
    // the factory exercises the real AI Slate tree without manufacturing a top-level native window.
    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    const auto* Tool = Tools.FindByPredicate([](const FCkDebuggerToolDescriptor& InTool)
    { return InTool.Get_TabId() == FCkAiDebuggerModule::Get_TabName(); });
    TestNotNull(TEXT("AI Overview is registered in the Common launcher catalog"), Tool);
    if (NOT Tool)
    { return false; }

    TestTrue(TEXT("AI Overview exposes a Common tab factory"), Tool->Get_TabFactory().IsBound());
    if (NOT Tool->Get_TabFactory().IsBound())
    { return false; }

    auto SpawnedTab = TSharedPtr<SDockTab>{Tool->Get_TabFactory().Execute()};

    TestTrue(TEXT("AI Overview constructs its real Slate window"), Module.Get_DebuggerWindow().IsValid());
    TestTrue(TEXT("AI Overview tracks its live dock tab"), Module.IsDebuggerOpen());

    Module.CloseDebugger();
    TestFalse(TEXT("AI Overview releases its Slate window on close"), Module.Get_DebuggerWindow().IsValid());
    TestFalse(TEXT("AI Overview releases its dock tab on close"), Module.IsDebuggerOpen());
    TestTrue(TEXT("caller may still release its detached tab reference safely"), SpawnedTab.IsValid());
    return true;
}
#endif
