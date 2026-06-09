#include "CkDebugOverlay_Provider_StateMachine.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// SM fragments — mirroring CkInspector_StateMachine.cpp includes
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// SM field naming decision:
//
// The CkStateMachine inspector is GENERIC — it renders whichever entity is
// selected (an SM entity, a state entity, a task entity, etc.). There are no
// named "Locomotion" / "Combat" slots in the CkStateMachine fragment model.
// Named SMs, if they exist at the application level, are child entities and
// would each be a separate selected entity.
//
// Design choice: use a single `State` field (and `History`) rather than
// fabricating "Locomotion" / "Combat" names that don't exist in the model.
//
// BATCH-VERIFY: confirm with the team whether there is a label/tag on SM
// entities that should be used to produce a "Locomotion" / "Combat" display
// name, or whether two provider instances per named SM slot is the intended
// split. If such a label exists (e.g. CkLabel on the SM entity), the compact
// token can be updated to show label:state rather than SM:state.
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_StateMachine,
    "Ck.OnScreenDebugger.Provider.StateMachine")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_StateMachine_State,
    "Ck.OnScreenDebugger.Provider.StateMachine.State")

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_StateMachine_History,
    "Ck.OnScreenDebugger.Provider.StateMachine.History")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()        { return TAG_Ck_OnScreenDebugger_Provider_StateMachine; }
    FGameplayTag FieldTag_State()     { return TAG_Ck_OnScreenDebugger_Provider_StateMachine_State; }
    FGameplayTag FieldTag_History()   { return TAG_Ck_OnScreenDebugger_Provider_StateMachine_History; }

    // Helper: produce a short class name string from a UClass* (mirrors inspector helper).
    auto Format_Sm_ClassName(const UClass* InClass) -> FString
    {
        return InClass != nullptr ? InClass->GetName() : FString(TEXT("(None)"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_StateMachine::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_StateMachine::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_State(),   true  },
        { FieldTag_History(), false },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_StateMachine::CanInspect.
//
// The inspector accepts ANY SM entity (state machine root, state, task,
// transition, or condition). For the overlay's focused view we restrict to
// the SM root: entities that carry FFragment_Sm_Current (the fragment that
// holds RunStatus + CurrentStateClass). This is the most useful "headline"
// fragment for an NPC state machine entry.
//
// BATCH-VERIFY: if the intended usage is to show the SM overlay on the NPC
// *owner* entity (which spawns SM children), a different CanProvide strategy
// is needed (e.g. check for a parent-of-SM relationship fragment on the owner).
// Currently this matches the inspector's minimal CanInspect condition scoped
// to SM root entities only.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_StateMachine::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return Entity.Has<ck::FFragment_Sm_Current>();
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// BATCH-VERIFY:
//   FFragment_Sm_Current::Get_CurrentStateClass()  — returns const UClass*.
//   FFragment_Sm_Current::Get_RunStatus()          — returns ECk_SmRunStatus.
//   FFragment_Sm_Debug::Get_History()              — returns const TArray<FCk_SmDebug_HistoryEntry>&.
//   FCk_SmDebug_HistoryEntry::ToStateName          — FString field, verified from inspector body.
//   All confirmed present in CkInspector_StateMachine.cpp source read.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_StateMachine::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Entity.Has<ck::FFragment_Sm_Current>()) { return; }

    const auto& SmCurrent = Entity.Get<ck::FFragment_Sm_Current>();
    const UClass* StateClass = SmCurrent.Get_CurrentStateClass();

    // --- State ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_State()))
    {
        const FString StateName = Format_Sm_ClassName(StateClass);

        ECk_DebugOverlay_Severity Sev = ECk_DebugOverlay_Severity::Normal;
        switch (SmCurrent.Get_RunStatus())
        {
            case ECk_SmRunStatus::Running: Sev = ECk_DebugOverlay_Severity::Good;   break;
            case ECk_SmRunStatus::Paused:  Sev = ECk_DebugOverlay_Severity::Warn;   break;
            case ECk_SmRunStatus::Stopped:
            default:                       Sev = ECk_DebugOverlay_Severity::Normal; break;
        }

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_State();
        Row.Value    = FText::FromString(StateName);
        Row.Severity = Sev;
        Out.Rows.Add(MoveTemp(Row));
    }

    // --- History ---
    // Populate ExplicitHistory from FFragment_Sm_Debug::Get_History() ring.
    // The history ring stores FCk_SmDebug_HistoryEntry { FString FromStateName; FString ToStateName; }.
    // We emit each entry as "From → To" text lines.
    //
    // BATCH-VERIFY:
    //   Confirm FCk_SmDebug_HistoryEntry field names FromStateName / ToStateName
    //   match exactly what is in CkStateMachine_Debug_Fragment.h.
    //   The inspector reads them as: Entry.FromStateName / Entry.ToStateName (line 215-216 of inspector).
    if (Cfg.EnabledFields.HasTagExact(FieldTag_History()) && Entity.Has<ck::FFragment_Sm_Debug>())
    {
        const auto& Debug   = Entity.Get<ck::FFragment_Sm_Debug>();
        const auto& History = Debug.Get_History();

        FCk_DebugOverlay_Row Row;
        Row.FieldTag = FieldTag_History();
        Row.Value    = FText::FromString(FString::Printf(TEXT("(%d entries)"), History.Num()));
        Row.Severity = ECk_DebugOverlay_Severity::Normal;

        constexpr auto MaxEntriesToShow = int32{ 8 };
        const auto     HistNum  = History.Num();
        const auto     StartIdx = FMath::Max(0, HistNum - MaxEntriesToShow);

        for (auto i = StartIdx; i < HistNum; ++i)
        {
            const auto& Entry = History[i];
            const auto  From  = Entry.FromStateName.IsEmpty() ? FString(TEXT("(None)")) : Entry.FromStateName;
            const auto  To    = Entry.ToStateName.IsEmpty()   ? FString(TEXT("(None)")) : Entry.ToStateName;
            Row.ExplicitHistory.Add(FText::FromString(FString::Printf(TEXT("%s → %s"), *From, *To)));
        }

        Out.Rows.Add(MoveTemp(Row));
    }
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_StateMachine::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity) || NOT Entity.Has<ck::FFragment_Sm_Current>()) { return {}; }
    const UClass* StateClass = Entity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass();
    const FString StateName  = Format_Sm_ClassName(StateClass);
    return FString::Printf(TEXT("SM:%s"), *StateName);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_StateMachine)

// --------------------------------------------------------------------------------------------------------------------
