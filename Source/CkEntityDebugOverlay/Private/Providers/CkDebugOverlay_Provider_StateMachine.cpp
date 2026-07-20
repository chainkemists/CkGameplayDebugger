#include "CkDebugOverlay_Provider_StateMachine.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

// SM fragments — mirroring CkInspector_StateMachine.cpp includes
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
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

    // The canonical shortener (SCkDebug_NameLabel::Get_ShortName, CkDebuggerCommon)
    // at the overlay's SmStateNameDepth setting — same rule as the SM debugger
    // ("Ck_SmTest_Complex_State_Chase" → depth 1 → "Chase"; depth <= 0 → full).
    auto Format_Sm_StateName(const FString& InClassName) -> FString
    {
        return SCkDebug_NameLabel::Get_ShortName(
            InClassName,
            GetDefault<UCk_DebugOverlay_Settings>()->SmStateNameDepth);
    }

    auto Format_Sm_ClassName(const UClass* InClass) -> FString
    {
        return InClass != nullptr
            ? Format_Sm_StateName(InClass->GetName())
            : FString(TEXT("(None)"));
    }

    // One node of the (possibly nested) state-machine chain: its nesting depth, the
    // current state name at that level, and the run status (severity) of that level.
    struct FSmChainNode
    {
        int32           Depth = 0;
        FString         StateName;
        ECk_SmRunStatus RunStatus = ECk_SmRunStatus::Stopped;
    };

    // Full-recursive descent into active sub-state-machines. From an SM entity, read its
    // current state, then find that state's cached tasks (FFragment_Sm_Debug) and recurse
    // into every task that hosts a live sub-SM (HasSubStateMachine → SubSmHandle).
    // Clamped by InMaxDepth; guarded by a visited-set so a malformed cycle can't recurse
    // forever.
    auto Collect_SmChain(
        const FCk_Handle&     InSm,
        int32                 InDepth,
        int32                 InMaxDepth,
        TSet<uint32>&         InOutVisited,
        TArray<FSmChainNode>& OutChain) -> void
    {
        if (ck::Is_NOT_Valid(InSm) || NOT InSm.Has<ck::FFragment_Sm_Current>())
        { return; }

        const auto EntityNum = static_cast<uint32>(InSm.Get_Entity().Get_EntityNumber());
        if (InOutVisited.Contains(EntityNum))
        { return; }
        InOutVisited.Add(EntityNum);

        const auto& Current    = InSm.Get<ck::FFragment_Sm_Current>();
        // Get_CurrentStateClass() returns a TSubclassOf<UCk_SmState_EntityScript> — also the
        // key type of the cached-states map, so it is used directly as the lookup key below.
        const auto  StateClass = Current.Get_CurrentStateClass();

        auto Node = FSmChainNode{};
        Node.Depth     = InDepth;
        Node.StateName = Format_Sm_ClassName(StateClass.Get());
        Node.RunStatus = Current.Get_RunStatus();
        OutChain.Add(MoveTemp(Node));

        if (InDepth >= InMaxDepth || StateClass.Get() == nullptr)
        { return; }

        if (NOT InSm.Has<ck::FFragment_Sm_Debug>())
        { return; }

        const auto& Debug       = InSm.Get<ck::FFragment_Sm_Debug>();
        const auto& Cached      = Debug.Get_CachedStates();
        const auto* CachedState = Cached.Find(StateClass);
        if (CachedState == nullptr)
        { return; }

        for (const auto& Task : CachedState->Tasks)
        {
            if (NOT Task.HasSubStateMachine)
            { continue; }

            const auto SubSm = FCk_Handle{ Task.SubSmHandle };
            if (ck::IsValid(SubSm))
            {
                Collect_SmChain(SubSm, InDepth + 1, InMaxDepth, InOutVisited, OutChain);
            }
        }
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
        { FieldTag_State(),   true },
        // Recent-transition history reads by default — the card is the one-screen NPC
        // debugger, and "how did it get into this state" is half the question.
        { FieldTag_History(), true },
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

    // --- State (recursive: top-level + every active sub-state-machine) ---
    if (Cfg.EnabledFields.HasTagExact(FieldTag_State()))
    {
        const auto MaxDepth = GetDefault<UCk_DebugOverlay_Settings>()->SmMaxRecursionDepth;

        auto Visited = TSet<uint32>{};
        auto Chain   = TArray<FSmChainNode>{};
        Collect_SmChain(Entity, 0, MaxDepth, Visited, Chain);

        for (const auto& ChainNode : Chain)
        {
            auto Sev = ECk_DebugOverlay_Severity::Normal;
            switch (ChainNode.RunStatus)
            {
                case ECk_SmRunStatus::Running: Sev = ECk_DebugOverlay_Severity::Good; break;
                case ECk_SmRunStatus::Paused:  Sev = ECk_DebugOverlay_Severity::Warn; break;
                case ECk_SmRunStatus::Stopped:
                default:                       Sev = ECk_DebugOverlay_Severity::Normal; break;
            }

            // Indent nested levels so the hierarchy reads as a chain of state chips.
            auto Prefix = FString{};
            for (auto Lvl = 0; Lvl < ChainNode.Depth; ++Lvl) { Prefix += TEXT("> "); }

            FCk_DebugOverlay_Row Row;
            Row.FieldTag = FieldTag_State();
            Row.Value    = FText::FromString(Prefix + ChainNode.StateName);
            Row.Severity = Sev;
            Out.Rows.Add(MoveTemp(Row));
        }
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
            const auto  From  = Entry.FromStateName.IsEmpty() ? FString(TEXT("(None)")) : Format_Sm_StateName(Entry.FromStateName);
            const auto  To    = Entry.ToStateName.IsEmpty()   ? FString(TEXT("(None)")) : Format_Sm_StateName(Entry.ToStateName);
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
