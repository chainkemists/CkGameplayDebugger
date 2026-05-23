#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkGoap/CkGoap_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"
#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/Planner/CkGoap_Planner_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Record_Internal.h"  // FFragment_RecordOfGoapPlanners + utils
#include "CkGoap/Action/CkGoap_Action_Fragment.h"
#include "CkGoap/Action/CkGoap_Action_Record_Internal.h"        // FFragment_RecordOfGoapActions + utils
#include "CkGoap/WorldState/CkGoap_WorldState_Fragment.h"
#include "CkGoap/WorldState/CkGoap_WorldState_Utils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Engine/World.h"

// ====================================================================================================================
// FILE-STATIC STATE — singleton storage.
// ====================================================================================================================

namespace ck_goap_debugger_data_collector_internal
{
    // Per-entity history ring buffers. Keyed by the owning entity handle
    // (the one that holds the Goap root, not the Goap root itself).
    static TMap<FCk_Handle, TArray<FCkGoapDebugger_HistoryEvent>> GHistoryByEntity;

    // Previous-tick snapshot per entity — used for diff-based event detection
    // and recently-changed-WS markup. Cleared on PIE start/stop.
    static TMap<FCk_Handle, FCkGoapDebugger_EntitySnapshot> GPrevSnapshotByEntity;

    // Maximum events retained per entity. Older events drop off the front.
    constexpr int32 GMaxHistoryPerEntity = 256;

    // After this many frames a WS key value-change marker fades.
    constexpr int64 GRecentlyChangedFrameWindow = 30;

    // Empty sentinel — returned by GetHistory when the entity has no entries.
    static const TArray<FCkGoapDebugger_HistoryEvent> GEmptyHistory{};

#if WITH_EDITOR
    static FDelegateHandle GBeginPieHandle;
    static FDelegateHandle GEndPieHandle;
#endif

