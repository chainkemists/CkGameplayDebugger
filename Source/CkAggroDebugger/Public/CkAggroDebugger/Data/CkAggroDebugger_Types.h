#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Plain view structs collected read-only from the world by FCkAggroDebugger_DataCollector.
//
// Deliberately free of CkAggro types so the window only ever sees PODs: threat/score are already floats, and the
// tag-driven state is flattened to bools at collect time. That keeps the window from having to re-derive anything
// per paint, which matters because the meters repaint every frame.
// --------------------------------------------------------------------------------------------------------------------

// One tracked target inside one owner's threat table.
struct FCkAggroDebugger_TargetInfo
{
    FCk_Handle TargetEntity;   // the AggroTarget child entity (the record member)
    FCk_Handle TrackedEntity;  // what the owner is actually angry at
    FString    TrackedName;

    float Threat   = 0.0f;
    float Score    = 0.0f;
    float Distance = 0.0f;

    // Params that make the live numbers readable — a threat of 8 means nothing without knowing the decay rate and
    // the floor it is racing toward.
    float ThreatDecayRate        = 0.0f;
    float UnperceivedDecayMult   = 1.0f;
    float MinimumTrackedThreat   = 0.0f;
    float RetentionDistance      = 0.0f;
    float ForgetDurationSeconds  = 0.0f;

    // Flattened tag state.
    bool IsActive           = false;
    bool IsPerceived        = false;
    bool IsWithinRetention  = false;
    bool IsPendingForget    = false;
    bool CannotBecomeActive = false;
    bool CannotBeForgotten  = false;

    float SecondsSincePerceived = 0.0f;

    // Effective decay per second right now — the perceived multiplier folded in. This is the number that explains
    // how fast the bar is draining, and it is not readable from any single stored field.
    auto Get_EffectiveDecayRate() const -> float
    {
        return IsPerceived ? ThreatDecayRate : ThreatDecayRate * UnperceivedDecayMult;
    }

    // Seconds until this target's threat crosses MinimumTrackedThreat at the current rate. Negative means "already
    // below" and INFINITY means "never at this rate" — both are states the caller must render differently.
    auto Get_SecondsToForget() const -> float
    {
        const auto Rate = Get_EffectiveDecayRate();
        if (Rate <= KINDA_SMALL_NUMBER)
        { return TNumericLimits<float>::Max(); }

        return (Threat - MinimumTrackedThreat) / Rate;
    }
};

// --------------------------------------------------------------------------------------------------------------------

// One Aggro owner (an entity holding a threat table).
struct FCkAggroDebugger_OwnerInfo
{
    FCk_Handle OwnerEntity;
    FString    OwnerName;

    FCk_Handle ActiveTrackedEntity;
    FString    ActiveTrackedName;

    bool  IsDisabled        = false;
    bool  IsSelectionPending = false;
    int64 EvaluationCount   = 0;

    float SecondsSinceSwitch      = 0.0f;
    float SecondsActiveTargetHeld = 0.0f;

    // Selection gate params — the four hysteresis knobs that explain a switch that did NOT happen.
    float CurrentTargetBias      = 1.0f;
    float SwitchThreshold        = 1.0f;
    float MinimumAggroDuration   = 0.0f;
    float SwitchCooldown         = 0.0f;
    float MinimumTargetScore     = 0.0f;
    int32 MaxTrackedTargets      = 0;

    TArray<FCkAggroDebugger_TargetInfo> Targets;  // sorted by score, descending

    auto Get_MaxThreat() const -> float
    {
        auto Max = 0.0f;
        for (const auto& Target : Targets)
        { Max = FMath::Max(Max, Target.Threat); }
        return Max;
    }

    auto Get_MaxScore() const -> float
    {
        auto Max = 0.0f;
        for (const auto& Target : Targets)
        { Max = FMath::Max(Max, Target.Score); }
        return Max;
    }

    // The score the challenger must beat to take the active slot. Selection multiplies the incumbent's score by
    // CurrentTargetBias * SwitchThreshold, so this is the bar drawn on the meters.
    auto Get_SwitchBarScore() const -> float
    {
        for (const auto& Target : Targets)
        {
            if (Target.IsActive)
            { return Target.Score * CurrentTargetBias * SwitchThreshold; }
        }
        return 0.0f;
    }
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkAggroDebugger_Snapshot
{
    bool  HasWorld = false;
    int32 NumOwners = 0;
    int32 NumTargets = 0;
    int32 NumEngaged = 0;  // owners holding a valid active target

    TArray<FCkAggroDebugger_OwnerInfo> Owners;  // sorted: engaged first, then by target count
};

// --------------------------------------------------------------------------------------------------------------------
