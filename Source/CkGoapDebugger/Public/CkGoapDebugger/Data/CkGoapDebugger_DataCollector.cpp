#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Time/CkTime_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkLabel/CkLabel_Utils.h"

#include "CkGoap/CkGoap_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"
#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Record_Internal.h"  // FFragment_RecordOfGoapPlanners + utils
#include "CkGoap/Action/CkGoap_Action_Fragment.h"
#include "CkGoap/Action/CkGoap_Action_Record_Internal.h"        // FFragment_RecordOfGoapActions + utils
#include "CkGoap/WorldState/CkGoap_WorldState_Fragment.h"
#include "CkGoap/WorldState/CkGoap_WorldState_Utils.h"

#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

// ====================================================================================================================
// FILE-STATIC STATE — singleton storage.
// ====================================================================================================================

namespace ck_goap_debugger_data_collector_internal
{
    // Per-entity history ring buffers. Keyed by the owning entity handle
    // (the one that holds the Goap root, not the Goap root itself).
    static TMap<FCk_Handle, TArray<FCkGoapDebugger_HistoryEvent>> GHistoryByEntity;

    // Previous-tick ROSTER per entity — the diff basis for event detection.
    // Flat by design: the deep per-agent snapshot map this replaced was one of
    // the dominant per-tick costs at ~150 agents. Cleared on PIE start/stop.
    static TMap<FCk_Handle, FCkGoapDebugger_RosterEntry> GPrevRosterByEntity;

    // Previous-tick FULL snapshot of the SELECTED entity only — the WS
    // recently-changed markup needs the previous deep snapshot, and only one
    // agent's deep data is ever rendered. Cleared on PIE start/stop.
    static TOptional<FCkGoapDebugger_EntitySnapshot> GPrevFullSelected;

    // Maximum events retained per entity. Older events drop off the front.
    constexpr int32 GMaxHistoryPerEntity = 256;

    // After this many frames a WS key value-change marker fades.
    constexpr int64 GRecentlyChangedFrameWindow = 30;

    // Fallback-Action cost floor. Duplicated per plan decision rather than
    // taking a dependency for one float — original:
    // CkGoapDebugger_DecisionModel.h:102 (ck_goap_debugger_decision_model::
    // k_FallbackCostFloor). Keep the two in sync.
    constexpr float GFallbackCostFloor = 900.0f;

    // Empty sentinel — returned by GetHistory when the entity has no entries.
    static const TArray<FCkGoapDebugger_HistoryEvent> GEmptyHistory{};

    static FDelegateHandle GSessionInvalidatedHandle;

    // ----------------------------------------------------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    ClearAllCaches() -> void
    {
        GHistoryByEntity.Reset();
        GPrevRosterByEntity.Reset();
        GPrevFullSelected.Reset();
    }

    static auto
    PushHistoryEvent(
        const FCk_Handle& InEntity,
        FCkGoapDebugger_HistoryEvent InEvent) -> void
    {
        auto& Ring = GHistoryByEntity.FindOrAdd(InEntity);
        Ring.Add(MoveTemp(InEvent));

        while (Ring.Num() > GMaxHistoryPerEntity)
        {
            Ring.RemoveAt(0);
        }
    }

    static auto
    GetCleanClassName(
        const UClass* InClass) -> FString
    {
        return UCk_Utils_Object_UE::Get_CleanClassName(InClass);
    }

    static auto
    DeriveActionTagFromClass(
        const TSubclassOf<UCk_GoapAction_EntityScript>& InClass) -> FGameplayTag
    {
        if (NOT ck::IsValid(InClass)) { return FGameplayTag{}; }
        return UCk_Utils_Object_UE::Get_TagFromClassName(InClass.Get(), FString{});
    }

    static auto
    AuthoredFromRawCondition(
        const ck::goap::FWorldStateCondition_Raw& InRaw) -> FCkGoapDebugger_Condition
    {
        auto Out = FCkGoapDebugger_Condition{};
        Out.Key   = InRaw.Key;
        Out.Value = InRaw.Value;
        return Out;
    }

    static auto
    AuthoredFromRawEffect(
        const ck::goap::FWorldStateEffect_Raw& InRaw) -> FCkGoapDebugger_Condition
    {
        auto Out = FCkGoapDebugger_Condition{};
        Out.Key   = InRaw.Key;
        Out.Value = InRaw.Value;
        return Out;
    }