    // ----------------------------------------------------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    ClearAllCaches() -> void
    {
        GHistoryByEntity.Reset();
        GPrevSnapshotByEntity.Reset();
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
        if (InActionHandle.Has<ck::FFragment_Goap_Action_ReplanThrottle>())
        {
            const auto& Throttle = InActionHandle.Get<ck::FFragment_Goap_Action_ReplanThrottle>();
            Info.SecondsSinceLastReplan = Throttle.Get_SecondsSinceLastReplan();
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
    BuildPlannerInfo(
        const FCk_Handle_Goap_Planner& InPlannerHandle,
        const FCk_Handle_Goap_Planner& InParentPlannerHandle,
        const TArray<FCk_Handle_Goap_Action>& InActiveChainHandles,
        int32 InActiveChainDepthHint) -> FCkGoapDebugger_PlannerInfo
    {
        // Compatibility wrapper used by older callers that don't pass WS prev /
        // depth map. WS-recently-changed markup falls through unset; sub-Planner
        // recursion uses an empty depth map (chain membership only on top-level).
        auto DepthByHandle = TMap<FCk_Handle_Goap_Action, int32>{};
        for (auto Idx = 0; Idx < InActiveChainHandles.Num(); ++Idx)
        { DepthByHandle.Add(InActiveChainHandles[Idx], Idx); }

        auto Visited = TSet<FCk_Handle_Goap_Planner>{};
        return BuildPlannerInfo_Recursive(
            InPlannerHandle,
            InParentPlannerHandle,
            InActiveChainHandles,
            DepthByHandle,
            /*InPrevSnapshot=*/ nullptr,
            /*InCurrentFrame=*/ 0,
            Visited);
        (void)InActiveChainDepthHint;
    }

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

        // ---- Plan + goal (Planner role) --------------------------------------------
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_PlanState>())
        {
            const auto& PlanState = InPlannerHandle.Get<ck::FFragment_Goap_Planner_PlanState>();
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

        // ---- WS source -------------------------------------------------------------
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_WorldStateSource>())
        {
            const auto& WSSource = InPlannerHandle.Get<ck::FFragment_Goap_Planner_WorldStateSource>();
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

        // ---- Goal ------------------------------------------------------------------
        if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_Goal>())
        {
            const auto& GoalFrag = InPlannerHandle.Get<ck::FFragment_Goap_Planner_Goal>();
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
            else if (InPlannerHandle.Has<ck::FFragment_Goap_Planner_Current>())
            {
                const auto& Current = InPlannerHandle.Get<ck::FFragment_Goap_Planner_Current>();
                const auto Root = Current.Get_RootAction();
                if (ck::IsValid(Root) && Root.Has<ck::FFragment_Goap_Action_Tree>())
                {
                    const auto& RootTree = Root.Get<ck::FFragment_Goap_Action_Tree>();
                    DirectChildren = RootTree.Get_ChildActions();
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

        // ---- Current (enable toggle, dependency cycles, root) ----------------------
        if (InActionSetHandle.Has<ck::FFragment_Goap_Planner_Current>())
        {
            const auto& Current = InActionSetHandle.Get<ck::FFragment_Goap_Planner_Current>();
            Info.EnableToggle    = Current.Get_EnableToggle();
            Info.RootActionHandle = Current.Get_RootAction();

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

        if (Snapshot.DebugName.IsEmpty())
        {
            Snapshot.DebugName = TEXT("(unnamed)");
        }

        Snapshot.FrameNumber      = static_cast<int64>(GFrameCounter);
        Snapshot.WorldTimeSeconds = (InWorld != nullptr) ? InWorld->GetTimeSeconds() : 0.0;

        // Enumerate Planners from the private record-of-planners.
        // Each top-level Planner contributes ONE PlannerInfo (new shape) AND ONE
        // legacy ActionSetInfo (shim).
        auto MutableOwner = InEntityHandle;
        ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
            MutableOwner,
            [&Snapshot, InPrevSnapshot](FCk_Handle_Goap_Planner InPlanner)
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
            });

        return Snapshot;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Event detection — diff prev vs current snapshot, push deltas into the
    // per-entity history ring.
    // ----------------------------------------------------------------------------------------------------------------

    static auto
    DetectAndPushEvents(
        const FCk_Handle& InEntityHandle,
        const FCkGoapDebugger_EntitySnapshot& InCurrent,
        const FCkGoapDebugger_EntitySnapshot* InPrev,
        double InWorldTime,
        int64  InFrame) -> void
    {
        for (const auto& CurAs : InCurrent.ActionSets)
        {
            const auto* PrevAs = (InPrev != nullptr)
                ? InPrev->ActionSets.FindByPredicate(
                      [&CurAs](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == CurAs.Handle; })
                : nullptr;

            // ---- Enable toggle flip ----------------------------------------
            if (PrevAs != nullptr && PrevAs->EnableToggle != CurAs.EnableToggle)
            {
                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind = (CurAs.EnableToggle == ECk_EnableDisable::Enable)
                    ? ECkGoapDebugger_HistoryEventKind::ActionSetEnabled
                    : ECkGoapDebugger_HistoryEventKind::ActionSetDisabled;
                Event.ActionSetHandle  = CurAs.Handle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.Title            = (Event.Kind == ECkGoapDebugger_HistoryEventKind::ActionSetEnabled)
                    ? FString::Printf(TEXT("Enabled: %s"), *CurAs.DebugName)
                    : FString::Printf(TEXT("Disabled: %s"), *CurAs.DebugName);
                Event.SnapshotAtEvent  = MakeShared<FCkGoapDebugger_ActionSetInfo>(CurAs);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ---- Chain delta ---------------------------------------------------
            const auto& CurChain  = CurAs.ActiveChainHandles;
            const auto& PrevChain = (PrevAs != nullptr) ? PrevAs->ActiveChainHandles : TArray<FCk_Handle_Goap_Action>{};

            // Activated: in cur, not in prev.
            for (const auto& CurEntry : CurChain)
            {
                if (PrevChain.Contains(CurEntry)) { continue; }

                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ActionActivated;
                Event.ActionSetHandle  = CurAs.Handle;
                Event.ActionHandle     = CurEntry;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;

                // Title = action class name (looked up in the current catalog).
                const auto* ActionInfo = CurAs.Catalog.FindByPredicate(
                    [&CurEntry](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == CurEntry; });
                Event.Title = (ActionInfo != nullptr && NOT ActionInfo->ClassName.IsEmpty())
                    ? FString::Printf(TEXT("Activated: %s"), *ActionInfo->ClassName)
                    : TEXT("Activated");

                Event.SnapshotAtEvent = MakeShared<FCkGoapDebugger_ActionSetInfo>(CurAs);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // Deactivated: in prev, not in cur.
            for (const auto& PrevEntry : PrevChain)
            {
                if (CurChain.Contains(PrevEntry)) { continue; }

                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ActionDeactivated;
                Event.ActionSetHandle  = CurAs.Handle;
                Event.ActionHandle     = PrevEntry;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;

                const auto* ActionInfo = (PrevAs != nullptr)
                    ? PrevAs->Catalog.FindByPredicate(
                          [&PrevEntry](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == PrevEntry; })
                    : nullptr;
                Event.Title = (ActionInfo != nullptr && NOT ActionInfo->ClassName.IsEmpty())
                    ? FString::Printf(TEXT("Deactivated: %s"), *ActionInfo->ClassName)
                    : TEXT("Deactivated");

                Event.SnapshotAtEvent = MakeShared<FCkGoapDebugger_ActionSetInfo>(CurAs);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ChainReset: prev had >=1, cur has exactly 0.
            if (PrevAs != nullptr && PrevChain.Num() > 0 && CurChain.Num() == 0)
            {
                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind             = ECkGoapDebugger_HistoryEventKind::ChainReset;
                Event.ActionSetHandle  = CurAs.Handle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.Title            = FString::Printf(TEXT("Chain reset: %s"), *CurAs.DebugName);
                Event.SnapshotAtEvent  = MakeShared<FCkGoapDebugger_ActionSetInfo>(CurAs);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
            }

            // ---- Plan status transitions per Action in the catalog -------------
            for (const auto& CurAction : CurAs.Catalog)
            {
                const auto* PrevAction = (PrevAs != nullptr)
                    ? PrevAs->Catalog.FindByPredicate(
                          [&CurAction](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == CurAction.Handle; })
                    : nullptr;

                if (PrevAction == nullptr) { continue; }
                if (PrevAction->PlanStatus == CurAction.PlanStatus) { continue; }

                auto IsPlanFoundTransition =
                    (PrevAction->PlanStatus == ECk_GoapPlanStatus::Idle ||
                     PrevAction->PlanStatus == ECk_GoapPlanStatus::Planning) &&
                    CurAction.PlanStatus == ECk_GoapPlanStatus::PlanFound;

                auto IsPlanFailedTransition =
                    CurAction.PlanStatus == ECk_GoapPlanStatus::PlanFailed &&
                    PrevAction->PlanStatus != ECk_GoapPlanStatus::PlanFailed;

                if (NOT IsPlanFoundTransition && NOT IsPlanFailedTransition) { continue; }

                auto Event = FCkGoapDebugger_HistoryEvent{};
                Event.Kind = IsPlanFoundTransition
                    ? ECkGoapDebugger_HistoryEventKind::PlanFound
                    : ECkGoapDebugger_HistoryEventKind::PlanFailed;
                Event.ActionSetHandle  = CurAs.Handle;
                Event.ActionHandle     = CurAction.Handle;
                Event.WorldTimeSeconds = InWorldTime;
                Event.FrameNumber      = InFrame;
                Event.Title = IsPlanFoundTransition
                    ? FString::Printf(TEXT("Plan found: %s"), *CurAction.ClassName)
                    : FString::Printf(TEXT("Plan failed: %s"), *CurAction.ClassName);

                if (IsPlanFoundTransition)
                {
                    Event.Meta = FString::Printf(TEXT("cost=%.2f, steps=%d"),
                        CurAction.PlanCost, CurAction.PlanClassNames.Num());
                }

                Event.SnapshotAtEvent = MakeShared<FCkGoapDebugger_ActionSetInfo>(CurAs);
                PushHistoryEvent(InEntityHandle, MoveTemp(Event));
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

#if WITH_EDITOR
    GBeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([](bool /*bIsSimulating*/)
    {
        ClearAllCaches();
    });

    GEndPieHandle = FEditorDelegates::EndPIE.AddLambda([](bool /*bIsSimulating*/)
    {
        ClearAllCaches();
    });
#endif
}

auto
    FCkGoapDebugger_DataCollector::
    Shutdown()
    -> void
{
    using namespace ck_goap_debugger_data_collector_internal;

#if WITH_EDITOR
    if (GBeginPieHandle.IsValid())
    {
        FEditorDelegates::BeginPIE.Remove(GBeginPieHandle);
        GBeginPieHandle.Reset();
    }
    if (GEndPieHandle.IsValid())
    {
        FEditorDelegates::EndPIE.Remove(GEndPieHandle);
        GEndPieHandle.Reset();
    }
#endif

    ClearAllCaches();
}

// ====================================================================================================================
// COLLECT
// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    CollectSnapshots(
        UWorld* InWorld)
    -> TArray<FCkGoapDebugger_EntitySnapshot>
{
    using namespace ck_goap_debugger_data_collector_internal;

    auto Out = TArray<FCkGoapDebugger_EntitySnapshot>{};

    if (NOT ck::IsValid(InWorld, ck::IsValid_Policy_NullptrOnly{})) { return Out; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (NOT ck::IsValid(TransientEntity)) { return Out; }

    // Snapshot map for this pass — keyed by the entity that owns the Goap root.
    // We track which entities we saw so the prev-snapshot map can be pruned of
    // entries whose entities have been destroyed.
    auto SeenThisPass = TSet<FCk_Handle>{};

    // U11.7-A: An owner entity holds FFragment_RecordOfGoapPlanners whose entries
    // are the typesafe FCk_Handle_Goap_Planner sub-entities. Iterate those owners,
    // build a snapshot for the owner with its forest of top-level Planners.
    TransientEntity.View<ck::FFragment_RecordOfGoapPlanners>().ForEach(
        [&Out, &SeenThisPass, &TransientEntity, InWorld](FCk_Entity InEntity, const ck::FFragment_RecordOfGoapPlanners&)
        {
            const auto OwnerHandle = ck::MakeHandle(InEntity, TransientEntity);
            if (NOT ck::IsValid(OwnerHandle)) { return; }

            const auto* PrevSnapshot = GPrevSnapshotByEntity.Find(OwnerHandle);

            // GoapHandle is the first valid Planner in the owner's record — kept
            // as a legacy field on the snapshot for widgets that still rely on
            // it as a stable per-entity identifier.
            auto FirstPlannerHandle = FCk_Handle_Goap_Planner{};
            {
                auto MutableOwner = OwnerHandle;
                ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
                    MutableOwner,
                    [&FirstPlannerHandle](FCk_Handle_Goap_Planner InPlanner)
                    {
                        if (NOT ck::IsValid(FirstPlannerHandle) && ck::IsValid(InPlanner))
                        {
                            FirstPlannerHandle = InPlanner;
                        }
                    });
            }

            auto Snapshot = BuildEntitySnapshot(OwnerHandle, FirstPlannerHandle, InWorld, PrevSnapshot);

            DetectAndPushEvents(
                OwnerHandle,
                Snapshot,
                PrevSnapshot,
                Snapshot.WorldTimeSeconds,
                Snapshot.FrameNumber);

            SeenThisPass.Add(OwnerHandle);
            GPrevSnapshotByEntity.Add(OwnerHandle, Snapshot);

            Out.Add(MoveTemp(Snapshot));
        });

    // Prune entries for entities that no longer exist (destroyed since last tick).
    auto KeysToRemove = TArray<FCk_Handle>{};
    for (const auto& KvPair : GPrevSnapshotByEntity)
    {
        if (NOT SeenThisPass.Contains(KvPair.Key))
        {
            KeysToRemove.Add(KvPair.Key);
        }
    }
    for (const auto& Key : KeysToRemove)
    {
        GPrevSnapshotByEntity.Remove(Key);
        // History intentionally preserved — user may want to scrub through
        // events for a recently-destroyed entity. PIE teardown clears it.
    }

    return Out;
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
