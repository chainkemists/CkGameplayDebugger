#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkDialog/Line/CkDialogLine_Fragment_Data.h"
#include "CkDialog/Emitter/CkDialogEmitter_Fragment_Data.h"

#include <GameplayTags.h>

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Plain view structs collected read-only from the world by FCkDialogDebugger_DataCollector.
// --------------------------------------------------------------------------------------------------------------------

struct FCkDialogDebugger_LineInfo
{
    FCk_Handle_DialogLine Handle;
    FName                 LineID;
    FString               Text;
    FGameplayTag          EventTag;
    FGameplayTag          LinkedEventTag;
    FGameplayTagContainer FilterTags;
    int32                 NumConditions = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDialogDebugger_CooldownInfo
{
    FCk_Handle_DialogLine Line;
    FName                 LineID;
    float                 RemainingSeconds = 0.0f;
    // The duration the cooldown was STARTED with — the meter's denominator. Remaining alone cannot say how far
    // through a cooldown is.
    float                 TotalSeconds = 0.0f;
    bool                  IsForever = false;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDialogDebugger_QueryHistoryEntry
{
    FGameplayTag EventTag;
    int32        NumEntries = 0;
    int32        NumPassed = 0;
    int32        NumFailLine = 0;
    int32        NumFailEmitter = 0;
    float        TimestampSeconds = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDialogDebugger_EmitterInfo
{
    FCk_Handle_DialogEmitter                    Handle;
    FString                                     DebugName;
    FGameplayTagContainer                       EmitterTags;
    TArray<FCkDialogDebugger_CooldownInfo>      Cooldowns;
    TArray<FCkDialogDebugger_QueryHistoryEntry> QueryHistory;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDialogDebugger_RegistrySnapshot
{
    bool                                  IsReady = false;
    int32                                 NumBanks = 0;
    TArray<FCkDialogDebugger_LineInfo>    Lines;
    TArray<FCkDialogDebugger_EmitterInfo> Emitters;
};

// --------------------------------------------------------------------------------------------------------------------
