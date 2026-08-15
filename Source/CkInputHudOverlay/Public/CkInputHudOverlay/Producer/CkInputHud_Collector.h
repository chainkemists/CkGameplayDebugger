#pragma once

#include "CoreMinimal.h"

class FCk_InputHud_Model;
class FCkDebug_KeyActivityObserver;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// The HUD's producer: one gather pass per ticker callback, reading only public CkInput / CkIntent Utils plus the
// registry views those Utils do not reach. It NEVER mutates gameplay state and never binds a signal — every reading
// is polled, because a HUD that subscribed would have to own unbind lifetimes across PIE for no benefit.
//
// The source entity is re-resolved on EVERY pass. Nothing entity-shaped is cached between passes; what survives in
// the model is plain values.
// --------------------------------------------------------------------------------------------------------------------

struct FCk_InputHud_CollectParams
{
    UWorld* World = nullptr;

    // The Slate input pre-processor the subsystem owns. Null is legal (Slate absent) and only costs the sticks.
    const FCkDebug_KeyActivityObserver* Observer = nullptr;

    // Wall-clock seconds — the only clock the event stream has. See the collector's note on batching error.
    double NowSeconds = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------

class CKINPUTHUDOVERLAY_API FCk_InputHud_Collector
{
public:
    static auto Collect(
        const FCk_InputHud_CollectParams& InParams,
        FCk_InputHud_Model&               OutModel) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
