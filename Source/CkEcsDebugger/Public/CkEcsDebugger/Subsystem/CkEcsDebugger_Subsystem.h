#pragma once

#include "CoreMinimal.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"
#include "CkEcsDebugger_Subsystem.generated.h"

UCLASS(DisplayName = "CkSubsystem_EcsDebuggerSlate")
class CKECSDEBUGGER_API UCkEcsDebugger_Subsystem : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    auto OnWorldBeginPlay(UWorld& InWorld) -> void override;
};
