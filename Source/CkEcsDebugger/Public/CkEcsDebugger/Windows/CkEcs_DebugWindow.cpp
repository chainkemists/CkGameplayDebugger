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

    const auto World = SelectionActor->GetWorld();
    const auto SelectedWorld = World->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>()->Get_SelectedWorld();

    [&]
    {
        if (ck::Is_NOT_Valid(SelectionActor->GetWorld()))
        { return; }

        if (ck::Is_NOT_Valid(World->GetNetDriver()))
        { return; }

        if (ck::Is_NOT_Valid(World->GetNetDriver()->GuidCache))
        { return; }

        if (ck::Is_NOT_Valid(SelectedWorld))
        { return; }

        if (ck::Is_NOT_Valid(SelectedWorld->GetNetDriver()))
        { return; }

        if (ck::Is_NOT_Valid(SelectedWorld->GetNetDriver()->GuidCache))
        { return; }

        const auto& SelectedNetGuid = World->GetNetDriver()->GuidCache->GetOrAssignNetGUID(SelectionActor);

        if (ck::Is_NOT_Valid(SelectedNetGuid))
        { return; }

        if (SelectedNetGuid.IsDefault())
        { return; }

        auto* FoundActor = Cast<AActor>(SelectedWorld->GetNetDriver()->GuidCache->GetObjectFromNetGUID(SelectedNetGuid, true));
        if (ck::IsValid(FoundActor))
        {
            SelectionActor = FoundActor;
        }
    }();

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(SelectionActor))
    { return {}; }

    return UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(SelectionActor);
}

// --------------------------------------------------------------------------------------------------------------------

