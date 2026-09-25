#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_DataCollector.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include <Engine/World.h>

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Collect(UWorld* InWorld)
    -> TArray<FCkProceduralAnimationDebugger_Row>
{
    auto Rows = TArray<FCkProceduralAnimationDebugger_Row>{};
    if (NOT ck::IsValid(InWorld) || NOT InWorld->HasBegunPlay())
    { return Rows; }
    for (const auto& Entity : UCk_Utils_ProceduralAnimation_Debug_UE::Get_Entities(InWorld))
    {
        auto Snapshot = UCk_Utils_ProceduralAnimation_Debug_UE::Get_Snapshot(Entity);
        if (NOT Snapshot.Get_Available())
        { continue; }
        auto Row = FCkProceduralAnimationDebugger_Row{};
        Row.Entity = Entity;
        Row.Label = Snapshot.Get_EntityName().IsNone() ? Snapshot.Get_EntityId() : Snapshot.Get_EntityName().ToString();
        Row.Snapshot = MoveTemp(Snapshot);
        Rows.Add(MoveTemp(Row));
    }
    Rows.Sort([](const auto& InA, const auto& InB)
    {
        const auto NameOrder = InA.Label.Compare(InB.Label);
        return NameOrder == 0 ? InA.Snapshot.Get_EntityId() < InB.Snapshot.Get_EntityId() : NameOrder < 0;
    });
    return Rows;
}

auto
    FCkProceduralAnimationDebugger_DataCollector::
    Is_Supported(const FCk_Handle& InEntity)
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_ProceduralGait_UE::Has(InEntity)
        && UCk_Utils_ProceduralAnimation_Debug_UE::Get_Snapshot(InEntity).Get_Available();
}
