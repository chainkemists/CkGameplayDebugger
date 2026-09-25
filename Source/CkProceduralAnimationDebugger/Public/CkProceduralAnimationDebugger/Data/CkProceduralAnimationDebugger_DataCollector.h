#pragma once

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkEcs/Handle/CkHandle.h"

struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Row
{
    FCk_Handle Entity;
    FString Label;
    FCk_ProceduralAnimation_DebugSnapshot Snapshot;
};

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_DataCollector
{
public:
    static auto Collect(UWorld* InWorld) -> TArray<FCkProceduralAnimationDebugger_Row>;
    static auto Is_Supported(const FCk_Handle& InEntity) -> bool;
};
