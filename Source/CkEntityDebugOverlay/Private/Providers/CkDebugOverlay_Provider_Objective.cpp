#include "CkDebugOverlay_Provider_Objective.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// Objective utils — mirroring CkInspector_ObjectiveOwner.cpp includes
#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Objective,
    "Ck.OnScreenDebugger.Provider.Objective")

// A single "catch-all" field tag for the objective rows.
// Each objective row shares this tag; the objective name and status are embedded in the value string.
// This avoids synthesizing dynamic per-objective gameplay tags at runtime.
// Rendering: [OBJECTIVE] [ObjectiveName: Status]
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Objective_Value,
    "Ck.OnScreenDebugger.Provider.Objective.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Objective; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Objective_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Objective::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Objective::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_ObjectiveOwner::CanInspect:
//   UCk_Utils_ObjectiveOwner_UE::Has(Entity)
//
// Decision: ObjectiveOwner is the NPC-level "owns many objectives" Has-check.
// A bare Objective entity (UCk_Utils_Objective_UE::Has) would require CastChecked
// to a typed handle before reading status — handled below. We prefer ObjectiveOwner
// as the primary gate since NPCs are the typical debug target; the compact token
// falls back gracefully to empty on non-owner entities.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Objective::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_ObjectiveOwner_UE::Has(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Reads the ObjectiveOwner's objective list and emits one row per objective:
//   "[ObjectiveName: Status]"
// Up to 3 rows are emitted to keep the overlay compact; a "+N more" row is
// appended when there are additional objectives beyond the first 3.
//
// BATCH-VERIFY:
//   UCk_Utils_ObjectiveOwner_UE::Has(Entity)                                — confirmed CkInspector_ObjectiveOwner.cpp:24
//   UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity)                 — confirmed CkInspector_ObjectiveOwner.cpp:45
//   UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle)             — confirmed CkInspector_ObjectiveOwner.cpp:51
//   UCk_Utils_Objective_UE::Get_Name(ObjectiveHandle) -> FGameplayTag       — confirmed CkInspector_ObjectiveOwner.cpp:68
//   UCk_Utils_Objective_UE::Get_Status(ObjectiveHandle) -> ECk_ObjectiveStatus — confirmed CkInspector_ObjectiveOwner.cpp:77
//   ck::Format_UE(TEXT("{}"), ECk_ObjectiveStatus) valid                    — CK_DEFINE_CUSTOM_FORMATTER_ENUM in CkObjective_Fragment_Data.h:30
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Objective::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    auto MutableEntity = Entity;
    const auto OwnerHandle = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(OwnerHandle)) { return; }

    const auto Objectives = UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle);

    constexpr int32 MaxRows = 3;
    int32 Emitted = 0;

    for (const auto& ObjectiveHandle : Objectives)
    {
        if (ck::Is_NOT_Valid(ObjectiveHandle)) { continue; }
        if (Emitted >= MaxRows) { break; }

        const auto Name   = UCk_Utils_Objective_UE::Get_Name(ObjectiveHandle);
        const auto Status = UCk_Utils_Objective_UE::Get_Status(ObjectiveHandle);

        const auto NameStr = Name.IsValid() ? Name.GetTagName().ToString() : FString(TEXT("Unnamed"));
        const auto DisplayStr = NameStr + TEXT(": ") + ck::Format_UE(TEXT("{}"), Status);

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_Value();
        Row.Value    = FText::FromString(DisplayStr);
        Row.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(Row));
        ++Emitted;
    }

    const int32 Remaining = Objectives.Num() - Emitted;
    if (Remaining > 0)
    {
        FCk_DebugOverlay_Row OverflowRow;
        OverflowRow.FieldTag = FieldTag_Value();
        OverflowRow.Value    = FText::FromString(FString::Printf(TEXT("+%d more"), Remaining));
        OverflowRow.Severity = ECk_DebugOverlay_Severity::Normal;
        Out.Rows.Add(MoveTemp(OverflowRow));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
//
// Shows: "Obj:<count>" (total objectives on the owner).
// If the entity has no ObjectiveOwner, returns {}.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Objective::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    auto MutableEntity = Entity;
    const auto OwnerHandle = UCk_Utils_ObjectiveOwner_UE::CastChecked(MutableEntity);
    if (ck::Is_NOT_Valid(OwnerHandle)) { return {}; }

    const auto Objectives = UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(OwnerHandle);
    if (Objectives.IsEmpty()) { return {}; }

    return FString::Printf(TEXT("Obj:%d"), Objectives.Num());
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Objective)

// --------------------------------------------------------------------------------------------------------------------
