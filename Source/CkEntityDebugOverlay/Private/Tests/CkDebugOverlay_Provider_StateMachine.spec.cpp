// CkDebugOverlay_Provider_StateMachine — provider unit tests.
//
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"
#include "CkStateMachine/State/EntityScripts/CkSmState_EntityScript.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"

// The provider is in Private — include by relative path from the source root.
// BATCH-VERIFY: if the unity build merges Private/.cpp files, the provider
// type may already be visible; if not, the test must be in the same TU or
// the provider must be forward-declared + accessed via the registry.
#include "../Providers/CkDebugOverlay_Provider_StateMachine.h"

// Needed tags for building a ProviderConfig with enabled fields
#include "NativeGameplayTags.h"

// Tag stubs — declared here so HasTagExact resolves in the test without a
// full DefaultGameplayTags.ini registration.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TestTag_SM_Provider,
    "Ck.OnScreenDebugger.Provider.StateMachine")
UE_DEFINE_GAMEPLAY_TAG_STATIC(TestTag_SM_State,
    "Ck.OnScreenDebugger.Provider.StateMachine.State")
UE_DEFINE_GAMEPLAY_TAG_STATIC(TestTag_SM_History,
    "Ck.OnScreenDebugger.Provider.StateMachine.History")

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_Provider_StateMachine_Shape,
    "Ck.DebugOverlay.Provider.StateMachine.Shape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_Provider_StateMachine_Shape::RunTest(const FString&)
{
    FCk_DebugOverlay_Provider_StateMachine Provider;

    // --- Provider tag ---
    const FGameplayTag ProvTag = Provider.Get_ProviderTag();
    TestTrue(TEXT("provider tag is valid"), ProvTag.IsValid());
    TestEqual(TEXT("provider tag name"),
        ProvTag.ToString(),
        FString(TEXT("Ck.OnScreenDebugger.Provider.StateMachine")));

    // --- Field tags ---
    const auto Fields = Provider.Get_FieldTags();
    TestEqual(TEXT("field count"), Fields.Num(), 2);

    const bool bHasState = Fields.ContainsByPredicate(
        [](const FCk_DebugOverlay_FieldDesc& F) { return F.Tag.ToString().Contains(TEXT("State")); });
    const bool bHasHistory = Fields.ContainsByPredicate(
        [](const FCk_DebugOverlay_FieldDesc& F) { return F.Tag.ToString().Contains(TEXT("History")); });
    TestTrue(TEXT("has State field"),   bHasState);
    TestTrue(TEXT("has History field"), bHasHistory);

    // --- Sort priority ---
    TestEqual(TEXT("sort priority"), Provider.Get_SortPriority(), 50);

    // --- CanProvide: invalid handle → false ---
    FCk_Handle NullHandle;
    TestFalse(TEXT("CanProvide(invalid) == false"), Provider.CanProvide(NullHandle));

    // --- CompactToken: invalid handle → empty ---
    FCk_DebugOverlay_ProviderConfig Cfg;
    const FString Token = Provider.Get_CompactToken(NullHandle, Cfg);
    TestTrue(TEXT("CompactToken(invalid) is empty"), Token.IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_Provider_StateMachine_CurrentStateWithoutDebugCache,
    "Ck.DebugOverlay.Provider.StateMachine.CurrentStateWithoutDebugCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_Provider_StateMachine_CurrentStateWithoutDebugCache::RunTest(const FString&)
{
    auto EnttRegistry = ck::registry_table::EnttRegistryType{};
    const auto RegistryHandle = ck::registry_table::Allocate(&EnttRegistry);
    auto Registry = FCk_Registry{RegistryHandle};
    ON_SCOPE_EXIT { ck::registry_table::Free(RegistryHandle); };

    const auto TransientEntityId = FCk_Entity{EnttRegistry.create()};
    Registry.SetContext<ck::FCtx_TransientEntity>(ck::FCtx_TransientEntity{TransientEntityId});

    auto StateMachine = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Registry);
    StateMachine.Add<ck::FFragment_Sm_Current>(
        ECk_SmRunStatus::Running,
        FCk_Handle_SmState{},
        UCk_SmState_EntityScript::StaticClass());

    FCk_DebugOverlay_ProviderConfig Config;
    Config.EnabledFields.AddTag(TestTag_SM_State);

    FCk_DebugOverlay_Section Section;
    FCk_DebugOverlay_Provider_StateMachine Provider;
    TestFalse(TEXT("state machine has no debug cache"), StateMachine.Has<ck::FFragment_Sm_Debug>());
    Provider.Collect(StateMachine, Config, Section);

    TestEqual(TEXT("one current-state row"), Section.Rows.Num(), 1);
    if (Section.Rows.Num() != 1)
    { return false; }

    const auto& Row = Section.Rows[0];
    TestTrue(TEXT("row is State"), Row.FieldTag == TestTag_SM_State);
    TestFalse(TEXT("row has current-state text"), Row.Value.IsEmpty());
    TestEqual(TEXT("running state is good"), Row.Severity, ECk_DebugOverlay_Severity::Good);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