    static auto
    AuthoredFromCkAuthored(
        const FCk_GoapWS_Condition_Authored& InAuthored) -> FCkGoapDebugger_Condition
    {
        auto Out = FCkGoapDebugger_Condition{};
        Out.Key   = InAuthored.Get_Key();
        Out.Value = InAuthored.Get_Value();
        return Out;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Planner world-state snapshot — reads the resolved WS handle's registry
    // + values, cross-references previous tick's snapshot for recently-changed
    // markers.
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    BuildWorldStateEntries(
        const FCk_Handle_Goap_WorldState& InWsHandle,
        const TArray<FCkGoapDebugger_WorldStateEntry>* InPrevEntries,
        int64 InCurrentFrame) -> TArray<FCkGoapDebugger_WorldStateEntry>
    {
        auto Out = TArray<FCkGoapDebugger_WorldStateEntry>{};

        if (NOT ck::IsValid(InWsHandle)) { return Out; }
        if (NOT InWsHandle.Has<ck::FFragment_Goap_WorldState_KeyRegistry>()) { return Out; }
        if (NOT InWsHandle.Has<ck::FFragment_Goap_WorldState_Values>())      { return Out; }

        const auto& RegFrag = InWsHandle.Get<ck::FFragment_Goap_WorldState_KeyRegistry>();

        const auto& Registry = RegFrag.Get_Registry();

        const auto& AllTags = Registry.GetAllTags();
        Out.Reserve(AllTags.Num());

        for (auto Index = 0; Index < AllTags.Num(); ++Index)
        {
            auto Entry = FCkGoapDebugger_WorldStateEntry{};
            Entry.Key   = AllTags[Index];
            // Read through the public Get_Value API so the override stack is
            // walked top-down. Reading FFragment_Goap_WorldState_Values
            // directly would return only the base store and miss DebugUI
            // overrides — the rail would display stale values, and the click
            // handler (which captures the displayed value) would push
            // idempotent no-ops on every subsequent click.
            Entry.Value = UCk_Utils_Goap_WorldState_UE::Get_Value(InWsHandle, Entry.Key);

            // Carry over recently-changed annotation: if prev had a different
            // value, this entry just changed.
            if (InPrevEntries != nullptr)
            {
                const auto* PrevEntry = InPrevEntries->FindByPredicate(
                    [&Entry](const FCkGoapDebugger_WorldStateEntry& In) { return In.Key == Entry.Key; });

                if (PrevEntry != nullptr)
                {
                    if (PrevEntry->Value != Entry.Value)
                    {
                        Entry.RecentlyChanged  = true;
                        Entry.LastChangedFrame = InCurrentFrame;
                    }
                    else
                    {
                        // Keep stale marker until the window expires
                        const auto FramesSince = InCurrentFrame - PrevEntry->LastChangedFrame;
                        if (PrevEntry->LastChangedFrame > 0 && FramesSince <= GRecentlyChangedFrameWindow)
                        {
                            Entry.RecentlyChanged  = true;
                            Entry.LastChangedFrame = PrevEntry->LastChangedFrame;
                        }
                    }
                }
            }

            Out.Add(MoveTemp(Entry));
        }

        return Out;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Per-Action snapshot. Pre-derived ChainDepth + IsInActiveChain are passed
    // in — the ActionSet snapshot owns the chain walk.
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    BuildActionInfo(
        const FCk_Handle_Goap_Action& InActionHandle,
        int32 InChainDepth,
        bool InIsInActiveChain) -> FCkGoapDebugger_ActionInfo
    {
        auto Info = FCkGoapDebugger_ActionInfo{};
        Info.Handle = InActionHandle;

        if (NOT ck::IsValid(InActionHandle)) { return Info; }

        // ---- Params (class, replan) -------------------------------------------------
        if (InActionHandle.Has<ck::FFragment_Goap_Action_Params>())
        {
            const auto& Params = InActionHandle.Get<ck::FFragment_Goap_Action_Params>();
            Info.ActionClass = Params.Get_ActionClass();
            Info.ClassName   = GetCleanClassName(Info.ActionClass.Get());
            Info.ActionTag   = DeriveActionTagFromClass(Info.ActionClass);
        }

        // ---- Definition (CDO-extracted preconditions/effects/cost) ------------------
        if (InActionHandle.Has<ck::FFragment_Goap_Action_Definition>())
        {
            const auto& Def = InActionHandle.Get<ck::FFragment_Goap_Action_Definition>();

            Info.Cost = Def.Get_Cost();

            for (const auto& Raw : Def.Get_Preconditions())
            {
                Info.Preconditions.Add(AuthoredFromRawCondition(Raw));
            }
            for (const auto& Raw : Def.Get_Effects())
            {
                Info.Effects.Add(AuthoredFromRawEffect(Raw));
            }
        }

        // ---- Planner-role live runtime state (PlanState + Goal + WS) ---------------
        if (InActionHandle.Has<ck::FFragment_Goap_Planner_PlanState>())
        {
            const auto& PlanState = InActionHandle.Get<ck::FFragment_Goap_Planner_PlanState>();

            Info.PlanStatus       = PlanState.Get_PlanStatus();
            Info.PlanCost         = PlanState.Get_PlanCost();
            Info.PlanAttemptCount = PlanState.Get_PlanAttemptCount();

            for (const auto& ChildHandle : PlanState.Get_Plan())
            {
                if (NOT ck::IsValid(ChildHandle)) { continue; }
                if (NOT ChildHandle.Has<ck::FFragment_Goap_Action_Params>()) { continue; }

                const auto& ChildParams = ChildHandle.Get<ck::FFragment_Goap_Action_Params>();
                Info.PlanClassNames.Add(GetCleanClassName(ChildParams.Get_ActionClass().Get()));
            }
        }

        if (InActionHandle.Has<ck::FFragment_Goap_Planner_Goal>())
        {
            const auto& GoalFrag = InActionHandle.Get<ck::FFragment_Goap_Planner_Goal>();

            for (const auto& InvalidEntry : GoalFrag.Get_InvalidGoal())
            {
                Info.InvalidGoal.Add(AuthoredFromCkAuthored(InvalidEntry));
            }

            // Goal display: the unified model populates _Goal as
            // ck::goap::FWorldStateCondition (key as FCk_GoapKey int). To present
            // tag-form we'd need the resolved WS registry.
            auto WsHandle = FCk_Handle_Goap_WorldState{};
            if (InActionHandle.Has<ck::FFragment_Goap_Planner_WorldStateSource>())
            {
                const auto& WSSource = InActionHandle.Get<ck::FFragment_Goap_Planner_WorldStateSource>();
                WsHandle = WSSource.Get_Resolved();
            }

            if (ck::IsValid(WsHandle) && WsHandle.Has<ck::FFragment_Goap_WorldState_KeyRegistry>())
            {
                const auto& Registry = WsHandle.Get<ck::FFragment_Goap_WorldState_KeyRegistry>().Get_Registry();
                for (const auto& Cond : GoalFrag.Get_Goal())
                {
                    auto Display = FCkGoapDebugger_Condition{};
                    Display.Key   = Registry.GetTag(Cond.Key);
                    Display.Value = Cond.Value;
                    Info.Goal.Add(MoveTemp(Display));
                }

                Info.WorldStateSourceLabel = UCk_Utils_Handle_UE::Get_DebugName(WsHandle).ToString();
            }
        }

        // ---- Tree position (parent + children) -------------------------------------
        if (InActionHandle.Has<ck::FFragment_Goap_Action_Tree>())
        {
            const auto& Tree = InActionHandle.Get<ck::FFragment_Goap_Action_Tree>();

            Info.ParentActionHandle = Tree.Get_ParentAction();

            if (ck::IsValid(Info.ParentActionHandle) &&
                Info.ParentActionHandle.Has<ck::FFragment_Goap_Action_Params>())
            {
                const auto& ParentParams = Info.ParentActionHandle.Get<ck::FFragment_Goap_Action_Params>();
                Info.ParentClassName = GetCleanClassName(ParentParams.Get_ActionClass().Get());
            }

            Info.ChildActionHandles = Tree.Get_ChildActions();

            // U11.7-A: Parent Planner. In the transitional model, the parent
            // Action entity may itself carry the Planner role; if so, it IS
            // the parent Planner. (Top-level Actions registered directly under
            // a Planner have an invalid _ParentAction — the Planner is then
            // the registering entity, not derivable from the Action's _Tree.)
            if (ck::IsValid(Info.ParentActionHandle) &&
                Info.ParentActionHandle.Has<ck::FFragment_Goap_Planner_Params>())
            {
                Info.ParentPlanner =
                    UCk_Utils_Goap_Planner_UE::CastChecked(Info.ParentActionHandle);
            }
        }

        // ---- Role badges (U11.7-A) ---------------------------------------------------
        // Action role is by definition (we got here via an Action handle). Dual-role if
        // this entity also carries the Planner discriminator.
        Info.IsActionRole  = true;
        Info.IsPlannerRole = InActionHandle.Has<ck::FFragment_Goap_Planner_Params>();

        // ---- Replan throttle (seconds-since-last) ----------------------------------
        // PR-B.1b Stage 5: ReplanThrottle lives on the Planner entity. Read it
        // here only for promoted mid-tier Planners (the Action handle that
        // also carries the Planner role).
        // The throttle now stores a world-time STAMP of the last replan rather than a per-frame
        // accumulator (the accumulator could not survive AutoReplan skipping idle planners), so the
        // displayed seconds-since is derived here against the same clock the processor compares to.
        if (InActionHandle.Has<ck::FFragment_Goap_Planner_ReplanThrottle>())
        {
            const auto& Throttle = InActionHandle.Get<ck::FFragment_Goap_Planner_ReplanThrottle>();

            const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InActionHandle);
            const auto TimeParams = FCk_Utils_Time_GetWorldTime_Params{World};
            const auto Now = static_cast<double>(
                UCk_Utils_Time_UE::Get_WorldTime(TimeParams).Get_WorldTime().Get_Time().Get_Seconds());

            Info.SecondsSinceLastReplan = static_cast<float>(
                FMath::Max(0.0, Now - Throttle.Get_LastReplanWorldTimeSeconds()));
        }

        // ---- Chain position --------------------------------------------------------
        Info.IsInActiveChain = InIsInActiveChain;
        Info.ChainDepth      = InIsInActiveChain ? InChainDepth : -1;

        if (NOT InIsInActiveChain)
        {
            Info.Role = ECkGoapDebugger_ActionRole::Catalog;
        }
        else if (InChainDepth == 0)
        {
            Info.Role = ECkGoapDebugger_ActionRole::Root;
        }
        else
        {
            // Mid vs Leaf: Mid if the Action has registered children AND it is
            // NOT the last entry in the chain. The caller knows the chain length
            // and tags us via InChainDepth + InIsInActiveChain only. Defer the
            // Mid/Leaf decision to the ActionSet pass that owns the chain array.
            // For now mark as Leaf — the ActionSet builder overrides Mid when
            // appropriate.
            Info.Role = ECkGoapDebugger_ActionRole::Leaf;
        }

        return Info;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Per-Planner snapshot (U11.7-A, spec §3.4 / §4).
    //
    // Walks: identity → goal/plan/WS → role badges → child Actions (direct children
    // via Action_Tree under the Planner's root) → recursive sub-Planners.
    //
    // In the transitional U11 model every Action entity carries the Planner-role
    // fragment cluster. The discriminator for "this entity should be surfaced as
    // a Planner in the debugger" is FFragment_Goap_Planner_Params (set by
    // UCk_Utils_Goap_Planner_UE::Add and Promote). Sub-Planners that arise from
    // PromoteActionToPlanner are dual-role and surface in BOTH this tree (as
    // child Planners under their parent) and the parent's ChildActions list.
    // ----------------------------------------------------------------------------------------------------------------

    // Forward decl — BuildPlannerInfo recurses via BuildPlannerInfo_Recursive
    // which itself recurses back into BuildPlannerInfo for nested sub-Planners.
    static auto
    BuildPlannerInfo_Recursive(
        const FCk_Handle_Goap_Planner& InPlannerHandle,
        const FCk_Handle_Goap_Planner& InParentPlannerHandle,
        const TArray<FCk_Handle_Goap_Action>& InActiveChainHandles,
        const TMap<FCk_Handle_Goap_Action, int32>& InChainDepthByHandle,
        const FCkGoapDebugger_EntitySnapshot* InPrevSnapshot,
        int64 InCurrentFrame,
        TSet<FCk_Handle_Goap_Planner>& InOutVisited) -> FCkGoapDebugger_PlannerInfo;

    static auto
    BuildPlannerInfo_Recursive(
        const FCk_Handle_Goap_Planner& InPlannerHandle,
        const FCk_Handle_Goap_Planner& InParentPlannerHandle,
        const TArray<FCk_Handle_Goap_Action>& InActiveChainHandles,
        const TMap<FCk_Handle_Goap_Action, int32>& InChainDepthByHandle,
        const FCkGoapDebugger_EntitySnapshot* InPrevSnapshot,
        int64 InCurrentFrame,
        TSet<FCk_Handle_Goap_Planner>& InOutVisited) -> FCkGoapDebugger_PlannerInfo
    {
        auto Info = FCkGoapDebugger_PlannerInfo{};
        Info.PlannerHandle = InPlannerHandle;
        Info.ParentPlanner = InParentPlannerHandle;

        if (NOT ck::IsValid(InPlannerHandle)) { return Info; }

        // Defensive cycle guard. The tree is acyclic by construction (a Planner
        // can only be promoted from an Action whose _ParentAction is unrelated
        // to this Planner's identity) but PIE corruption / debugger edge cases
        // shouldn't recurse forever.
        if (InOutVisited.Contains(InPlannerHandle)) { return Info; }
        InOutVisited.Add(InPlannerHandle);

        // ---- Identity ---------------------------------------------------------------
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_Params>())
        {
            const auto& Params = InPlannerHandle.Get<ck::FFragment_Goap_Planner_Params>();
            Info.PlannerTag = Params.Get_PlannerTag();
            Info.EnableToggle = Params.Get_InitialToggle();
            Info.AllowPlanFailed = Params.Get_AllowPlanFailed();

            // Settings drawer block.
            Info.ReplanPolicy             = Params.Get_ReplanPolicy();
            Info.MinReplanIntervalSeconds = Params.Get_MinReplanIntervalSeconds();
            Info.SearchBudgetMicroseconds = Params.Get_SearchBudgetMicroseconds();
            Info.CostThreshold            = Params.Get_CostThreshold();
            Info.PlanOnStart              = Params.Get_PlanOnStart();
        }
        if (Info.PlannerTag.IsValid())
        {
            auto TagString = Info.PlannerTag.ToString();
            auto LastDot = int32{INDEX_NONE};
            if (TagString.FindLastChar(TEXT('.'), LastDot))
            {
                Info.DisplayName = TagString.RightChop(LastDot + 1);
            }
            else
            {
                Info.DisplayName = TagString;
            }
        }

        // ---- Role badges ------------------------------------------------------------
        Info.IsPlannerRole = true;
        Info.IsActionRole  = InPlannerHandle.Has<ck::FFragment_Goap_Action_Definition>();

        // ---- Action-role payload (only when dual-role) ------------------------------
        if (Info.IsActionRole)
        {
            const auto& Def = InPlannerHandle.Get<ck::FFragment_Goap_Action_Definition>();
            Info.Cost = Def.Get_Cost();
            for (const auto& Raw : Def.Get_Preconditions())
            {
                Info.Preconditions.Add(AuthoredFromRawCondition(Raw));
            }
            for (const auto& Raw : Def.Get_Effects())
            {
                Info.Effects.Add(AuthoredFromRawEffect(Raw));
            }
        }

        // ---- Enable + activation ----------------------------------------------------
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_Current>())
        {
            const auto& Current = InPlannerHandle.Get<ck::FFragment_Goap_Planner_Current>();
            Info.EnableToggle = Current.Get_EnableToggle();
            Info.HasUnconditionalFallback = Current.Get_HasUnconditionalFallback();

            Info.DependencyCycles = Current.Get_DependencyCycles();
            for (const auto& Cycle : Info.DependencyCycles)
            {
                auto CycleInfo = FCkGoapDebugger_CycleInfo{};
                for (const auto& InCycleClass : Cycle.Get_ActionsInCycle())
                {
                    CycleInfo.ActionsInCycle.Add(GetCleanClassName(InCycleClass.Get()));
                }
                CycleInfo.CycleConditions = Cycle.Get_CycleConditions();
                Info.DependencyCyclesDisplay.Add(MoveTemp(CycleInfo));
            }
        }
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_Activation>())
        {
            const auto& Activation = InPlannerHandle.Get<ck::FFragment_Goap_Planner_Activation>();
            Info.IsActive = Activation.Get_IsActive();
        }

