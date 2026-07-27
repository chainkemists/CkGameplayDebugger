#include "CkAggroDebugger/Data/CkAggroDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Time/CkTime_Utils.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggro_Utils.h"
#include "CkAggro/CkAggroTarget_Fragment.h"
#include "CkAggro/CkAggroTarget_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_aggro_debugger_collector
{
    // Mirrors ck_aggro_processor::Get_Now so the "seconds since…" figures line up with what the processors act on.
    // Reading the world clock directly (rather than accumulating Slate delta time) is what keeps the numbers honest
    // while PIE is paused.
    auto Get_Now(
        const FCk_Handle& InHandle)
        -> FCk_Time
    {
        const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        const auto TimeParams = FCk_Utils_Time_GetWorldTime_Params{World};
        return UCk_Utils_Time_UE::Get_WorldTime(TimeParams).Get_WorldTime().Get_Time();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAggroDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Snapshot = FCkAggroDebugger_Snapshot{};

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    _Snapshot.HasWorld = true;

    TransientEntity.View<ck::FFragment_Aggro_Current>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_Aggro_Current& InCurrent)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            const auto Aggro = UCk_Utils_Aggro_UE::Cast(Handle);
            if (ck::Is_NOT_Valid(Aggro))
            { return; }

            const auto Now = ck_aggro_debugger_collector::Get_Now(Handle);

            auto Info = FCkAggroDebugger_OwnerInfo{};
            Info.OwnerEntity = Handle;
            Info.OwnerName   = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();

            Info.IsDisabled         = NOT UCk_Utils_Aggro_UE::Get_IsEnabled(Aggro);
            Info.IsSelectionPending = Handle.Has<ck::FTag_Aggro_SelectionPending>();
            Info.EvaluationCount    = UCk_Utils_Aggro_UE::Get_Debug_EvaluationCount(Aggro);

            Info.ActiveTrackedEntity = UCk_Utils_Aggro_UE::TryGet_ActiveTrackedEntity(Aggro);
            Info.ActiveTrackedName   = ck::IsValid(Info.ActiveTrackedEntity)
                ? UCk_Utils_Handle_UE::Get_DebugName(Info.ActiveTrackedEntity).ToString()
                : FString{};

            Info.SecondsSinceSwitch      = static_cast<float>((Now - InCurrent.Get_LastSwitchTime()).Get_Seconds());
            Info.SecondsActiveTargetHeld = static_cast<float>((Now - InCurrent.Get_ActiveTargetStartTime()).Get_Seconds());

            if (Handle.Has<ck::FFragment_Aggro_SelectionParams>())
            {
                const auto& Selection = Handle.Get<ck::FFragment_Aggro_SelectionParams>();
                Info.CurrentTargetBias    = Selection.Get_CurrentTargetBias();
                Info.SwitchThreshold      = Selection.Get_TargetSwitchThreshold();
                Info.MinimumTargetScore   = Selection.Get_MinimumTargetScore();
                Info.SwitchCooldown       = static_cast<float>(Selection.Get_TargetSwitchCooldown().Get_Seconds());
                Info.MinimumAggroDuration = static_cast<float>(Selection.Get_MinimumAggroDuration().Get_Seconds());
            }

            if (Handle.Has<ck::FFragment_Aggro_CapParams>())
            {
                const auto& Cap = Handle.Get<ck::FFragment_Aggro_CapParams>();
                Info.MaxTrackedTargets = Cap.Get_TargetCapMode() == ECk_EnableDisable::Enable
                    ? Cap.Get_MaximumTrackedTargets()
                    : 0;
            }

            UCk_Utils_Aggro_UE::ForEach_Target(Aggro,
                [&Info, Now](FCk_Handle_AggroTarget InTarget)
                {
                    if (ck::Is_NOT_Valid(InTarget))
                    { return; }

                    auto Target = FCkAggroDebugger_TargetInfo{};
                    Target.TargetEntity  = InTarget;
                    Target.TrackedEntity = UCk_Utils_AggroTarget_UE::Get_TrackedEntity(InTarget);
                    Target.TrackedName   = ck::IsValid(Target.TrackedEntity)
                        ? UCk_Utils_Handle_UE::Get_DebugName(Target.TrackedEntity).ToString()
                        : FString(TEXT("(destroyed)"));

                    Target.Threat   = UCk_Utils_AggroTarget_UE::Get_Threat(InTarget);
                    Target.Score    = UCk_Utils_AggroTarget_UE::Get_Score(InTarget);
                    Target.Distance = UCk_Utils_AggroTarget_UE::Get_Distance(InTarget);

                    Target.IsActive          = UCk_Utils_AggroTarget_UE::Get_IsActiveTarget(InTarget);
                    Target.IsPerceived       = UCk_Utils_AggroTarget_UE::Get_IsPerceived(InTarget);
                    Target.IsWithinRetention = UCk_Utils_AggroTarget_UE::Get_IsWithinRetention(InTarget);

                    // No public getter for these — they are pure debug state, so read the tags directly.
                    Target.IsPendingForget    = InTarget.Has<ck::FTag_AggroTarget_PendingForget>();
                    Target.CannotBecomeActive = InTarget.Has<ck::FTag_AggroTarget_CannotBecomeActive>();
                    Target.CannotBeForgotten  = InTarget.Has<ck::FTag_AggroTarget_CannotBeForgotten>();

                    if (InTarget.Has<ck::FFragment_AggroTarget_ThreatParams>())
                    {
                        const auto& Threat = InTarget.Get<ck::FFragment_AggroTarget_ThreatParams>();
                        Target.ThreatDecayRate      = Threat.Get_ThreatDecayRate();
                        Target.UnperceivedDecayMult = Threat.Get_UnperceivedThreatDecayMultiplier();
                        Target.MinimumTrackedThreat = Threat.Get_MinimumTrackedThreat();
                    }

                    if (InTarget.Has<ck::FFragment_AggroTarget_SpatialParams>())
                    { Target.RetentionDistance = InTarget.Get<ck::FFragment_AggroTarget_SpatialParams>().Get_RetentionDistance(); }

                    if (InTarget.Has<ck::FFragment_AggroTarget_ForgetParams>())
                    {
                        Target.ForgetDurationSeconds = static_cast<float>(
                            InTarget.Get<ck::FFragment_AggroTarget_ForgetParams>().Get_ForgetDuration().Get_Seconds());
                    }

                    if (InTarget.Has<ck::FFragment_AggroTarget_Perception>())
                    {
                        const auto LastPerceived = InTarget.Get<ck::FFragment_AggroTarget_Perception>().Get_LastPerceivedTime();
                        Target.SecondsSincePerceived = static_cast<float>((Now - LastPerceived).Get_Seconds());
                    }

                    Info.Targets.Add(Target);
                });

            // Score-descending: selection is an argmax over score, so the top row is the one that would win if every
            // hysteresis gate were open. Reading the table top-down then matches how selection reasons about it.
            Info.Targets.Sort([](const FCkAggroDebugger_TargetInfo& InA, const FCkAggroDebugger_TargetInfo& InB)
            {
                return InA.Score > InB.Score;
            });

            _Snapshot.NumTargets += Info.Targets.Num();
            if (ck::IsValid(Info.ActiveTrackedEntity))
            { ++_Snapshot.NumEngaged; }

            _Snapshot.Owners.Add(Info);
        });

    _Snapshot.NumOwners = _Snapshot.Owners.Num();

    // Engaged owners first — in a crowd of idle NPCs the two that are actually fighting are the ones worth seeing,
    // and scrolling for them defeats the point of the window.
    _Snapshot.Owners.Sort([](const FCkAggroDebugger_OwnerInfo& InA, const FCkAggroDebugger_OwnerInfo& InB)
    {
        const auto AEngaged = ck::IsValid(InA.ActiveTrackedEntity);
        const auto BEngaged = ck::IsValid(InB.ActiveTrackedEntity);

        if (AEngaged != BEngaged)
        { return AEngaged; }

        if (InA.Targets.Num() != InB.Targets.Num())
        { return InA.Targets.Num() > InB.Targets.Num(); }

        return InA.OwnerName < InB.OwnerName;
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAggroDebugger_DataCollector::
    Reset()
    -> void
{
    _Snapshot = FCkAggroDebugger_Snapshot{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAggroDebugger_DataCollector::
    Get_Snapshot() const
    -> const FCkAggroDebugger_Snapshot&
{
    return _Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
