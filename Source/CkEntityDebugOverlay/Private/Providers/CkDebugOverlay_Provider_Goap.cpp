#include "CkDebugOverlay_Provider_Goap.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// GOAP planner utils — lives in CkGoap module (already a dep in Build.cs)
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment_Data.h"
#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// NOTE: We deliberately do NOT include CkGoapDebugger_DataCollector.h.
// The DataCollector lives in the separate CkGoapDebugger module (editor-only,
// heavy Slate deps) which is NOT a dependency of CkEntityDebugOverlay.
// Instead we call UCk_Utils_Goap_Planner_UE utilities directly.
// BATCH-VERIFY: confirm CkGoap is in the module's public deps (it is per Build.cs).
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap,
    "Ck.OnScreenDebugger.Provider.Goap")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_Goal,
    "Ck.OnScreenDebugger.Provider.Goap.Status")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_Action,
    "Ck.OnScreenDebugger.Provider.Goap.Active")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_Plan,
    "Ck.OnScreenDebugger.Provider.Goap.Plan")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_Cost,
    "Ck.OnScreenDebugger.Provider.Goap.Cost")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_LegacyGoal,
    "Ck.OnScreenDebugger.Provider.Goap.Goal")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Goap_LegacyAction,
    "Ck.OnScreenDebugger.Provider.Goap.Action")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()      { return TAG_Ck_OnScreenDebugger_Provider_Goap; }
    FGameplayTag FieldTag_Status()  { return TAG_Ck_OnScreenDebugger_Provider_Goap_Goal; }
    FGameplayTag FieldTag_Active()  { return TAG_Ck_OnScreenDebugger_Provider_Goap_Action; }
    FGameplayTag FieldTag_Plan()    { return TAG_Ck_OnScreenDebugger_Provider_Goap_Plan; }
    FGameplayTag FieldTag_Cost()    { return TAG_Ck_OnScreenDebugger_Provider_Goap_Cost; }
    FGameplayTag FieldTag_LegacyGoal() { return TAG_Ck_OnScreenDebugger_Provider_Goap_LegacyGoal; }
    FGameplayTag FieldTag_LegacyAction() { return TAG_Ck_OnScreenDebugger_Provider_Goap_LegacyAction; }

    // Convert ECk_GoapPlanStatus to a short label string.
    auto LabelForStatus(ECk_GoapPlanStatus InStatus) -> FString
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:            return TEXT("PlanFound");
        case ECk_GoapPlanStatus::Planning:             return TEXT("Planning");
        case ECk_GoapPlanStatus::PlanFailed:           return TEXT("PlanFailed");
        case ECk_GoapPlanStatus::CostThresholdReached: return TEXT("CostThreshold");
        case ECk_GoapPlanStatus::Idle:                 return TEXT("Idle");
        default:                                       return TEXT("(unknown)");
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Goap::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Goap::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Status(), true  },
        { FieldTag_Active(), true  },
        { FieldTag_Plan(),   true  },
        { FieldTag_Cost(),   true  },   // planner cost at a glance — card is the one-screen NPC debugger
        // Serialized layouts from before the truthful Status/Active rename continue
        // to resolve, but new defaults do not enable these aliases.
        { FieldTag_LegacyGoal(), false }, { FieldTag_LegacyAction(), false },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkGoapInspector_Gateway::CanInspect:
