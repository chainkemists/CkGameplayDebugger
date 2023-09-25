#include "CkDebugger_Category_Data.h"

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
        APlayerController* InOwnerPC,
        FGameplayDebuggerCanvasContext* InCanvasContext,
        AGameplayDebuggerCategoryReplicator* InReplicator)
    : _OwnerPC(InOwnerPC)
    , _Replicator(InReplicator)
    , _CanvasContext(InCanvasContext)
{
}

// --------------------------------------------------------------------------------------------------------------------
