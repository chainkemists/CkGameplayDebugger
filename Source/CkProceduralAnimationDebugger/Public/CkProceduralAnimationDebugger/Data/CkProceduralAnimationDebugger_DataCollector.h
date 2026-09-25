#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkProceduralAnimation/CkProceduralAnimation_Fragment_Data.h"

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
    ECk_ProceduralAnimation_Status _GaitStatus = ECk_ProceduralAnimation_Status::PendingSetup;
    bool _HasRig = false;
    ECk_ProceduralAnimation_Status _RigStatus = ECk_ProceduralAnimation_Status::PendingSetup;
    int32 _LegCount = 0;
    int32 _EnabledLegCount = 0;
    int32 _PlantedCount = 0;

public:
    CK_PROPERTY(_EntityName);
    CK_PROPERTY(_EntityId);
    CK_PROPERTY(_GaitStatus);
    CK_PROPERTY(_HasRig);
    CK_PROPERTY(_RigStatus);
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