        // A sub-Planner is in the active chain when the entity carrying its
        // Action role (same handle, different cast) appears in the top-level
        // chain. For top-level Planners themselves (no Action role) treat
        // IsActive as the answer.
        {
            const auto AsAction = ck::StaticCast<FCk_Handle_Goap_Action>(
                static_cast<FCk_Handle>(InPlannerHandle));
            Info.IsInActiveChain = InChainDepthByHandle.Contains(AsAction) || Info.IsActive;
        }

        // PR-B.1b Stage 5: the canonical-entity redirect is gone. PlanState /
        // Goal / WorldStateSource live on the Planner entity directly for
        // both top-level Planners and promoted mid-tier Planners.
        auto CanonicalHandle = static_cast<FCk_Handle>(InPlannerHandle);

        // ---- Plan + goal (Planner role) — read from the Planner entity ----------
        if (CanonicalHandle.Has<ck::FFragment_Goap_Planner_PlanState>())
        {
            const auto& PlanState = CanonicalHandle.Get<ck::FFragment_Goap_Planner_PlanState>();
            Info.PlanStatus       = PlanState.Get_PlanStatus();
            Info.PlanCost         = PlanState.Get_PlanCost();
            Info.PlanAttemptCount = PlanState.Get_PlanAttemptCount();
            Info.PlanHandles      = PlanState.Get_Plan();

            for (const auto& ChildHandle : Info.PlanHandles)
            {
                if (NOT ck::IsValid(ChildHandle))
                {
                    Info.PlanClassNames.Add(FString{});
                    continue;
                }
                if (NOT ChildHandle.Has<ck::FFragment_Goap_Action_Params>())
                {
                    Info.PlanClassNames.Add(FString{});
                    continue;
                }
                const auto& ChildParams = ChildHandle.Get<ck::FFragment_Goap_Action_Params>();
                Info.PlanClassNames.Add(GetCleanClassName(ChildParams.Get_ActionClass().Get()));
            }
        }

        // ---- WS source (canonical entity) ------------------------------------------
        if (CanonicalHandle.Has<ck::FFragment_Goap_Planner_WorldStateSource>())
        {
            const auto& WSSource = CanonicalHandle.Get<ck::FFragment_Goap_Planner_WorldStateSource>();
            Info.WorldStateSourceOverride = WSSource.Get_WorldStateSource();
            Info.WorldStateSourceResolved = WSSource.Get_Resolved();
        }

        const auto WsForDisplay = ck::IsValid(Info.WorldStateSourceResolved)
            ? Info.WorldStateSourceResolved
            : Info.WorldStateSourceOverride;
        if (ck::IsValid(WsForDisplay))
        {
            Info.WorldStateSourceLabel = UCk_Utils_Handle_UE::Get_DebugName(WsForDisplay).ToString();
        }

        // ---- Last-replan diagnostics + search stats (P3 hooks) ---------------------
        if (CanonicalHandle.Has<ck::FFragment_Goap_Planner_ReplanCause>())
        {
            Info.LastReplanCause = CanonicalHandle.Get<ck::FFragment_Goap_Planner_ReplanCause>().Get_Info();
        }

        Info.SearchStats = UCk_Utils_Goap_Planner_UE::Get_LastSearchStats(InPlannerHandle);
        if (ck::IsValid(WsForDisplay) &&
            WsForDisplay.Has<ck::FFragment_Goap_WorldState_KeyRegistry>())
        {
            Info.SearchDebug = UCk_Utils_Goap_Planner_UE::Get_LastSearchDebug(InPlannerHandle);
        }

        if (ck::IsValid(WsForDisplay) &&
            WsForDisplay.Has<ck::FFragment_Goap_WorldState_ChangeLog>())
        {
            Info.RecentWorldStateChanges =
                WsForDisplay.Get<ck::FFragment_Goap_WorldState_ChangeLog>().Get_Entries();
        }

        // ---- Goal (canonical entity) -----------------------------------------------
        if (CanonicalHandle.Has<ck::FFragment_Goap_Planner_Goal>())
        {
            const auto& GoalFrag = CanonicalHandle.Get<ck::FFragment_Goap_Planner_Goal>();
            Info.GoalAuthored        = GoalFrag.Get_GoalAuthored();
            Info.InvalidGoalAuthored = GoalFrag.Get_InvalidGoal();

            if (ck::IsValid(WsForDisplay) &&
                WsForDisplay.Has<ck::FFragment_Goap_WorldState_KeyRegistry>())
            {
                const auto& Registry = WsForDisplay
                    .Get<ck::FFragment_Goap_WorldState_KeyRegistry>().Get_Registry();
                for (const auto& Cond : GoalFrag.Get_Goal())
                {
                    auto Display = FCkGoapDebugger_Condition{};
                    Display.Key   = Registry.GetTag(Cond.Key);
                    Display.Value = Cond.Value;
                    Info.GoalResolved.Add(MoveTemp(Display));
                }
            }
        }

