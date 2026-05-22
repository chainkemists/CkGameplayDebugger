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
    // ActionSet world-state snapshot — reads the resolved WS handle's registry
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
        const auto& ValFrag = InWsHandle.Get<ck::FFragment_Goap_WorldState_Values>();

        const auto& Registry = RegFrag.Get_Registry();
        const auto& Values   = ValFrag.Get_Values();

        const auto& AllTags = Registry.GetAllTags();
        Out.Reserve(AllTags.Num());

        for (auto Index = 0; Index < AllTags.Num(); ++Index)
        {
            auto Entry = FCkGoapDebugger_WorldStateEntry{};
            Entry.Key   = AllTags[Index];
            Entry.Value = Values.Get(Index);

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
        }

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
    // Per-ActionSet snapshot.
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
        if (InActionSetHandle.Has<ck::FFragment_Goap_Planner_ActiveChain>())
        {
            const auto& Chain = InActionSetHandle.Get<ck::FFragment_Goap_Planner_ActiveChain>();
            Info.ActiveChainHandles = Chain.Get_Chain();
        }

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

        // ---- Catalog walk: gather all Action entities in the ActionSet --------------
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
    // gather every ActionSet entity.
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

        // Enumerate ActionSets from the private record-of-actionsets.
        auto MutableGoap = InGoapHandle;
        ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
            MutableGoap,
            [&Snapshot, InPrevSnapshot](FCk_Handle_Goap_Planner InPlanner)
            {
                if (NOT ck::IsValid(InPlanner)) { return; }

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

    // The Goap root entity holds FFragment_RecordOfGoapPlanners. Iterate
    // those — the OWNER of each Goap root is the gameplay entity the debugger
    // surfaces; the Goap root itself is a typesafe child entity.
    TransientEntity.View<ck::FFragment_RecordOfGoapPlanners>().ForEach(
        [&Out, &SeenThisPass, &TransientEntity, InWorld](FCk_Entity InEntity, const ck::FFragment_RecordOfGoapPlanners&)
        {
            const auto GoapEntityHandle = ck::MakeHandle(InEntity, TransientEntity);
            if (NOT ck::IsValid(GoapEntityHandle)) { return; }

            // Resolve the owner entity — that's the gameplay entity we surface.
            // Goap roots are spawned as typesafe children of the owner via
            // Request_CreateEntity_AsTypeSafe<FCk_Handle_Goap_Planner>. The owner is
            // therefore the lifetime owner of this entity. Fall back to the
            // Goap entity itself if no owner is found (defensive).
            auto OwnerHandle = FCk_Handle{};
            if (GoapEntityHandle.Has<ck::FFragment_LifetimeOwner>())
            {
                OwnerHandle = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(GoapEntityHandle);
            }
            if (NOT ck::IsValid(OwnerHandle)) { OwnerHandle = GoapEntityHandle; }

            const auto GoapTypedHandle = UCk_Utils_Goap_Planner_UE::CastChecked(GoapEntityHandle);

            const auto* PrevSnapshot = GPrevSnapshotByEntity.Find(OwnerHandle);

            auto Snapshot = BuildEntitySnapshot(OwnerHandle, GoapTypedHandle, InWorld, PrevSnapshot);

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
