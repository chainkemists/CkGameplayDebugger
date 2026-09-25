#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_collector
{
    auto
        DoAggregate_Status(
            ECk_ProceduralAnimation_Status InAggregate,
            ECk_ProceduralAnimation_Status InRig)
        -> ECk_ProceduralAnimation_Status
    {
        if (InAggregate == ECk_ProceduralAnimation_Status::Failed || InRig == ECk_ProceduralAnimation_Status::Failed)
        { return ECk_ProceduralAnimation_Status::Failed; }

        if (InAggregate == ECk_ProceduralAnimation_Status::PendingSetup || InRig == ECk_ProceduralAnimation_Status::PendingSetup)
        { return ECk_ProceduralAnimation_Status::PendingSetup; }

        return ECk_ProceduralAnimation_Status::Ready;
    }

    auto
        Make_Summary(
            const FCk_Handle& InBody)
        -> FCkProceduralAnimationDebugger_Summary
    {
        const auto Gait = UCk_Utils_ProceduralGait_UE::CastChecked(InBody);
        const auto Legs = UCk_Utils_ProceduralGait_UE::Get_Legs(Gait);

        auto HasRig = false;
        auto RigStatus = ECk_ProceduralAnimation_Status::Ready;
        for (const auto& Leg : Legs)
        {
            const auto Rig = UCk_Utils_ProceduralRig_UE::Cast(Leg);
            if (ck::Is_NOT_Valid(Rig))
            { continue; }

            HasRig = true;
            RigStatus = DoAggregate_Status(RigStatus, UCk_Utils_ProceduralRig_UE::Get_Status(Rig));
        }

        auto Summary = FCkProceduralAnimationDebugger_Summary{};
        Summary.Set_EntityName(InBody.Get_DebugName())
            .Set_EntityId(InBody.Get_Entity().ToString())
            .Set_GaitStatus(UCk_Utils_ProceduralGait_UE::Get_Status(Gait))
            .Set_HasRig(HasRig)
            .Set_RigStatus(HasRig ? RigStatus : ECk_ProceduralAnimation_Status::PendingSetup)
            .Set_LegCount(Legs.Num())
            .Set_EnabledLegCount(UCk_Utils_ProceduralGait_UE::Get_EnabledLegCount(Gait))
            .Set_PlantedCount(UCk_Utils_ProceduralGait_UE::Get_PlantedCount(Gait));
        return Summary;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Summary::
    operator==(
        const FCkProceduralAnimationDebugger_Summary& InOther) const
    -> bool
{
    return _EntityName == InOther._EntityName
        && _EntityId == InOther._EntityId
        && _GaitStatus == InOther._GaitStatus
        && _HasRig == InOther._HasRig
        && _RigStatus == InOther._RigStatus
        && _LegCount == InOther._LegCount
        && _EnabledLegCount == InOther._EnabledLegCount
        && _PlantedCount == InOther._PlantedCount;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> TArray<FCkProceduralAnimationDebugger_Row>
{
    auto Rows = TArray<FCkProceduralAnimationDebugger_Row>{};
    if (ck::Is_NOT_Valid(InWorld) || NOT InWorld->HasBegunPlay())
    { return Rows; }

    for (const auto& Entity : UCk_Utils_ProceduralAnimation_Debug_UE::Get_Entities(InWorld))
    {
        auto Row = FCkProceduralAnimationDebugger_Row{};
        Row.Entity = Entity;
        Row.Summary = ck_procedural_debug_collector::Make_Summary(Entity);
        Row.Label = Row.Summary.Get_EntityName().IsNone() ? Row.Summary.Get_EntityId() : Row.Summary.Get_EntityName().ToString();
        Rows.Add(MoveTemp(Row));
    }

    Rows.Sort([](const FCkProceduralAnimationDebugger_Row& InA, const FCkProceduralAnimationDebugger_Row& InB)
    {
        const auto NameOrder = InA.Label.Compare(InB.Label);
        return NameOrder == 0 ? InA.Summary.Get_EntityId() < InB.Summary.Get_EntityId() : NameOrder < 0;
    });
    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Is_Supported(
        const FCk_Handle& InEntity)
    -> bool
{
    return ck::IsValid(InEntity)
        && NOT InEntity.Has<ck::FTag_DestroyEntity_Initiate>()
        && UCk_Utils_ProceduralGait_UE::Has(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------