        // ---- WorldState entries (resolved registry + values) -----------------------
        // Mirrors what the legacy ActionSetInfo path produced — Planner consumers
        // (WorldStateRail) read this list directly off PlannerInfo so the legacy
        // shim can be retired.
        {
            const TArray<FCkGoapDebugger_WorldStateEntry>* PrevEntries = nullptr;
            if (InPrevSnapshot != nullptr)
            {
                // Walk prior snapshot's TopLevelPlanners forest to find this
                // Planner's previous WorldState entries (used for recently-
                // changed markup).
                auto FindPrevWs =
                    [&](auto& InSelf,
                        const TArray<FCkGoapDebugger_PlannerInfo>& InPlanners)
                    -> const TArray<FCkGoapDebugger_WorldStateEntry>*
                    {
                        for (const auto& P : InPlanners)
                        {
                            if (P.PlannerHandle == InPlannerHandle)
                            { return &P.WorldState; }
                            if (const auto* Found = InSelf(InSelf, P.ChildPlanners))
                            { return Found; }
                        }
                        return nullptr;
                    };
                PrevEntries = FindPrevWs(FindPrevWs, InPrevSnapshot->TopLevelPlanners);
            }
            Info.WorldState = BuildWorldStateEntries(WsForDisplay, PrevEntries, InCurrentFrame);
        }

        // ---- Direct children (U11.7 B.4) -------------------------------------------
        // Build a fully-populated tree so downstream widgets can navigate without
        // falling back to the legacy ActionSetInfo catalog. "Direct children" per
        // spec §7.2 / FProcessor_Goap_Planner_Setup:
        //   - Promoted Action-Planner (dual-role host): host._ChildActions.
        //   - Top-level Planner: rootAction._ChildActions.
        // Each direct child becomes an ActionInfo; dual-role children additionally
        // get an entry in ChildPlanners.
        {
            using ActionHandle = FCk_Handle_Goap_Action;
            auto DirectChildren = TArray<ActionHandle>{};

            if (InPlannerHandle.Has<ck::FFragment_Goap_Action_Tree>())
            {
                const auto& Tree = InPlannerHandle.Get<ck::FFragment_Goap_Action_Tree>();
                DirectChildren = Tree.Get_ChildActions();
            }
            else if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_ActionCatalogIndex>())
            {
                // PR-B.1b Stage 5: top-level Planners' direct children come from
                // the ActionCatalogIndex (every AddAction registers there).
                const auto& Index = InPlannerHandle.Get<ck::FFragment_Goap_Planner_ActionCatalogIndex>();
                DirectChildren.Reserve(Index.Get_TagToAction().Num());
                for (const auto& Pair : Index.Get_TagToAction())
                {
                    if (ck::IsValid(Pair.Value)) { DirectChildren.Add(Pair.Value); }
                }
            }

