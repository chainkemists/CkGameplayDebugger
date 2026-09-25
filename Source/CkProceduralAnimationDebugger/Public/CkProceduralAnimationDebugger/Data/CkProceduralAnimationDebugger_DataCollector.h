#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

// Roster values read through the public feature utils; only the selected entity pays for a full snapshot.
struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Summary
{
    CK_GENERATED_BODY(FCkProceduralAnimationDebugger_Summary);

private:
    FName _EntityName;
    FString _EntityId;
    bool _GaitReady = false;
    bool _GaitFailed = false;
    bool _HasRig = false;
    bool _RigReady = false;
    bool _RigFailed = false;
    int32 _LegCount = 0;
    int32 _EnabledLegCount = 0;
    int32 _PlantedCount = 0;

public:
    CK_PROPERTY(_EntityName);
    CK_PROPERTY(_EntityId);
    CK_PROPERTY(_GaitReady);
    CK_PROPERTY(_GaitFailed);
    CK_PROPERTY(_HasRig);
    CK_PROPERTY(_RigReady);
    CK_PROPERTY(_RigFailed);
    CK_PROPERTY(_LegCount);
    CK_PROPERTY(_EnabledLegCount);
    CK_PROPERTY(_PlantedCount);

public:
    auto operator==(const FCkProceduralAnimationDebugger_Summary& InOther) const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Row
{
    FCk_Handle Entity;
    FString Label;
    FCkProceduralAnimationDebugger_Summary Summary;
};

// --------------------------------------------------------------------------------------------------------------------

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_DataCollector
{
public:
    static auto
        Collect(
            UWorld* InWorld)
        -> TArray<FCkProceduralAnimationDebugger_Row>;

    static auto
        Is_Supported(
            const FCk_Handle& InEntity)
        -> bool;
};

// --------------------------------------------------------------------------------------------------------------------
