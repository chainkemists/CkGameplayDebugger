#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Window/SCkJoltDebuggerWindow.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"

#include "CkJolt/Constraint/CkJoltConstraint_Fragment.h"

namespace ck_jolt_debugger_window_tests
{
auto CountWidgetType(const TSharedRef<SWidget>& InWidget, const FString& InType) -> int32
{
    auto Count = InWidget->GetTypeAsString() == InType ? 1 : 0;
    const auto* Children = InWidget->GetChildren();
    for (auto Index = 0; Index < Children->Num(); ++Index)
    { Count += CountWidgetType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); }
    return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerWindow_ConstructsWithoutSlotAttributeEnsure,
    "Ck.JoltDebugger.Window.ConstructsWithoutSlotAttributeEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto FCkJoltDebuggerWindow_ConstructsWithoutSlotAttributeEnsure::RunTest(const FString&) -> bool
{
    const auto Window = SNew(SCkJoltDebuggerWindow);
    Window->SlatePrepass();

    TestTrue(TEXT("Jolt debugger window has a non-empty layout"), Window->GetDesiredSize().Y > 0.0f);
    const TSharedPtr<FCkUiView> AuthoredShell = Window->Get_AuthoredShellView();
    TestTrue(TEXT("Jolt debugger mounts its successful authored shell without the native fallback"),
        AuthoredShell.IsValid() && AuthoredShell->GetLastResult().Succeeded && !Window->IsUsingNativeShellFallback());
    TestEqual(TEXT("Jolt authored shell retains two Common hosts in the stats/detail right rail"),
        ck_jolt_debugger_window_tests::CountWidgetType(Window, TEXT("SCkDebug_PaneHost")), 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerWindow_ConstraintIsADebuggerEntity,
    "Ck.JoltDebugger.Window.ConstraintIsADebuggerEntity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/*
 * The FIFTH clause of `Is_JoltDebuggerEntity` (P8-D55, pinned by P8-D74/F10). That one predicate answers for
 * BOTH the module's entity-target route ("Open In → Jolt") and the game-viewport picker's filter, so a
 * constraint the outliner happily lists but the predicate rejects is a row nothing outside this window can
 * reach — and the four body-ish clauses would never notice, because a constraint entity carries none of them.
 */
auto FCkJoltDebuggerWindow_ConstraintIsADebuggerEntity::RunTest(const FString&) -> bool
{
    auto EcsWorld = ck::FEcsWorld{};

    auto Constraint = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());
    auto Stranger   = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(EcsWorld.Get_Registry());

    TestFalse(TEXT("an entity with none of the five fragments is not this window's"),
        SCkJoltDebuggerWindow::Is_JoltDebuggerEntity(Stranger));

    Constraint.Add<ck::FFragment_JoltConstraint>();

    TestTrue(TEXT("a constraint entity IS this window's, so the route and the picker both reach it"),
        SCkJoltDebuggerWindow::Is_JoltDebuggerEntity(Constraint));

    TestFalse(TEXT("and an invalid handle is nobody's"),
        SCkJoltDebuggerWindow::Is_JoltDebuggerEntity(FCk_Handle{}));

    return true;
}

#endif
