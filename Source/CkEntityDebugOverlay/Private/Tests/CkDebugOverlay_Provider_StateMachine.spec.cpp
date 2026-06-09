// CkDebugOverlay_Provider_StateMachine — provider unit test.
//
// -------------------------------------------------------------------------
// SCAFFOLD STATUS: incomplete entity setup — marked BATCH-VERIFY below.
//
// A robust test needs:
//   1. A live ECS world (requires PIE or a minimal in-process UWorld).
//   2. An entity with FFragment_Sm_Current spawned and a current state set.
//   3. Optionally: FFragment_Sm_Debug with history entries.
//
// CkStateMachine net tests (CkStateMachine_Net_*) use ACk_AutoTest_NetSubject
// with PIE multi-client setup (heavy). That harness is not usable for a
// simple unit test of Collect().
//
// The synchronous entity + SM creation path (if it exists) would look like:
//   - Get or create a FCk_Registry / world context.
//   - Spawn an entity via ck::EntityLifetime.
//   - Call UCk_Utils_StateMachine_UE::Add(entity, params) to attach an SM.
//   - Drive the SM into a known initial state.
//   - Tick the registry one frame so fragments settle.
//   - Call Collect() and assert.
//
// BATCH-VERIFY: investigate whether CkStateMachine exposes a synchronous
// FCk_Handle + Add-SM utility that works without a full PIE world (i.e. a
// unit-test-friendly factory). If found, complete the entity setup below and
// remove the #if 0 guard. Reference: CkStateMachine_Utils.h.
//
// For now the test is a structural skeleton that verifies the provider's
// compile-time shape (Get_ProviderTag / Get_FieldTags / CanProvide on an
// invalid handle) without requiring a live world.
// -------------------------------------------------------------------------

#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEcs/Handle/CkHandle.h"

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
// BATCH-VERIFY: Complete entity-construction test below.
//
// When a synchronous SM construction path is available, implement the
// following test. The outline is intentionally commented out rather than
// left as dead #if 0 so the batch-verifier has a clear roadmap.
//
// Steps needed:
//   1. Obtain a UWorld* (minimal or PIE). If PIE is required, wrap in a
//      PIE latent test using FCk_Latent_* helpers (see CkStateMachine net tests).
//   2. Create an FCk_Handle via ck::EntityLifetime::Spawn or equivalent.
//   3. Call UCk_Utils_StateMachine_UE::Add(entity, FStateMachineParams{...})
//      to attach a state machine with a known initial state class.
//   4. Tick the registry so the SM enters the initial state (FFragment_Sm_Current
//      gets Get_CurrentStateClass() != null).
//   5. Build FCk_DebugOverlay_ProviderConfig with EnabledFields containing
//      TAG_Ck_OnScreenDebugger_Provider_StateMachine_State.
//   6. Call Provider.Collect(entity, Cfg, Section).
//   7. TestTrue(Section.Rows.Num() >= 1).
//   8. TestEqual(Section.Rows[0].FieldTag, TAG_..._State).
//   9. TestFalse(Section.Rows[0].Value.IsEmpty()) — should contain the state class name.
//
// Expected assertion:
//   Section.Rows[0].Value contains the leaf name of the initial state class,
//   matching UCk_Utils_StateMachine_UE::Get_CurrentStateClass(smHandle)->GetName().
// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
