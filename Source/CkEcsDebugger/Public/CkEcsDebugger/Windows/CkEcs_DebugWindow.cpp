#include "CkEcs_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <EngineUtils.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Ecs_DebugWindow::
    Get_SelectionEntity()
    -> FCk_Handle
{
    auto SelectionActor = GetSelection();

    if (ck::Is_NOT_Valid(SelectionActor))
    { return {}; }

    const auto SelectedWorld = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>()->_SelectedWorld;
    if (ck::IsValid(SelectedWorld))
    {
        for (TActorIterator<AActor> ActorItr(SelectedWorld); ActorItr; ++ActorItr)
        {
            auto* FoundActor = *ActorItr;

            if (FoundActor && FoundActor->GetName() == SelectionActor->GetName())
            {
                SelectionActor = FoundActor;
                break;
            }
        }
    }

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(SelectionActor))
    { return {}; }

    return UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(SelectionActor);
}

// --------------------------------------------------------------------------------------------------------------------

