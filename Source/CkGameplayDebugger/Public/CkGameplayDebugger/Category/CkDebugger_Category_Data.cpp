#include "CkDebugger_Category_Data.h"

#include "GameplayDebuggerCategoryReplicator.h"

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnCollectData::
    FCk_Payload_GameplayDebugger_OnCollectData(
        APlayerController* InOwnerPC,
        AActor* InDebugActor,
        AGameplayDebuggerCategoryReplicator* InReplicator)
    : _OwnerPC(InOwnerPC)
    , _DebugActor(InDebugActor)
    , _Replicator(InReplicator)
{
}

// --------------------------------------------------------------------------------------------------------------------

FCk_Payload_GameplayDebugger_OnDrawData::
    FCk_Payload_GameplayDebugger_OnDrawData(
        TWeakObjectPtr<APlayerController> InOwnerPC,
        FGameplayDebuggerCanvasContext* InCanvasContext,
        TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> InReplicator,
        TArray<TWeakObjectPtr<UWorld>> InAvailableWorlds,
        TWeakObjectPtr<UWorld> InCurrentWorld)
    : _OwnerPC(InOwnerPC)
    , _Replicator(InReplicator)
    , _AvailableWorlds(MoveTemp(InAvailableWorlds))
    , _CurrentWorld(InCurrentWorld)
    , _CanvasContext(InCanvasContext)
{
}

// --------------------------------------------------------------------------------------------------------------------
