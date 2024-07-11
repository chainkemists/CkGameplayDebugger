#include "CkEcs_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <EngineUtils.h>
#include <Engine/PackageMapClient.h>

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
    if (ck::IsValid(GetWorld()->GetNetDriver()) &&
        ck::IsValid(GetWorld()->GetNetDriver()->GuidCache) &&
        ck::IsValid(SelectedWorld) &&
        ck::IsValid(SelectedWorld->GetNetDriver()) &&
        ck::IsValid(SelectedWorld->GetNetDriver()->GuidCache))
    {
        const auto SelectedNetGuid = GetWorld()->GetNetDriver()->GuidCache->GetOrAssignNetGUID(SelectionActor);
        if (SelectedNetGuid.IsValid() && NOT SelectedNetGuid.IsDefault())
        {
            auto* FoundActor = Cast<AActor>(SelectedWorld->GetNetDriver()->GuidCache->GetObjectFromNetGUID(SelectedNetGuid, true));
            if (ck::IsValid(FoundActor))
            {
                SelectionActor = FoundActor;
            }
        }
    }

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(SelectionActor))
    { return {}; }

    return UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(SelectionActor);
}

// --------------------------------------------------------------------------------------------------------------------

