#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/ObjectPooling/CkObjectPooling_Subsystem.h"

#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "Engine/World.h"

#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

    Snapshot.WorldName = InWorld->GetName();

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

auto
    FCkObjectPoolingDebugger_Snapshot::
    ToJsonString() const
    -> FString
{
    const auto MakePoolObject = [](const FCkObjectPoolingDebugger_PoolRow& InRow) -> TSharedRef<FJsonObject>
    {
        auto Policy = MakeShared<FJsonObject>();
        Policy->SetStringField(TEXT("recyclePolicy"), InRow.IsRecyclePolicy ? TEXT("Recycle") : TEXT("DestroyOnRelease"));
        Policy->SetStringField(TEXT("capacityPolicy"), InRow.IsBoundedCapacity ? TEXT("Bounded") : TEXT("Unbounded"));
        Policy->SetNumberField(TEXT("maxSize"), InRow.MaxSize); // meaningful only when bounded
        Policy->SetStringField(TEXT("exhaustionPolicy"), InRow.IsGrowOnExhaustion ? TEXT("Grow") : TEXT("Fail"));
        Policy->SetNumberField(TEXT("growBatchCount"), InRow.GrowBatchCount);
        Policy->SetNumberField(TEXT("prewarmCount"), InRow.PrewarmCount);
        Policy->SetNumberField(TEXT("prewarmBudgetPerTick"), InRow.PrewarmBudgetPerTick);

        auto Stats = MakeShared<FJsonObject>();
        Stats->SetNumberField(TEXT("numFree"), InRow.NumFree);
        Stats->SetNumberField(TEXT("numInUse"), InRow.NumInUse);
        Stats->SetNumberField(TEXT("numLiveInstances"), InRow.NumLiveInstances);
        Stats->SetNumberField(TEXT("numPrewarmRemaining"), InRow.NumPrewarmRemaining);
        Stats->SetNumberField(TEXT("highWaterMark"), InRow.HighWaterMark);
        Stats->SetNumberField(TEXT("numHits"), InRow.NumHits);
        Stats->SetNumberField(TEXT("numMisses"), InRow.NumMisses);
        Stats->SetNumberField(TEXT("totalAcquires"), InRow.Get_TotalAcquires());
        Stats->SetNumberField(TEXT("hitRate"), InRow.Get_HitRate());

        auto Pool = MakeShared<FJsonObject>();
        Pool->SetStringField(TEXT("class"), InRow.DisplayClassName);
        Pool->SetStringField(TEXT("classRaw"), InRow.ClassName);
        Pool->SetStringField(TEXT("archetype"), InRow.ArchetypeName);
        Pool->SetBoolField(TEXT("isArchetypeCdo"), InRow.IsArchetypeCDO);
        Pool->SetObjectField(TEXT("policy"), Policy);
        Pool->SetObjectField(TEXT("stats"), Stats);
        return Pool;
    };

    // aggregate totals across every pool — the top-line the report opens with
    auto TotalLive = 0, TotalInUse = 0, TotalFree = 0, TotalHits = 0, TotalMisses = 0;
    auto PoolJsonValues = TArray<TSharedPtr<FJsonValue>>{};
    PoolJsonValues.Reserve(Pools.Num());
    for (const auto& Row : Pools)
    {
        TotalLive   += Row.NumLiveInstances;
        TotalInUse  += Row.NumInUse;
        TotalFree   += Row.NumFree;
        TotalHits   += Row.NumHits;
        TotalMisses += Row.NumMisses;
        PoolJsonValues.Emplace(MakeShared<FJsonValueObject>(MakePoolObject(Row)));
    }

    const auto TotalAcquires = TotalHits + TotalMisses;

    auto Totals = MakeShared<FJsonObject>();
    Totals->SetNumberField(TEXT("liveInstances"), TotalLive);
    Totals->SetNumberField(TEXT("inUse"), TotalInUse);
    Totals->SetNumberField(TEXT("free"), TotalFree);
    Totals->SetNumberField(TEXT("hits"), TotalHits);
    Totals->SetNumberField(TEXT("misses"), TotalMisses);
    Totals->SetNumberField(TEXT("totalAcquires"), TotalAcquires);
    Totals->SetNumberField(TEXT("hitRate"), TotalAcquires > 0 ? static_cast<float>(TotalHits) / TotalAcquires : 1.0f);

    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("generatedAt"), FDateTime::Now().ToIso8601());
    Root->SetStringField(TEXT("world"), WorldName);
    Root->SetBoolField(TEXT("hasSubsystem"), HasSubsystem);
    Root->SetNumberField(TEXT("numPools"), Pools.Num());
    Root->SetNumberField(TEXT("numPinnedUnique"), NumPinnedUnique);
    Root->SetObjectField(TEXT("totals"), Totals);
    Root->SetArrayField(TEXT("pools"), PoolJsonValues);

    auto Out = FString{};
    const auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Root, Writer);
    return Out;
}

// --------------------------------------------------------------------------------------------------------------------
