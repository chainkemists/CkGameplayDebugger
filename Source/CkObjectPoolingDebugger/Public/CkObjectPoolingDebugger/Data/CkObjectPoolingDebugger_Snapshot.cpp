#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/ObjectPooling/CkObjectPooling_Subsystem.h"

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

        // the archetype is the class CDO for a plain class-keyed pool — call that out rather than
        // printing the CDO's mangled Default__ name
        Row.ArchetypeName = (ck::IsValid(Archetype) && ck::IsValid(Class) && Archetype == Class->GetDefaultObject())
            ? TEXT("CDO")
            : (ck::IsValid(Archetype) ? Archetype->GetName() : TEXT("(null)"));

        Row.NumFree             = InStats.Get_NumFree();
        Row.NumInUse            = InStats.Get_NumInUse();
        Row.NumLiveInstances    = InStats.Get_NumLiveInstances();
        Row.NumPrewarmRemaining = InStats.Get_NumPrewarmRemaining();
        Row.HighWaterMark       = InStats.Get_HighWaterMark();
        Row.NumHits             = InStats.Get_NumHits();
        Row.NumMisses           = InStats.Get_NumMisses();

        Snapshot.Pools.Emplace(MoveTemp(Row));
    });

    Snapshot.Pools.Sort([](const FCkObjectPoolingDebugger_PoolRow& A, const FCkObjectPoolingDebugger_PoolRow& B)
    {
        return A.ClassName == B.ClassName ? A.ArchetypeName < B.ArchetypeName : A.ClassName < B.ClassName;
    });

    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
