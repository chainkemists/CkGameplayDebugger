#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/ObjectPooling/CkObjectPooling_Subsystem.h"

#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkObjectPoolingDebugger_Snapshot::
    Gather(
        UWorld* InWorld)
    -> FCkObjectPoolingDebugger_Snapshot
{
    auto Snapshot = FCkObjectPoolingDebugger_Snapshot{};

    if (ck::Is_NOT_Valid(InWorld))
    { return Snapshot; }

    auto* Subsystem = InWorld->GetSubsystem<UCk_ObjectPooling_Subsystem_UE>();

    if (ck::Is_NOT_Valid(Subsystem))
    { return Snapshot; }

    Snapshot.HasSubsystem = true;
    Snapshot.NumPinnedUnique = Subsystem->Get_NumPinnedUnique();

    Subsystem->ForEach_Pool(
    [&](const FCk_ObjectPooling_PoolKey& InKey, const FCk_ObjectPooling_PoolStats& InStats)
    {
        const UClass*  Class     = InKey.Get_Class();
        const UObject* Archetype = InKey.Get_Archetype();

        auto Row = FCkObjectPoolingDebugger_PoolRow{};
        Row.ClassName = ck::IsValid(Class) ? Class->GetName() : TEXT("(null)");
        Row.DisplayClassName = ck::DebugNameClean::Get_CleanName(Row.ClassName);

        // show "CDO" for a class-keyed pool rather than the mangled Default__ name
        Row.IsArchetypeCDO = ck::IsValid(Archetype) && ck::IsValid(Class) && Archetype == Class->GetDefaultObject();
        Row.ArchetypeName = Row.IsArchetypeCDO
            ? TEXT("CDO")
            : (ck::IsValid(Archetype) ? ck::DebugNameClean::Get_CleanName(Archetype->GetName()) : TEXT("(null)"));

        Row.NumFree             = InStats.Get_NumFree();
        Row.NumInUse            = InStats.Get_NumInUse();
        Row.NumLiveInstances    = InStats.Get_NumLiveInstances();
        Row.NumPrewarmRemaining = InStats.Get_NumPrewarmRemaining();
        Row.HighWaterMark       = InStats.Get_HighWaterMark();
        Row.NumHits             = InStats.Get_NumHits();
        Row.NumMisses           = InStats.Get_NumMisses();

        const auto& Params = InStats.Get_Params();
        Row.IsRecyclePolicy      = Params.Get_RecyclePolicy() == ECk_ObjectPooling_RecyclePolicy::Recycle;
        Row.IsBoundedCapacity    = Params.Get_CapacityPolicy() == ECk_ObjectPooling_CapacityPolicy::Bounded;
        Row.MaxSize              = Params.Get_MaxSize();
        Row.IsGrowOnExhaustion   = Params.Get_ExhaustionPolicy() == ECk_ObjectPooling_ExhaustionPolicy::Grow;
        Row.GrowBatchCount       = Params.Get_GrowBatchCount();
        Row.PrewarmCount         = Params.Get_PrewarmCount();
        Row.PrewarmBudgetPerTick = Params.Get_PrewarmBudgetPerTick();

        Snapshot.Pools.Emplace(MoveTemp(Row));
    });

    Snapshot.Pools.Sort([](const FCkObjectPoolingDebugger_PoolRow& A, const FCkObjectPoolingDebugger_PoolRow& B)
    {
        return A.ClassName == B.ClassName ? A.ArchetypeName < B.ArchetypeName : A.ClassName < B.ClassName;
    });

    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
