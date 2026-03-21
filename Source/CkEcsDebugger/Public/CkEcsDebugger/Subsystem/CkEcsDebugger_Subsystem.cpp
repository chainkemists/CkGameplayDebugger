#include "CkEcsDebugger_Subsystem.h"

auto UCkEcsDebugger_Subsystem::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
}

auto UCkEcsDebugger_Subsystem::Deinitialize() -> void
{
    Super::Deinitialize();
}

auto UCkEcsDebugger_Subsystem::OnWorldBeginPlay(UWorld& InWorld) -> void
{
    Super::OnWorldBeginPlay(InWorld);
}
