#include "CkEcsDebugger_Classification.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_classification::
    BuildTable(
        const TSet<FName>& InInternalFeatureIds)
    -> FClassificationTable
{
    auto Table = FClassificationTable{};
    Table.BitToFeatureId.Init(NAME_None, debug_feature_flags::MaxFlags);

    for (const auto& FeatureId : debug_feature_flags::Get_RegisteredFeatureIds())
    {
        const auto Bit = debug_feature_flags::Get_BitIndex(FeatureId);
        if (Bit == INDEX_NONE)
        { continue; }

        Table.BitToFeatureId[Bit] = FeatureId;

        // Underscore-prefixed ids are infrastructure flags (e.g. "_TreeEntity", the churn
        // detector on FFragment_LifetimeOwner) — structural like Transform/Label, never a
        // classifying feature.
        const auto IsInfrastructure = FeatureId.ToString().StartsWith(TEXT("_"));

        if (IsInfrastructure || FeatureId == TEXT("Transform") || FeatureId == TEXT("Label"))
        { Table.StructuralMask |= uint64{1} << Bit; }

        if (InInternalFeatureIds.Contains(FeatureId))
        { Table.InternalMask |= uint64{1} << Bit; }
    }

    return Table;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_classification::
    Classify(
        uint64 InOwnBits,
        const FClassificationTable& InTable)
    -> FClassification
{
    const auto Residual = InOwnBits & ~InTable.StructuralMask;

    if (FMath::CountBits(Residual) != 1)
    { return {}; }

    if ((Residual & InTable.InternalMask) == 0)
    { return {}; }

    const auto Bit = static_cast<int32>(FMath::CountTrailingZeros64(Residual));
    return FClassification{ true, InTable.BitToFeatureId[Bit] };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_classification::
    ComputeRollups(
        const TArray<uint64>& InOwnBits,
        const TArray<int32>& InOwnerIndex,
        const FClassificationTable& InTable)
    -> TArray<TMap<FName, int32>>
{
    const auto Num = InOwnBits.Num();

    auto Result = TArray<TMap<FName, int32>>{};
    Result.SetNum(Num);

    CK_ENSURE_IF_NOT(InOwnerIndex.Num() == Num,
        TEXT("ComputeRollups: bits [{}] and owners [{}] arrays must be parallel"),
        Num, InOwnerIndex.Num())
    { return Result; }

    auto Classifications = TArray<FClassification>{};
    Classifications.Reserve(Num);
    for (auto Index = 0; Index < Num; ++Index)
    { Classifications.Add(Classify(InOwnBits[Index], InTable)); }

    // Nearest-primary-ancestor memo with path compression: a chain of internals is
    // resolved once, then every node on it answers O(1). InProgress marks the current
    // walk so an owner cycle resolves to "no primary" instead of looping.
    constexpr auto Unvisited  = int32{-2};
    constexpr auto InProgress = int32{-3};
    constexpr auto NoPrimary  = int32{INDEX_NONE};

    auto NearestPrimary = TArray<int32>{};
    NearestPrimary.Init(Unvisited, Num);

    const auto ResolveNearestPrimary = [&](int32 InNode) -> int32
    {
        auto Path = TArray<int32>{};
        auto Cursor = InNode;
        auto Answer = NoPrimary;

        while (true)
        {
            const auto Owner = InOwnerIndex[Cursor];

            if (Owner < 0 || Owner >= Num)
            { break; }

            if (NOT Classifications[Owner].IsInternal)
            { Answer = Owner; break; }

            if (NearestPrimary[Owner] == InProgress)
            { break; }   // owner cycle

            if (NearestPrimary[Owner] != Unvisited)
            { Answer = NearestPrimary[Owner]; break; }

            NearestPrimary[Owner] = InProgress;
            Path.Add(Owner);
            Cursor = Owner;
        }

        for (const auto PathNode : Path)
        { NearestPrimary[PathNode] = Answer; }

        NearestPrimary[InNode] = Answer;
        return Answer;
    };

    for (auto Index = 0; Index < Num; ++Index)
    {
        const auto& Classification = Classifications[Index];
        if (NOT Classification.IsInternal)
        { continue; }

        const auto Primary = NearestPrimary[Index] == Unvisited || NearestPrimary[Index] == InProgress
            ? ResolveNearestPrimary(Index)
            : NearestPrimary[Index];

        if (Primary == NoPrimary)
        { continue; }

        ++Result[Primary].FindOrAdd(Classification.InternalFeatureId);
    }

    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_classification::
    Apply_TransparentOwnerPassThrough(
        const TArray<int32>& InOwnerIndex,
        const TArray<bool>& InIsTransparentOwner)
    -> TArray<int32>
{
    const auto Num = InOwnerIndex.Num();

    auto Result = InOwnerIndex;

    CK_ENSURE_IF_NOT(InIsTransparentOwner.Num() == Num,
        TEXT("Apply_TransparentOwnerPassThrough: owners [{}] and transparency [{}] arrays must be parallel"),
        Num, InIsTransparentOwner.Num())
    { return Result; }

    // Nothing transparent — the literal graph IS the effective graph. The steady state
    // whenever the user's transparency setting is off.
    if (NOT InIsTransparentOwner.Contains(true))
    { return Result; }

    // Same memo-with-path-compression shape as ComputeRollups' nearest-primary walk:
    // Resolved[i] = i's nearest NON-transparent owner. InProgress marks the active walk so
    // an owner cycle answers "no owner" instead of looping.
    constexpr auto Unvisited  = int32{-2};
    constexpr auto InProgress = int32{-3};
    constexpr auto NoOwner    = int32{INDEX_NONE};

    auto Resolved = TArray<int32>{};
    Resolved.Init(Unvisited, Num);

    const auto ResolveOwner = [&](int32 InNode) -> int32
    {
        auto Path = TArray<int32>{};
        auto Cursor = InNode;
        auto Answer = NoOwner;

        while (true)
        {
            const auto Owner = InOwnerIndex[Cursor];

            if (Owner < 0 || Owner >= Num)
            { break; }

            if (NOT InIsTransparentOwner[Owner])
            { Answer = Owner; break; }

            if (Resolved[Owner] == InProgress)
            { break; }   // owner cycle through transparent owners

            if (Resolved[Owner] != Unvisited)
            { Answer = Resolved[Owner]; break; }

            Resolved[Owner] = InProgress;
            Path.Add(Owner);
            Cursor = Owner;
        }

        for (const auto PathNode : Path)
        { Resolved[PathNode] = Answer; }

        Resolved[InNode] = Answer;
        return Answer;
    };

    for (auto Index = 0; Index < Num; ++Index)
    {
        Result[Index] = Resolved[Index] == Unvisited || Resolved[Index] == InProgress
            ? ResolveOwner(Index)
            : Resolved[Index];
    }

    return Result;
}
