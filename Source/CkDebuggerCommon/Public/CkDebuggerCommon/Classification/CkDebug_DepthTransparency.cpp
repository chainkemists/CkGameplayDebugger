#include "CkDebug_DepthTransparency.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkActorRelay/CkActorRelay_Actor.h"

#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

// ====================================================================================================================

namespace ck::DebugDepthTransparency
{
    auto Get_IsTransparentOwner(
        const FCk_Handle& InOwner)
        -> bool
    {
        const auto* Settings = UCkDebuggerSettings::Get();
        if (Settings == nullptr || NOT Settings->bActorRelayDepthTransparency)
        { return false; }

        return Get_IsRelayEntity(InOwner);
    }

    auto Get_IsRelayEntity(
        const FCk_Handle& InEntity)
        -> bool
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return false; }

        // Same check CkInspector_ActorRelay uses to decide it can inspect an entity.
        auto* OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
        return Cast<ACk_ActorRelay_UE>(OwningActor) != nullptr;
    }
}

// ====================================================================================================================
