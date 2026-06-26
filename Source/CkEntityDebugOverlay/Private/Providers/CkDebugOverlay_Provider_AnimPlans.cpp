#include "CkDebugOverlay_Provider_AnimPlans.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// AnimPlan utils — mirroring CkInspector_AnimPlans.cpp includes
#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_AnimPlans,
    "Ck.OnScreenDebugger.Provider.AnimPlans")

// A single "catch-all" field tag for the collection.
// Each anim plan row shares this tag; the goal tag name is embedded in the value string.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_AnimPlans_Value,
    "Ck.OnScreenDebugger.Provider.AnimPlans.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_AnimPlans; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_AnimPlans_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_AnimPlans::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_AnimPlans::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_AnimPlans::CanInspect:
//   ck::IsValid(Entity) && UCk_Utils_AnimPlan_UE::Has_Any(Entity)
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_AnimPlans::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_AnimPlan_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// FieldTag approach: all rows share FieldTag_Value().
// The goal tag name is placed in the value string so the key chip renders
// the plan label naturally — same rationale as FloatAttributes/IntegerAttributes.
//
// Row format: "GoalName | ClusterLeaf | StateLeaf"
// (leaf = last tag node, e.g. "AnimPlan.Goal.Run" → "Run")
//
// BATCH-VERIFY:
//   UCk_Utils_AnimPlan_UE::Has_Any(Entity)                         — confirmed from CkInspector_AnimPlans.cpp
//   UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(FCk_Handle&, TFunc)   — confirmed from CkAnimPlan_Utils.h (non-const ref overload)
//   UCk_Utils_AnimPlan_UE::Get_AnimGoal(plan)   → FCk_AnimPlan_Goal, .Get_AnimGoal() → FGameplayTag  — confirmed
//   UCk_Utils_AnimPlan_UE::Get_AnimCluster(plan) → FCk_AnimPlan_Cluster, .Get_AnimCluster() → FGameplayTag — confirmed
//   UCk_Utils_AnimPlan_UE::Get_AnimState(plan)  → FCk_AnimPlan_State,  .Get_AnimState()  → FGameplayTag  — confirmed
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_AnimPlans::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(MutableEntity,
        [&Out](FCk_Handle_AnimPlan& InAnimPlan)
        {
            const auto Goal        = UCk_Utils_AnimPlan_UE::Get_AnimGoal(InAnimPlan);
            const auto Cluster     = UCk_Utils_AnimPlan_UE::Get_AnimCluster(InAnimPlan);
            const auto State       = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);

            const auto GoalTag    = Goal.Get_AnimGoal();
            const auto ClusterTag = Cluster.Get_AnimCluster();
            const auto StateTag   = State.Get_AnimState();

            const auto GoalName    = GoalTag.IsValid()    ? GoalTag.GetTagName().ToString()    : FString(TEXT("Unknown"));
            const auto ClusterName = ClusterTag.IsValid() ? ClusterTag.GetTagName().ToString() : FString(TEXT("-"));
            const auto StateName   = StateTag.IsValid()   ? StateTag.GetTagName().ToString()   : FString(TEXT("-"));

            const auto DisplayStr = FString::Printf(TEXT("%s | %s | %s"),
                *GoalName, *ClusterName, *StateName);

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_Value();
            Row.Value    = FText::FromString(DisplayStr);
            Row.Severity = ECk_DebugOverlay_Severity::Normal;
            Out.Rows.Add(MoveTemp(Row));
        });
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
//
// Produces "Anim:N" (plan count), optionally appending the state-tag leaf name
// of the first plan when available — e.g. "Anim:2 [Run]".
// Returns {} when the entity has no anim plans.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_AnimPlans::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    int32 Count = 0;
    FString FirstStateName;

    UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(MutableEntity,
        [&Count, &FirstStateName](FCk_Handle_AnimPlan& InAnimPlan)
        {
            if (Count == 0)
            {
                const auto State    = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);
                const auto StateTag = State.Get_AnimState();
                if (StateTag.IsValid())
                {
                    FirstStateName = StateTag.GetTagName().ToString();
                }
            }
            ++Count;
        });

    if (Count == 0) { return {}; }

    if (NOT FirstStateName.IsEmpty())
    {
        return FString::Printf(TEXT("Anim:%d [%s]"), Count, *FirstStateName);
    }
    return FString::Printf(TEXT("Anim:%d"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_AnimPlans)

// --------------------------------------------------------------------------------------------------------------------
