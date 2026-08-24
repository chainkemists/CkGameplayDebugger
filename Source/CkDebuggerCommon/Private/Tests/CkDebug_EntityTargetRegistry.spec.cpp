#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugEntityTargetRegistry_RegisterReplaceAndStaleUnregister,
    "Ck.DebuggerCommon.EntityTarget.RegisterReplaceAndStaleUnregister",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugEntityTargetRegistry_InvalidEntityRejectsWithoutCallbacks,
    "Ck.DebuggerCommon.EntityTarget.InvalidEntityRejectsWithoutCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugSelectionSync_ResolveConceptualTarget,
    "Ck.DebuggerCommon.SelectionSync.ResolveConceptualTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_entity_target_registry_spec
{
auto MakeRoute(FName InOwner, FName InTab, int32& InCanTargetCalls, int32& InOpenCalls)
    -> FCkDebug_EntityTargetRoute
{
    return FCkDebug_EntityTargetRoute{
        InOwner,
        InTab,
        [&InCanTargetCalls](const FCk_Handle&)
        {
            ++InCanTargetCalls;
            return true;
        },
        [&InOpenCalls](const FCk_Handle&)
        {
            ++InOpenCalls;
        }};
}
} // namespace ck_debug_entity_target_registry_spec

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebugEntityTargetRegistry_RegisterReplaceAndStaleUnregister::RunTest(const FString&)
{
    using namespace ck_debug_entity_target_registry_spec;

    auto Registry = FCkDebug_EntityTargetRegistry{};
    auto CanTargetCalls = 0;
    auto OpenCalls = 0;

    const auto FirstId = Registry.Register(MakeRoute(TEXT("TestOwner"), TEXT("TestTab"), CanTargetCalls, OpenCalls));
    TestNotEqual(TEXT("first registration succeeds"), FirstId, uint64{0});
    TestTrue(TEXT("route present"), Registry.Has_Route(TEXT("TestTab")));

    const auto ReplacementId = Registry.Register(
        MakeRoute(TEXT("TestOwner"), TEXT("TestTab"), CanTargetCalls, OpenCalls));
    TestNotEqual(TEXT("replacement registration succeeds"), ReplacementId, uint64{0});
    TestNotEqual(TEXT("replacement receives new generation"), ReplacementId, FirstId);

    Registry.Unregister(TEXT("TestTab"), FirstId);
    TestTrue(TEXT("stale unregister cannot remove replacement"), Registry.Has_Route(TEXT("TestTab")));

    Registry.Unregister(TEXT("TestTab"), ReplacementId);
    TestFalse(TEXT("matching unregister removes route"), Registry.Has_Route(TEXT("TestTab")));
    return true;
}

bool FCkDebugEntityTargetRegistry_InvalidEntityRejectsWithoutCallbacks::RunTest(const FString&)
{
    using namespace ck_debug_entity_target_registry_spec;

    auto Registry = FCkDebug_EntityTargetRegistry{};
    auto CanTargetCalls = 0;
    auto OpenCalls = 0;
    Registry.Register(MakeRoute(TEXT("TestOwner"), TEXT("TestTab"), CanTargetCalls, OpenCalls));

    const auto Invalid = FCk_Handle{};
    TestFalse(TEXT("invalid cannot target"), Registry.CanTarget(TEXT("TestTab"), Invalid));
    TestFalse(TEXT("invalid cannot open"), Registry.TryOpenAndTarget(TEXT("TestTab"), Invalid));
    TestEqual(TEXT("CanTarget callback not invoked"), CanTargetCalls, 0);
    TestEqual(TEXT("Open callback not invoked"), OpenCalls, 0);
    TestTrue(TEXT("targetable list empty"), Registry.Get_TargetableTabs(Invalid).IsEmpty());

    auto ResolverCalls = 0;
    const auto Resolved = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(
        Invalid,
        [&ResolverCalls](const FCk_Handle&)
        {
            ++ResolverCalls;
            return true;
        });
    TestFalse(TEXT("invalid lineage selection cannot resolve"), ck::IsValid(Resolved));
    TestEqual(TEXT("lineage predicate not invoked for invalid selection"), ResolverCalls, 0);
    return true;
}

bool FCkDebugSelectionSync_ResolveConceptualTarget::RunTest(const FString&)
{
    using namespace ck::registry_table;

    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    ON_SCOPE_EXIT { Free(RegistrySlot); };
    auto RegistryView = FCk_Registry{RegistrySlot};

    const auto TransientId = FCk_Entity{Registry.create()};
    RegistryView.SetContext<ck::FCtx_TransientEntity>(ck::FCtx_TransientEntity{TransientId});
    const auto Transient = FCk_Handle{TransientId, RegistrySlot};

    const auto MakeChild = [&Registry, RegistrySlot, &Transient]()
    {
        auto Child = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
        Child.Add<ck::FFragment_LifetimeOwner>(Transient);
        return Child;
    };

    const auto DirectSiblingA = MakeChild();
    const auto DirectSiblingB = MakeChild();
    TestTrue(TEXT("first direct transient child resolves to itself"),
        ck::DebugSelectionSync::Resolve_ConceptualTarget(DirectSiblingA) == DirectSiblingA);
    TestTrue(TEXT("second direct transient child resolves to itself"),
        ck::DebugSelectionSync::Resolve_ConceptualTarget(DirectSiblingB) == DirectSiblingB);
    TestTrue(TEXT("direct transient siblings remain distinct"),
        ck::DebugSelectionSync::Resolve_ConceptualTarget(DirectSiblingA)
        != ck::DebugSelectionSync::Resolve_ConceptualTarget(DirectSiblingB));

    const auto Npc = MakeChild();
    auto CrowdChild = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    CrowdChild.Add<ck::FFragment_LifetimeOwner>(Npc);
    TestTrue(TEXT("child below an NPC resolves to that NPC"),
        ck::DebugSelectionSync::Resolve_ConceptualTarget(CrowdChild) == Npc);

    TestFalse(TEXT("invalid input resolves quietly to invalid"),
        ck::IsValid(ck::DebugSelectionSync::Resolve_ConceptualTarget(FCk_Handle{})));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