            Info.ChildActions.Reserve(DirectChildren.Num());
            for (const auto& ChildAction : DirectChildren)
            {
                if (NOT ck::IsValid(ChildAction)) { continue; }

                const auto* DepthPtr  = InChainDepthByHandle.Find(ChildAction);
                const bool  IsInChain = DepthPtr != nullptr;
                const auto  Depth     = IsInChain ? *DepthPtr : -1;

                auto ChildInfo = BuildActionInfo(ChildAction, Depth, IsInChain);
                Info.ChildActions.Add(MoveTemp(ChildInfo));

                // Dual-role child: also surface as a sub-Planner. The Planner-
                // role discriminator is FFragment_Goap_Planner_Params (set by
                // PromoteActionToPlanner and the top-level Add path).
                if (ChildAction.Has<ck::FFragment_Goap_Planner_Params>())
                {
                    const auto ChildAsPlanner =
                        UCk_Utils_Goap_Planner_UE::CastChecked(ChildAction);
                    if (ck::IsValid(ChildAsPlanner))
                    {
                        auto SubPlanner = BuildPlannerInfo_Recursive(
                            ChildAsPlanner,
                            InPlannerHandle,
                            InActiveChainHandles,
                            InChainDepthByHandle,
                            InPrevSnapshot,
                            InCurrentFrame,
                            InOutVisited);
                        Info.ChildPlanners.Add(MoveTemp(SubPlanner));
                    }
                }
            }
        }

        // ---- Per-key usage census over this planner's subtree ----------------------
        // X = precondition/goal reads, Y = effect writes. Children were built
        // above, so fold their maps up and add this tier's contributions.
        {
            const auto Bump = [&Info](const FGameplayTag& InKey, int32 InPreDelta, int32 InEffDelta)
            {
                if (NOT InKey.IsValid()) { return; }
                auto& Usage = Info.KeyUsage.FindOrAdd(InKey);
                Usage.X += InPreDelta;
                Usage.Y += InEffDelta;
            };

            for (const auto& Goal : Info.GoalAuthored)
            { Bump(Goal.Get_Key(), 1, 0); }

            for (const auto& Child : Info.ChildActions)
            {
                for (const auto& Pre : Child.Preconditions) { Bump(Pre.Key, 1, 0); }
                for (const auto& Eff : Child.Effects)       { Bump(Eff.Key, 0, 1); }
            }

            for (const auto& ChildPlanner : Info.ChildPlanners)
            {
                for (const auto& [Key, Usage] : ChildPlanner.KeyUsage)
                {
                    auto& Merged = Info.KeyUsage.FindOrAdd(Key);
                    Merged.X += Usage.X;
                    Merged.Y += Usage.Y;
                }
            }
        }

        return Info;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Per-ActionSet snapshot (LEGACY SHIM — pre-U11 widget consumers).
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    BuildActionSetInfo(
        const FCk_Handle_Goap_Planner& InActionSetHandle,
        const FCkGoapDebugger_EntitySnapshot* InPrevSnapshot,
        int64 InCurrentFrame) -> FCkGoapDebugger_ActionSetInfo
    {
        auto Info = FCkGoapDebugger_ActionSetInfo{};
        Info.Handle = InActionSetHandle;

        if (NOT ck::IsValid(InActionSetHandle)) { return Info; }

        // ---- Identity (tag + debug name) -------------------------------------------
        if (InActionSetHandle.Has<ck::FFragment_Goap_Planner_Params>())
        {
            const auto& Params = InActionSetHandle.Get<ck::FFragment_Goap_Planner_Params>();
            Info.ActionSetTag = Params.Get_PlannerTag();
            Info.EnableToggle = Params.Get_InitialToggle();
        }

        if (Info.ActionSetTag.IsValid())
        {
            // Leaf-only name for compactness.
            auto TagString = Info.ActionSetTag.ToString();
            auto LastDot = int32{INDEX_NONE};
            if (TagString.FindLastChar(TEXT('.'), LastDot))
            {
                Info.DebugName = TagString.RightChop(LastDot + 1);
            }
            else
            {
                Info.DebugName = TagString;
            }
        }

        // ---- Current (enable toggle, dependency cycles) ----------------------------
        // PR-B.1b Stage 5: _RootAction is gone; RootActionHandle stays invalid
        // (the field is preserved on FCkGoapDebugger_ActionSetInfo for now to
        // avoid breaking downstream widget consumers).
        if (InActionSetHandle.Has<ck::FFragment_Goap_Planner_Current>())
        {
            const auto& Current = InActionSetHandle.Get<ck::FFragment_Goap_Planner_Current>();
            Info.EnableToggle    = Current.Get_EnableToggle();

            for (const auto& Cycle : Current.Get_DependencyCycles())
            {
                auto CycleInfo = FCkGoapDebugger_CycleInfo{};
                for (const auto& InCycleClass : Cycle.Get_ActionsInCycle())
                {
                    CycleInfo.ActionsInCycle.Add(GetCleanClassName(InCycleClass.Get()));
                }
                CycleInfo.CycleConditions = Cycle.Get_CycleConditions();
                Info.DependencyCycles.Add(MoveTemp(CycleInfo));
            }
        }

        // ---- Active chain -----------------------------------------------------------
        // U11.2: the active chain is no longer stored in a fragment — derive it
        // from the runtime walk exposed by UCk_Utils_Goap_Planner_UE::Get_ActiveChain.
        Info.ActiveChainHandles = UCk_Utils_Goap_Planner_UE::Get_ActiveChain(InActionSetHandle);

        // ---- WS source --------------------------------------------------------------
        auto WsHandle = FCk_Handle_Goap_WorldState{};
        if (InActionSetHandle.Has<ck::FFragment_Goap_Planner_WorldStateSource>())
        {
            const auto& Src = InActionSetHandle.Get<ck::FFragment_Goap_Planner_WorldStateSource>();
            WsHandle = Src.Get_WorldStateSource();
        }

        if (ck::IsValid(WsHandle))
        {
            Info.WorldStateSourceLabel = UCk_Utils_Handle_UE::Get_DebugName(WsHandle).ToString();
            Info.WorldStateHandle      = WsHandle;

            const TArray<FCkGoapDebugger_WorldStateEntry>* PrevEntries = nullptr;
            if (InPrevSnapshot != nullptr)
            {
                const auto* PrevAs = InPrevSnapshot->ActionSets.FindByPredicate(
                    [&InActionSetHandle](const FCkGoapDebugger_ActionSetInfo& In)
                    {
                        return In.Handle == InActionSetHandle;
                    });

                if (PrevAs != nullptr) { PrevEntries = &PrevAs->WorldState; }
            }

            Info.WorldState = BuildWorldStateEntries(WsHandle, PrevEntries, InCurrentFrame);
        }

        // ---- Catalog walk: gather all Action entities in the Planner ---------------
        // Determine chain membership + depth from ActiveChainHandles.
        auto ChainDepthByHandle = TMap<FCk_Handle_Goap_Action, int32>{};
        for (auto Index = 0; Index < Info.ActiveChainHandles.Num(); ++Index)
        {
            ChainDepthByHandle.Add(Info.ActiveChainHandles[Index], Index);
        }

        auto MutableActionSet = InActionSetHandle;
        ck::goap::internal_action::FRecordOfGoapActions_Utils::ForEach_ValidEntry(
            MutableActionSet,
            [&Info, &ChainDepthByHandle](FCk_Handle_Goap_Action InAction)
            {
                if (NOT ck::IsValid(InAction)) { return; }

                const auto* DepthPtr = ChainDepthByHandle.Find(InAction);
                const bool  IsInChain = DepthPtr != nullptr;
                const auto  Depth     = IsInChain ? *DepthPtr : -1;

                auto ActionInfo = BuildActionInfo(InAction, Depth, IsInChain);
                Info.Catalog.Add(MoveTemp(ActionInfo));
            });

        // ---- Refine Mid vs Leaf for chain members based on position ----------------
        const auto ChainLen = Info.ActiveChainHandles.Num();
        for (auto& ActionInfo : Info.Catalog)
        {
            if (NOT ActionInfo.IsInActiveChain) { continue; }
            if (ActionInfo.ChainDepth == 0) { continue; }  // Root already

            const bool IsLast = (ActionInfo.ChainDepth == ChainLen - 1);
            const bool HasChildren = ActionInfo.ChildActionHandles.Num() > 0;

            if (IsLast)
            {
                ActionInfo.Role = ECkGoapDebugger_ActionRole::Leaf;
            }
            else if (HasChildren)
            {
                ActionInfo.Role = ECkGoapDebugger_ActionRole::Mid;
            }
            else
            {
                ActionInfo.Role = ECkGoapDebugger_ActionRole::Leaf;
            }
        }

        return Info;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Per-entity snapshot. Walks the Goap root's RecordOfGoapActionSets to
    // gather every Planner entity.
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    BuildEntitySnapshot(
        const FCk_Handle& InEntityHandle,
        const FCk_Handle_Goap_Planner& InGoapHandle,
        UWorld* InWorld,
        const FCkGoapDebugger_EntitySnapshot* InPrevSnapshot) -> FCkGoapDebugger_EntitySnapshot
    {
        auto Snapshot = FCkGoapDebugger_EntitySnapshot{};
        Snapshot.EntityHandle = InEntityHandle;
        Snapshot.GoapHandle   = InGoapHandle;
        Snapshot.DebugName    = UCk_Utils_Handle_UE::Get_DebugName(InEntityHandle).ToString();

        // CDO-named entities (Add-style gym stations) carry UE's "Default__"
        // marker — pure noise on every display surface (agent card, pickers,
        // squad rows).
        Snapshot.DebugName.RemoveFromStart(TEXT("Default__"));

        if (Snapshot.DebugName.IsEmpty())
        {
            Snapshot.DebugName = TEXT("(unnamed)");
        }

        Snapshot.FrameNumber      = static_cast<int64>(GFrameCounter);
        Snapshot.WorldTimeSeconds = (InWorld != nullptr) ? InWorld->GetTimeSeconds() : 0.0;

        // Enumerate top-level Planners from BOTH installation paradigms:
        //   - Add:    the owner entity ITSELF carries the Planner role (the Add
        //             path never writes a record entry).
        //   - Create: child Planner entities registered in the owner's private
        //             record-of-planners.
        // Each top-level Planner contributes ONE PlannerInfo (new shape) AND ONE
        // legacy ActionSetInfo (shim).
        auto MutableOwner = InEntityHandle;

        const auto AppendTopLevelPlanner = [&Snapshot, InPrevSnapshot](FCk_Handle_Goap_Planner InPlanner)
        {
            if (NOT ck::IsValid(InPlanner)) { return; }

            // New shape (spec §2.1). Top-level Planners have invalid ParentPlanner
            // by construction. Build the full recursive tree with WS-prev linkage
            // so recently-changed markers carry across frames.
            auto ActiveChain = UCk_Utils_Goap_Planner_UE::Get_ActiveChain(InPlanner);

            auto ChainDepthByHandle = TMap<FCk_Handle_Goap_Action, int32>{};
            for (auto Idx = 0; Idx < ActiveChain.Num(); ++Idx)
            { ChainDepthByHandle.Add(ActiveChain[Idx], Idx); }

            auto Visited = TSet<FCk_Handle_Goap_Planner>{};
            auto PlannerInfo = BuildPlannerInfo_Recursive(
                InPlanner,
                FCk_Handle_Goap_Planner{},
                ActiveChain,
                ChainDepthByHandle,
                InPrevSnapshot,
                Snapshot.FrameNumber,
                Visited);
            Snapshot.TopLevelPlanners.Add(MoveTemp(PlannerInfo));

            // Legacy shim — synthesize ActionSetInfo for the existing widgets.
            auto AsInfo = BuildActionSetInfo(InPlanner, InPrevSnapshot, Snapshot.FrameNumber);
            Snapshot.ActionSets.Add(MoveTemp(AsInfo));
        };

        AppendTopLevelPlanner(UCk_Utils_Goap_Planner_UE::Cast(MutableOwner));

        ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
            MutableOwner, AppendTopLevelPlanner);

        return Snapshot;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // ROSTER — the cheap all-agents tier. One flat row per top-level Planner,
    // read straight off fragments. No recursion, no WS key scan, no catalog
    // walk. See CkGoapDebugger_Types.h for why this tier exists.
    // ----------------------------------------------------------------------------------------------------------------

    // Tag leaf, matching FCkGoapDebugger_PlannerInfo::DisplayName exactly
    // (BuildPlannerInfo_Recursive's identity block) so squad rows read the same.
    static auto
    DisplayNameFromPlannerTag(
        const FGameplayTag& InTag) -> FString
    {
        if (NOT InTag.IsValid()) { return FString{}; }

        auto TagString = InTag.ToString();
        auto LastDot   = int32{INDEX_NONE};
        if (TagString.FindLastChar(TEXT('.'), LastDot))
        { return TagString.RightChop(LastDot + 1); }
        return TagString;
    }

    static auto
    BuildRosterPlannerRow(
        const FCk_Handle_Goap_Planner& InPlanner) -> FCkGoapDebugger_RosterPlannerRow
    {
        auto Row = FCkGoapDebugger_RosterPlannerRow{};
        Row.PlannerHandle = InPlanner;

        if (NOT ck::IsValid(InPlanner)) { return Row; }

        if (InPlanner.Has<ck::FFragment_Goap_Planner_Params>())
        {
            const auto& Params = InPlanner.Get<ck::FFragment_Goap_Planner_Params>();
            Row.PlannerTag      = Params.Get_PlannerTag();
            Row.EnableToggle    = Params.Get_InitialToggle();
            Row.AllowPlanFailed = Params.Get_AllowPlanFailed();
        }
        Row.DisplayName = DisplayNameFromPlannerTag(Row.PlannerTag);

        if (InPlanner.Has<ck::FFragment_Goap_Planner_Current>())
        {
            const auto& Current = InPlanner.Get<ck::FFragment_Goap_Planner_Current>();
            Row.EnableToggle             = Current.Get_EnableToggle();
            Row.HasUnconditionalFallback = Current.Get_HasUnconditionalFallback();
        }

        if (InPlanner.Has<ck::FFragment_Goap_Planner_PlanState>())
        {
            const auto& PlanState = InPlanner.Get<ck::FFragment_Goap_Planner_PlanState>();
            Row.PlanStatus       = PlanState.Get_PlanStatus();
            Row.PlanCost         = PlanState.Get_PlanCost();
            Row.PlanAttemptCount = PlanState.Get_PlanAttemptCount();
        }

        if (InPlanner.Has<ck::FFragment_Goap_Planner_ReplanCause>())
        {
            Row.LastReplanCause = InPlanner.Get<ck::FFragment_Goap_Planner_ReplanCause>().Get_Info();
        }

        if (InPlanner.Has<ck::FFragment_Goap_Planner_WorldStateSource>())
        {
            const auto& WsSource = InPlanner.Get<ck::FFragment_Goap_Planner_WorldStateSource>();
            Row.WorldStateHandle = ck::IsValid(WsSource.Get_Resolved())
                ? WsSource.Get_Resolved()
                : WsSource.Get_WorldStateSource();
        }

        // ---- Active spine: Plan[0] descent -----------------------------------------
        // Fragment-level twin of SCkGoapDebugger_SquadTable::Compute_ChainText's
        // walk over PlannerInfo — emit each level's Plan[0] class name, then
        // descend into it only when that step is itself a Planner (dual-role).
        {
            auto Visited = TSet<FCk_Handle>{};
            auto Cursor  = static_cast<FCk_Handle>(InPlanner);

            while (ck::IsValid(Cursor) && NOT Visited.Contains(Cursor))
            {
                Visited.Add(Cursor);

                if (NOT Cursor.Has<ck::FFragment_Goap_Planner_PlanState>()) { break; }

                const auto& Plan = Cursor.Get<ck::FFragment_Goap_Planner_PlanState>().Get_Plan();
                if (Plan.IsEmpty()) { break; }

                const auto& Step = Plan[0];
                Row.ChainStepHandles.Add(Step);

                auto StepClassName = FString{};
                if (ck::IsValid(Step) && Step.Has<ck::FFragment_Goap_Action_Params>())
                {
                    StepClassName = GetCleanClassName(
                        Step.Get<ck::FFragment_Goap_Action_Params>().Get_ActionClass().Get());
                }
                Row.ChainStepClassNames.Add(MoveTemp(StepClassName));

                if (NOT ck::IsValid(Step) || NOT Step.Has<ck::FFragment_Goap_Planner_Params>())
                { break; }

                Cursor = static_cast<FCk_Handle>(Step);
            }
        }

        // ---- Fallback alert input ---------------------------------------------------
        // Reproduces SquadTable.cpp:218-227 exactly: is this Planner's Plan[0]
        // one of its direct children whose cost sits at/above the fallback
        // floor? Direct-children resolution mirrors BuildPlannerInfo_Recursive
        // (Action_Tree first for dual-role hosts, else the catalog index).
        if (NOT Row.ChainStepClassNames.IsEmpty() && NOT Row.ChainStepClassNames[0].IsEmpty())
        {
            const auto& Plan0Name = Row.ChainStepClassNames[0];

            auto DirectChildren = TArray<FCk_Handle_Goap_Action>{};
            if (InPlanner.Has<ck::FFragment_Goap_Action_Tree>())
            {
                DirectChildren = InPlanner.Get<ck::FFragment_Goap_Action_Tree>().Get_ChildActions();
            }
            else if (InPlanner.Has<ck::FFragment_Goap_Planner_ActionCatalogIndex>())
            {
                const auto& Index = InPlanner.Get<ck::FFragment_Goap_Planner_ActionCatalogIndex>();
                DirectChildren.Reserve(Index.Get_TagToAction().Num());
                for (const auto& Pair : Index.Get_TagToAction())
                {
                    if (ck::IsValid(Pair.Value)) { DirectChildren.Add(Pair.Value); }
                }
            }

            for (const auto& Child : DirectChildren)
            {
                if (NOT ck::IsValid(Child)) { continue; }
                if (NOT Child.Has<ck::FFragment_Goap_Action_Params>())     { continue; }
                if (NOT Child.Has<ck::FFragment_Goap_Action_Definition>()) { continue; }

                const auto ChildName = GetCleanClassName(
                    Child.Get<ck::FFragment_Goap_Action_Params>().Get_ActionClass().Get());
                if (ChildName != Plan0Name) { continue; }

                if (Child.Get<ck::FFragment_Goap_Action_Definition>().Get_Cost() >= GFallbackCostFloor)
                {
                    Row.ChainLeafIsFallback = true;
                    break;
                }
            }
        }

        return Row;
    }

    static auto
    BuildRosterEntry(
        const FCk_Handle& InEntityHandle,
        UWorld* InWorld) -> FCkGoapDebugger_RosterEntry
    {
        auto Entry = FCkGoapDebugger_RosterEntry{};
        Entry.EntityHandle = InEntityHandle;
        Entry.DebugName    = UCk_Utils_Handle_UE::Get_DebugName(InEntityHandle).ToString();

        // Same "Default__" strip + unnamed fallback as BuildEntitySnapshot, so
        // roster-fed surfaces read identically to the deep tier.
        Entry.DebugName.RemoveFromStart(TEXT("Default__"));
        if (Entry.DebugName.IsEmpty())
        { Entry.DebugName = TEXT("(unnamed)"); }

        Entry.FrameNumber      = static_cast<int64>(GFrameCounter);
        Entry.WorldTimeSeconds = (InWorld != nullptr) ? InWorld->GetTimeSeconds() : 0.0;

        auto MutableOwner = InEntityHandle;

        const auto AppendTopLevelPlanner = [&Entry](FCk_Handle_Goap_Planner InPlanner)
        {
            if (NOT ck::IsValid(InPlanner)) { return; }
            Entry.Planners.Add(BuildRosterPlannerRow(InPlanner));
        };

        AppendTopLevelPlanner(UCk_Utils_Goap_Planner_UE::Cast(MutableOwner));

        ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
            MutableOwner, AppendTopLevelPlanner);

        return Entry;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Event detection — diff prev vs current ROSTER, push deltas into the
    // per-entity history ring. This is the SINGLE event producer.
    // ----------------------------------------------------------------------------------------------------------------

    // Fallback display name for a chain step whose class name did not resolve.
    // Mirrors the label branch of the retired ResolveActionDisplayName: the
    // action entity's gameplay-label tag (e.g. "Bb.NpcAction.PickGenreShelf").
    static auto
    ResolveActionLabelName(
        const FCk_Handle_Goap_Action& InAction) -> FString
    {
        if (NOT ck::IsValid(InAction)) { return FString{}; }

        if (UCk_Utils_GameplayLabel_UE::Has(InAction))
        {
            const auto Label = UCk_Utils_GameplayLabel_UE::Get_Label(InAction);
            if (Label.IsValid()) { return Label.ToString(); }
        }
        return FString{};
    }

    static auto
    DetectAndPushEvents(
        const FCk_Handle& InEntityHandle,
        const FCkGoapDebugger_RosterEntry& InCurrent,
        const FCkGoapDebugger_RosterEntry* InPrev,
        const FCkGoapDebugger_EntitySnapshot* InSelectedFull,
        double InWorldTime,
        int64  InFrame) -> void
    {
        // Rich SnapshotAtEvent copies exist ONLY for the entity the user has
        // selected — InSelectedFull is that entity's deep snapshot, or null.
        // Every other agent's events carry a null snapshot and scrub as
        // metadata-only rows (declared degradation, see CLAUDE.md).
        const auto SnapshotForPlanner =
            [InSelectedFull](const FCk_Handle_Goap_Planner& InTopLevel)
                -> TSharedPtr<FCkGoapDebugger_ActionSetInfo>
            {
                if (InSelectedFull == nullptr) { return nullptr; }

                const auto* Found = InSelectedFull->ActionSets.FindByPredicate(
                    [&InTopLevel](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == InTopLevel; });
                if (Found == nullptr) { return nullptr; }

                return MakeShared<FCkGoapDebugger_ActionSetInfo>(*Found);
            };

        // Chain-step display name by index — read out of the roster row that
        // recorded the handle, never off the handle itself (a deactivated step
        // may already be destroyed).
        const auto StepName =
            [](const FCkGoapDebugger_RosterPlannerRow& InRow, int32 InIndex,
               const FCk_Handle_Goap_Action& InStep) -> FString
            {
                if (InRow.ChainStepClassNames.IsValidIndex(InIndex) &&
                    NOT InRow.ChainStepClassNames[InIndex].IsEmpty())
                { return InRow.ChainStepClassNames[InIndex]; }

                return ResolveActionLabelName(InStep);
            };

        // Sibling top-level Planners can share one WorldState, so the same ring
        // entry surfaces on several rows — dedup by (key, frame) as before.
        auto SeenWsChanges = TSet<TPair<FGameplayTag, int64>>{};

        for (const auto& CurRow : InCurrent.Planners)
        {
            const auto* PrevRow = (InPrev != nullptr)
                ? InPrev->Planners.FindByPredicate(
                      [&CurRow](const FCkGoapDebugger_RosterPlannerRow& In)
                      { return In.PlannerHandle == CurRow.PlannerHandle; })
                : nullptr;

            // ---- Enable toggle flip ----------------------------------------
            if (PrevRow != nullptr && PrevRow->EnableToggle != CurRow.EnableToggle)
            {
                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind = (CurRow.EnableToggle == ECk_EnableDisable::Enable)
                    ? ECkGoapDebugger_HistoryEventKind::ActionSetEnabled
                    : ECkGoapDebugger_HistoryEventKind::ActionSetDisabled;
                Event.ActionSetHandle  = CurRow.PlannerHandle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.Title            = (Event.Kind == ECkGoapDebugger_HistoryEventKind::ActionSetEnabled)
                    ? FString::Printf(TEXT("Enabled: %s"), *CurRow.DisplayName)
                    : FString::Printf(TEXT("Disabled: %s"), *CurRow.DisplayName);
                Event.SnapshotAtEvent  = SnapshotForPlanner(CurRow.PlannerHandle);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ---- Chain delta ---------------------------------------------------
            const auto& CurChain  = CurRow.ChainStepHandles;
            const auto  EmptyChain = TArray<FCk_Handle_Goap_Action>{};
            const auto& PrevChain = (PrevRow != nullptr) ? PrevRow->ChainStepHandles : EmptyChain;

            // Activated: in cur, not in prev.
            for (auto Idx = 0; Idx < CurChain.Num(); ++Idx)
            {
                const auto& CurEntry = CurChain[Idx];
                if (PrevChain.Contains(CurEntry)) { continue; }

                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ActionActivated;
                Event.ActionSetHandle  = CurRow.PlannerHandle;
                Event.ActionHandle     = CurEntry;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;

                Event.ActionClassName = StepName(CurRow, Idx, CurEntry);
                Event.Title = NOT Event.ActionClassName.IsEmpty()
                    ? FString::Printf(TEXT("Activated: %s"), *Event.ActionClassName)
                    : FString::Printf(TEXT("Activated: <action %s>"), *CurEntry.ToString());

                Event.SnapshotAtEvent = SnapshotForPlanner(CurRow.PlannerHandle);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // Deactivated: in prev, not in cur.
            for (auto Idx = 0; Idx < PrevChain.Num(); ++Idx)
            {
                const auto& PrevEntry = PrevChain[Idx];
                if (CurChain.Contains(PrevEntry)) { continue; }

                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ActionDeactivated;
                Event.ActionSetHandle  = CurRow.PlannerHandle;
                Event.ActionHandle     = PrevEntry;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;

                Event.ActionClassName = (PrevRow != nullptr)
                    ? StepName(*PrevRow, Idx, PrevEntry)
                    : FString{};
                Event.Title = NOT Event.ActionClassName.IsEmpty()
                    ? FString::Printf(TEXT("Deactivated: %s"), *Event.ActionClassName)
                    : FString::Printf(TEXT("Deactivated: <action %s>"), *PrevEntry.ToString());

                Event.SnapshotAtEvent = SnapshotForPlanner(CurRow.PlannerHandle);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ChainReset: prev had >=1, cur has exactly 0.
            if (PrevRow != nullptr && PrevChain.Num() > 0 && CurChain.Num() == 0)
            {
                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ChainReset;
                Event.ActionSetHandle  = CurRow.PlannerHandle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.Title            = FString::Printf(TEXT("Chain reset: %s"), *CurRow.DisplayName);
                Event.SnapshotAtEvent  = SnapshotForPlanner(CurRow.PlannerHandle);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ---- Plan status transition (top-level Planner) --------------------
            if (PrevRow != nullptr && PrevRow->PlanStatus != CurRow.PlanStatus)
            {
                const auto IsPlanFoundTransition =
                    (PrevRow->PlanStatus == ECk_GoapPlanStatus::Idle ||
                     PrevRow->PlanStatus == ECk_GoapPlanStatus::Planning) &&
                    CurRow.PlanStatus == ECk_GoapPlanStatus::PlanFound;

                const auto IsPlanFailedTransition =
                    CurRow.PlanStatus == ECk_GoapPlanStatus::PlanFailed &&
                    PrevRow->PlanStatus != ECk_GoapPlanStatus::PlanFailed;

                if (IsPlanFoundTransition || IsPlanFailedTransition)
                {
                    auto Event = FCkGoapDebugger_HistoryEvent{};
                    Event.Kind = IsPlanFoundTransition
                        ? ECkGoapDebugger_HistoryEventKind::PlanFound
                        : ECkGoapDebugger_HistoryEventKind::PlanFailed;
                    Event.ActionSetHandle  = CurRow.PlannerHandle;
                    Event.WorldTimeSeconds = InWorldTime;
                    Event.FrameNumber      = InFrame;
                    Event.ActionClassName  = CurRow.ChainStepClassNames.IsValidIndex(0)
                        ? CurRow.ChainStepClassNames[0]
                        : FString{};

                    const auto& EventSubject = NOT Event.ActionClassName.IsEmpty()
                        ? Event.ActionClassName
                        : CurRow.DisplayName;

                    Event.Title = IsPlanFoundTransition
                        ? FString::Printf(TEXT("Plan found: %s"), *EventSubject)
                        : FString::Printf(TEXT("Plan failed: %s"), *EventSubject);

                    if (IsPlanFoundTransition)
                    {
                        Event.Meta = FString::Printf(TEXT("cost=%.2f, steps=%d"),
                            CurRow.PlanCost, CurRow.ChainStepClassNames.Num());
                    }

                    Event.SnapshotAtEvent = SnapshotForPlanner(CurRow.PlannerHandle);
                    PushHistoryEvent(InEntityHandle, MoveTemp(Event));
                }
            }

            // ---- Replanned — attempt counter advanced --------------------------
            if (PrevRow != nullptr && CurRow.PlanAttemptCount > PrevRow->PlanAttemptCount)
            {
                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::Replanned;
                Event.ActionSetHandle  = CurRow.PlannerHandle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.CauseAtEvent     = CurRow.LastReplanCause;

                const auto ChangedCount = CurRow.LastReplanCause.Get_ChangedKeys().Num();
                Event.Title = FString::Printf(TEXT("Replan #%d: %s"),
                    CurRow.PlanAttemptCount, *CurRow.DisplayName);
                Event.Meta = ChangedCount > 1
                    ? FString::Printf(TEXT("%d changes coalesced"), ChangedCount)
                    : FString{};

                Event.SnapshotAtEvent = SnapshotForPlanner(CurRow.PlannerHandle);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ---- World-state changes since the previous roster frame -----------
            // Bounded ring read off the resolved WS handle (capacity 32) — NOT
            // the override-stack key scan BuildWorldStateEntries performs.
            if (ck::IsValid(CurRow.WorldStateHandle) &&
                CurRow.WorldStateHandle.Has<ck::FFragment_Goap_WorldState_ChangeLog>())
            {
                const auto PrevFrame = (InPrev != nullptr) ? InPrev->FrameNumber : 0;
                const auto& Changes =
                    CurRow.WorldStateHandle.Get<ck::FFragment_Goap_WorldState_ChangeLog>().Get_Entries();

                for (const auto& Change : Changes)
                {
                    if (Change.Get_FrameNumber() <= PrevFrame) { continue; }

                    const auto DedupKey = TPair<FGameplayTag, int64>{Change.Get_Key(), Change.Get_FrameNumber()};
                    if (SeenWsChanges.Contains(DedupKey)) { continue; }
                    SeenWsChanges.Add(DedupKey);

                    auto Event = FCkGoapDebugger_HistoryEvent{};
                    Event.Kind             = ECkGoapDebugger_HistoryEventKind::WorldStateChanged;
                    Event.ActionSetHandle  = CurRow.PlannerHandle;
                    Event.WorldTimeSeconds = InWorldTime;
                    Event.FrameNumber      = Change.Get_FrameNumber();
                    Event.WorldStateChangeAtEvent = Change;
                    Event.Title = FString::Printf(TEXT("%s -> %s"),
                        *Change.Get_Key().ToString(),
                        Change.Get_NewValue() ? TEXT("TRUE") : TEXT("FALSE"));

                    Event.SnapshotAtEvent = SnapshotForPlanner(CurRow.PlannerHandle);
                    PushHistoryEvent(InEntityHandle, MoveTemp(Event));
                }
            }
        }
    }
} // namespace ck_goap_debugger_data_collector_internal

// ====================================================================================================================
// LIFECYCLE
// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    Initialize()
    -> void
{
    using namespace ck_goap_debugger_data_collector_internal;

    GSessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddLambda([]()
    {
        ClearAllCaches();
    });
}

auto
    FCkGoapDebugger_DataCollector::
    Shutdown()
    -> void
{
    using namespace ck_goap_debugger_data_collector_internal;

    if (GSessionInvalidatedHandle.IsValid())
    {
        ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(GSessionInvalidatedHandle);
        GSessionInvalidatedHandle.Reset();
    }

    ClearAllCaches();
}

auto FCkGoapDebugger_DataCollector::Reset_ForWorldChange() -> void
{
    ck_goap_debugger_data_collector_internal::ClearAllCaches();
}

// ====================================================================================================================
// COLLECT
// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    CollectRoster(
        UWorld* InWorld,
        const FCkGoapDebugger_EntitySnapshot* InSelectedFull)
    -> TArray<FCkGoapDebugger_RosterEntry>
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkGoapDebugger_CollectRoster);

    using namespace ck_goap_debugger_data_collector_internal;

    auto Out = TArray<FCkGoapDebugger_RosterEntry>{};

    if (NOT ck::IsValid(InWorld, ck::IsValid_Policy_NullptrOnly{})) { return Out; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (NOT ck::IsValid(TransientEntity)) { return Out; }

    // Which entities we saw this pass, so the prev-roster map can be pruned of
    // entries whose entities have been destroyed.
    auto SeenThisPass = TSet<FCk_Handle>{};

    // Planner entities registered in SOME owner's record (Create-style children).
    // Collected during the record pass so the Planner-role pass below can tell
    // an Add-style owner (roster it) from a Create-style child (already covered
    // by its owning entity's entry).
    auto RecordRegisteredPlanners = TSet<FCk_Handle>{};

    const auto RosterOwner = [&Out, &SeenThisPass, InWorld, InSelectedFull](
        const FCk_Handle& InOwnerHandle)
    {
        const auto* PrevEntry = GPrevRosterByEntity.Find(InOwnerHandle);

        auto Entry = BuildRosterEntry(InOwnerHandle, InWorld);

        // The roster pass is the SINGLE event producer for every agent.
        DetectAndPushEvents(
            InOwnerHandle,
            Entry,
            PrevEntry,
            InSelectedFull,
            Entry.WorldTimeSeconds,
            Entry.FrameNumber);

        SeenThisPass.Add(InOwnerHandle);
        GPrevRosterByEntity.Add(InOwnerHandle, Entry);

        Out.Add(MoveTemp(Entry));
    };

    // Create-style owners — an owner entity holds FFragment_RecordOfGoapPlanners
    // whose entries are the typesafe FCk_Handle_Goap_Planner sub-entities.
    TransientEntity.View<ck::FFragment_RecordOfGoapPlanners>().ForEach(
        [&RosterOwner, &RecordRegisteredPlanners, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_RecordOfGoapPlanners&)
        {
            const auto OwnerHandle = ck::MakeHandle(InEntity, TransientEntity);
            if (NOT ck::IsValid(OwnerHandle)) { return; }

            auto MutableOwner = OwnerHandle;
            ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
                MutableOwner,
                [&RecordRegisteredPlanners](FCk_Handle_Goap_Planner InPlanner)
                {
                    if (NOT ck::IsValid(InPlanner)) { return; }
                    RecordRegisteredPlanners.Add(static_cast<FCk_Handle>(InPlanner));
                });

            RosterOwner(OwnerHandle);
        });

    // Add-style owners: the Planner role is stamped directly onto the owning
    // entity and the Add path never writes a record entry, so the record view
    // above cannot see these agents (every gym but Survival installs this way).
    // Walk every Planner-role entity and roster the ones that are neither
    // sub-nodes (Action role => leaf or promoted mid-tier under some Planner)
    // nor Create-style children (already covered by their owner's entry).
    TransientEntity.View<ck::FFragment_Goap_Planner_Params>().ForEach(
        [&RosterOwner, &SeenThisPass, &RecordRegisteredPlanners, &TransientEntity](
            FCk_Entity InEntity, const ck::FFragment_Goap_Planner_Params&)
        {
            const auto OwnerHandle = ck::MakeHandle(InEntity, TransientEntity);
            if (NOT ck::IsValid(OwnerHandle)) { return; }

            if (SeenThisPass.Contains(OwnerHandle)) { return; }
            if (RecordRegisteredPlanners.Contains(OwnerHandle)) { return; }
            if (OwnerHandle.Has<ck::FFragment_Goap_Action_Definition>()) { return; }

            RosterOwner(OwnerHandle);
        });

    // Prune entries for entities that no longer exist (destroyed since last tick).
    auto KeysToRemove = TArray<FCk_Handle>{};
    for (const auto& KvPair : GPrevRosterByEntity)
    {
        if (NOT SeenThisPass.Contains(KvPair.Key))
        {
            KeysToRemove.Add(KvPair.Key);
        }
    }
    for (const auto& Key : KeysToRemove)
    {
        GPrevRosterByEntity.Remove(Key);
        // History intentionally preserved — user may want to scrub through
        // events for a recently-destroyed entity. PIE teardown clears it.
    }

    return Out;
}

auto
    FCkGoapDebugger_DataCollector::
    CollectFull(
        UWorld* InWorld,
        const FCk_Handle& InEntity)
    -> TOptional<FCkGoapDebugger_EntitySnapshot>
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkGoapDebugger_CollectFull);

    using namespace ck_goap_debugger_data_collector_internal;

    if (NOT ck::IsValid(InWorld, ck::IsValid_Policy_NullptrOnly{})) { return {}; }
    if (NOT ck::IsValid(InEntity)) { return {}; }

    // GoapHandle is the first valid Planner on the owner — self-role first
    // (Add), then record entries (Create) — kept as a legacy field on the
    // snapshot for widgets that still rely on it as a stable per-entity
    // identifier. Mirrors BuildEntitySnapshot's TopLevelPlanners ordering.
    auto MutableOwner       = InEntity;
    auto FirstPlannerHandle = UCk_Utils_Goap_Planner_UE::Cast(MutableOwner);

    ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
        MutableOwner,
        [&FirstPlannerHandle](FCk_Handle_Goap_Planner InPlanner)
        {
            if (NOT ck::IsValid(InPlanner)) { return; }
            if (NOT ck::IsValid(FirstPlannerHandle)) { FirstPlannerHandle = InPlanner; }
        });

    if (NOT ck::IsValid(FirstPlannerHandle)) { return {}; }

    // The WS recently-changed markers need the PREVIOUS deep snapshot of this
    // same entity; a selection change restarts them (markers fade in 30 frames).
    const auto* PrevSnapshot =
        (GPrevFullSelected.IsSet() && GPrevFullSelected->EntityHandle == InEntity)
            ? &GPrevFullSelected.GetValue()
            : nullptr;

    auto Snapshot = BuildEntitySnapshot(InEntity, FirstPlannerHandle, InWorld, PrevSnapshot);

    GPrevFullSelected = Snapshot;

    return Snapshot;
}
// ====================================================================================================================
// HISTORY
// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    GetHistory(
        const FCk_Handle& InEntityHandle)
    -> const TArray<FCkGoapDebugger_HistoryEvent>&
{
    using namespace ck_goap_debugger_data_collector_internal;

    if (const auto* Found = GHistoryByEntity.Find(InEntityHandle))
    {
        return *Found;
    }
    return GEmptyHistory;
}

auto
    FCkGoapDebugger_DataCollector::
    ClearHistory()
    -> void
{
    using namespace ck_goap_debugger_data_collector_internal;
    GHistoryByEntity.Reset();
}

auto
    FCkGoapDebugger_DataCollector::
    ClearHistoryForEntity(
        const FCk_Handle& InEntityHandle)
    -> void
{
    using namespace ck_goap_debugger_data_collector_internal;
    GHistoryByEntity.Remove(InEntityHandle);
}

// ====================================================================================================================
