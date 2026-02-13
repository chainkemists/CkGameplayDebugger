#include "CkDebugger_Category_Data.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategoryReplicator.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnCollectData::
    FCk_Payload_GameplayDebugger_OnCollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor
#if WITH_GAMEPLAY_DEBUGGER
        , AGameplayDebuggerCategoryReplicator* InReplicator
#endif
    )
    : _OwnerPC(InOwnerPC)
    , _DebugActor(InDebugActor)
#if WITH_GAMEPLAY_DEBUGGER
    , _Replicator(InReplicator)
#endif
{
}

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnDrawData::
    FCk_Payload_GameplayDebugger_OnDrawData(
        TWeakObjectPtr<APlayerController> InOwnerPC,
        FGameplayDebuggerCanvasContext* InCanvasContext,
#if WITH_GAMEPLAY_DEBUGGER
        TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> InReplicator,
#endif
        TArray<TWeakObjectPtr<UWorld>> InAvailableWorlds,
        TWeakObjectPtr<UWorld> InCurrentWorld)
    : _OwnerPC(InOwnerPC)
#if WITH_GAMEPLAY_DEBUGGER
    , _Replicator(InReplicator)
#endif
    , _AvailableWorlds(MoveTemp(InAvailableWorlds))
    , _CurrentWorld(InCurrentWorld)
    , _CanvasContext(InCanvasContext)
{
}

// --------------------------------------------------------------------------------------------------------------------