//   return UCk_Utils_Goap_Planner_UE::Has(Entity);
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Goap::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Goap_Planner_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// The gateway inspector delegates rendering to FCkGoapDebugger_DataCollector
// (editor module). We replicate the same data via UCk_Utils_Goap_Planner_UE
// utilities which operate directly on the live ECS.
//
// BATCH-VERIFY:
//   UCk_Utils_Goap_Planner_UE::Has(Entity) — bool, confirmed.
//   UCk_Utils_Goap_Planner_UE::Get_ActiveChain(PlannerHandle) — returns TArray<FCk_Handle_Goap_Action>.
//   UCk_Utils_Goap_Planner_UE::Get_PlanStatus(PlannerHandle)  — returns ECk_GoapPlanStatus.
//   UCk_Utils_Goap_Planner_UE::Get_PlanCost(PlannerHandle)    — returns float.
//   UCk_Utils_Goap_Planner_UE::Get_PlanClasses(PlannerHandle) — returns TArray<TSubclassOf<>>.
//   UCk_Utils_Goap_Planner_UE::CastChecked(Entity)            — BATCH-VERIFY: CK_DEFINE_CPP_CASTCHECKED_TYPESAFE
//     defines Get_Handle_TypesafeFrom() or CastChecked(). Check actual API on UCk_Utils_Goap_Planner_UE.
//
// Planner cast: FCk_Handle_Goap_Planner is a FCk_Handle_TypeSafe subtype.
// We cast via UCk_Utils_Goap_Planner_UE::CastChecked(generic_handle) which
// the CK_DEFINE_CPP_CASTCHECKED_TYPESAFE macro generates.
// BATCH-VERIFY: the exact cast function name (may be Cast or CastChecked).
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Goap::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    // Cast the generic handle to a typed Planner handle.
    // BATCH-VERIFY: confirm correct cast API name. CK_DEFINE_CPP_CASTCHECKED_TYPESAFE
    // likely generates UCk_Utils_Goap_Planner_UE::CastChecked(const FCk_Handle&).
    FCk_Handle_Goap_Planner PlannerHandle = UCk_Utils_Goap_Planner_UE::CastChecked(Entity);
    if (NOT ck::IsValid(static_cast<FCk_Handle>(PlannerHandle))) { return; }

    const ECk_GoapPlanStatus PlanStatus = UCk_Utils_Goap_Planner_UE::Get_PlanStatus(PlannerHandle);

    // Severity mapping: PlanFound = Good, Planning = Warn, PlanFailed/Idle = Normal/Bad.
    auto StatusSeverity = [](ECk_GoapPlanStatus Status) -> ECk_DebugOverlay_Severity
    {
        switch (Status)
        {
        case ECk_GoapPlanStatus::PlanFound:  return ECk_DebugOverlay_Severity::Good;
        case ECk_GoapPlanStatus::Planning:   return ECk_DebugOverlay_Severity::Warn;
        case ECk_GoapPlanStatus::PlanFailed: return ECk_DebugOverlay_Severity::Bad;
        default:                             return ECk_DebugOverlay_Severity::Normal;
        }
    };

    // Status is deliberately labelled as status: do not present it as a goal.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Status()) || Cfg.EnabledFields.HasTagExact(FieldTag_LegacyGoal()))
    {
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Status();
        Row.Value    = FText::FromString(LabelForStatus(PlanStatus));
        Row.Severity = StatusSeverity(PlanStatus);
        Out.Rows.Add(MoveTemp(Row));
    }

    // Active execution is the runtime-safe signal available without the editor debugger.
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Active()) || Cfg.EnabledFields.HasTagExact(FieldTag_LegacyAction()))
    {
        const TArray<FCk_Handle_Goap_Action> Chain = UCk_Utils_Goap_Planner_UE::Get_ActiveChain(PlannerHandle);
        auto ActionName = FString{TEXT("(none)")};
        if (Chain.Num() > 0)
        {
            const auto& Leaf = Chain.Last();
            if (ck::IsValid(Leaf) && Leaf.Has<FCk_Fragment_Goap_ActionParamsData>())
            {
                const auto ActionClass = Leaf.Get<FCk_Fragment_Goap_ActionParamsData>().Get_ActionClass();
                ActionName = ck::IsValid(ActionClass) ? ActionClass->GetName() : FString{TEXT("(unnamed action)")};
            }
            else
            {
                ActionName = TEXT("(invalid active action)");
            }

            if (Chain.Num() > 1)
            { ActionName += ck::Format_UE(TEXT(" (depth {})"), Chain.Num()); }
        }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Active();
        Row.Value    = FText::FromString(ActionName);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- Plan ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Plan()))
    {
        // Get_PlanClasses returns TArray<TSubclassOf<UCk_GoapAction_EntityScript>>.
        const TArray<TSubclassOf<UCk_GoapAction_EntityScript>> PlanClasses =
            UCk_Utils_Goap_Planner_UE::Get_PlanClasses(PlannerHandle);

        FString PlanStr;
        const auto NumToShow = FMath::Min(3, PlanClasses.Num());
        for (auto i = 0; i < NumToShow; ++i)
        {
            if (i > 0) { PlanStr.Append(TEXT(" > ")); }
            PlanStr.Append(PlanClasses[i] ? PlanClasses[i]->GetName() : FString(TEXT("?")));
        }
        if (PlanClasses.Num() > NumToShow)
        {
            PlanStr.Append(FString::Printf(TEXT(" +%d"), PlanClasses.Num() - NumToShow));
        }
        if (PlanStr.IsEmpty()) { PlanStr = TEXT("(no plan)"); }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Plan();
        Row.Value    = FText::FromString(PlanStr);
        Row.Severity = PlanClasses.Num() > 0
                        ? ECk_DebugOverlay_Severity::Good
                        : ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- Cost ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_Cost()))
    {
        const float Cost = UCk_Utils_Goap_Planner_UE::Get_PlanCost(PlannerHandle);
        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Cost();
        Row.Value    = FText::FromString(FString::Printf(TEXT("%.0f"), Cost));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Goap::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Goap_Planner_UE::Has(Entity)) { return {}; }
    FCk_Handle_Goap_Planner PlannerHandle = UCk_Utils_Goap_Planner_UE::CastChecked(Entity);
    const ECk_GoapPlanStatus Status = UCk_Utils_Goap_Planner_UE::Get_PlanStatus(PlannerHandle);
    return FString::Printf(TEXT("GOAP:%s"), *LabelForStatus(Status));
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Goap)

// --------------------------------------------------------------------------------------------------------------------
