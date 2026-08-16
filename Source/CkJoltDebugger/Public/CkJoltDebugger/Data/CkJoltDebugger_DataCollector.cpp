#include "CkJoltDebugger/Data/CkJoltDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/Character/CkJoltCharacter_Fragment.h"
#include "CkJolt/Constraint/CkJoltConstraint_Fragment.h"
#include "CkJolt/Constraint/CkJoltConstraint_Utils.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Fragment.h"
#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_data_collector
{
    auto Get_DisplayName(
        const FCk_Handle& InHandle) -> FString
    {
        return ck::DebugNameClean::Get_CleanName(UCk_Utils_Handle_UE::Get_DebugName(InHandle).ToString());
    }

    /*
     * The drawn-body key behind an arbitrary handle, for the two bodies a constraint names. It reads the SAME
     * fragment the JoltBody pass keys its own rows from, so a constraint's body key and that body's row key are
     * the same number by construction rather than by agreement.
     */
    auto TryGet_BodyKey(
        const FCk_Handle& InHandle) -> TOptional<uint64>
    {
        if (ck::Is_NOT_Valid(InHandle) || NOT InHandle.Has<ck::FFragment_JoltBody_Current>())
        { return {}; }

        return ck::jolt::debug_draw::Make_BodyKey(
            InHandle.Get<ck::FFragment_JoltBody_Current>().Get_BodyId().GetIndexAndSequenceNumber());
    }

    auto Make_Snapshot(
        const FCk_Handle& InHandle,
        ECkJoltDebugger_Population InPopulation,
        TOptional<uint64> InBodyKey) -> FCkJoltDebugger_BodySnapshot
    {
        auto Snapshot = FCkJoltDebugger_BodySnapshot{};
        Snapshot.Handle      = InHandle;
        Snapshot.Population  = InPopulation;
        Snapshot.BodyKey     = InBodyKey;
        Snapshot.DisplayName = Get_DisplayName(InHandle);
        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkJoltDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Bodies.Reset();
    _BakedBodyOwners.Reset();
    _NumProblemRows = 0;

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    // A world appears in the engine's context list before it begins play, and subsystem access on one that
    // has not begun play crashes.
    if (NOT InWorld->HasBegunPlay())
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_JoltBody_Current>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_JoltBody_Current& InCurrent)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            auto Snapshot = ck_jolt_debugger_data_collector::Make_Snapshot(
                Handle,
                ECkJoltDebugger_Population::JoltBody,
                ck::jolt::debug_draw::Make_BodyKey(InCurrent.Get_BodyId().GetIndexAndSequenceNumber()));

            const auto JoltBody = UCk_Utils_JoltBody_UE::CastChecked(Handle);
            Snapshot.MotionType         = UCk_Utils_JoltBody_UE::Get_MotionType(JoltBody);
            Snapshot.SleepState         = UCk_Utils_JoltBody_UE::Get_SleepState(JoltBody);
            Snapshot.HasSimulationState = true;

            _Bodies.Emplace(MoveTemp(Snapshot));
        });

    TransientEntity.View<ck::FFragment_JoltStaticActor_Current>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_JoltStaticActor_Current& InCurrent)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            const auto& BodyIds = InCurrent.Get_BodyIds();

            auto FirstBodyKey = TOptional<uint64>{};

            for (const auto& BodyId : BodyIds)
            {
                const auto BodyKey = ck::jolt::debug_draw::Make_BodyKey(BodyId);
                _BakedBodyOwners.Emplace(BodyKey, Handle);

                if (NOT FirstBodyKey.IsSet())
                { FirstBodyKey = BodyKey; }
            }

            auto Snapshot = ck_jolt_debugger_data_collector::Make_Snapshot(
                Handle,
                ECkJoltDebugger_Population::BakedStatic,
                FirstBodyKey);

            Snapshot.NumBodies       = BodyIds.Num();
            Snapshot.SourceActorName = InCurrent.Get_SourceActorName().ToString();

            _Bodies.Emplace(MoveTemp(Snapshot));
        });

    TransientEntity.View<ck::FFragment_Probe_Current>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_Probe_Current& InCurrent)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            _Bodies.Emplace(ck_jolt_debugger_data_collector::Make_Snapshot(
                Handle,
                ECkJoltDebugger_Population::Sensor,
                ck::jolt::debug_draw::Make_BodyKey(InCurrent.Get_BodyId().GetIndexAndSequenceNumber())));
        });

    TransientEntity.View<ck::FFragment_JoltCharacter_Current>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_JoltCharacter_Current&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            const auto CharacterKey = ck::jolt::debug_draw::Make_CharacterBodyKey(Handle);

            _Bodies.Emplace(ck_jolt_debugger_data_collector::Make_Snapshot(
                Handle,
                ECkJoltDebugger_Population::Character,
                CharacterKey == 0 ? TOptional<uint64>{} : TOptional<uint64>{CharacterKey}));
        });

    // The fifth pass (P8-D55). A constraint entity draws NOTHING, so its row carries no body key of its own —
    // what it carries is the pair it joins, which is the whole reason the row exists.
    TransientEntity.View<ck::FFragment_JoltConstraint_Current>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_JoltConstraint_Current&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            auto Snapshot = ck_jolt_debugger_data_collector::Make_Snapshot(
                Handle,
                ECkJoltDebugger_Population::Constraint,
                TOptional<uint64>{});

            const auto Constraint = UCk_Utils_JoltConstraint_UE::CastChecked(Handle);

            Snapshot.ConstraintType      = UCk_Utils_JoltConstraint_UE::Get_ConstraintType(Constraint);
            Snapshot.IsBodyBWorldAnchor  = UCk_Utils_JoltConstraint_UE::Get_IsBodyBWorldAnchor(Constraint);

            if (const auto KeyA = ck_jolt_debugger_data_collector::TryGet_BodyKey(
                UCk_Utils_JoltConstraint_UE::Get_BodyA(Constraint)); KeyA.IsSet())
            { Snapshot.ConstraintBodyKeys.Emplace(*KeyA); }

            if (const auto KeyB = ck_jolt_debugger_data_collector::TryGet_BodyKey(
                UCk_Utils_JoltConstraint_UE::Get_BodyB(Constraint)); KeyB.IsSet())
            { Snapshot.ConstraintBodyKeys.Emplace(*KeyB); }

            Snapshot.NumBodies = Snapshot.ConstraintBodyKeys.Num();

            _Bodies.Emplace(MoveTemp(Snapshot));
        });
}

auto
    FCkJoltDebugger_DataCollector::
    Apply_ProblemFlags(
        const TMap<uint64, ECk_Jolt_DebugDraw_ProblemFlags>& InProblemBodies)
    -> void
{
    _NumProblemRows = 0;

    for (auto& Body : _Bodies)
    {
        Body.ProblemFlags = ECk_Jolt_DebugDraw_ProblemFlags::None;
    }

    if (InProblemBodies.IsEmpty())
    { return; }

    // Body-keyed rows first — the direct answer for every population that draws exactly one body.
    for (auto& Body : _Bodies)
    {
        if (NOT Body.BodyKey.IsSet())
        { continue; }

        if (const auto* Flags = InProblemBodies.Find(*Body.BodyKey))
        { Body.ProblemFlags = *Flags; }
    }

    // ...then the baked actors, whose row names only its FIRST body while the scan flags whichever one broke.
    for (const auto& Entry : InProblemBodies)
    {
        const auto* Owner = _BakedBodyOwners.Find(Entry.Key);

        if (Owner == nullptr)
        { continue; }

        const auto OwnerHandle = *Owner;

        auto* Row = _Bodies.FindByPredicate([&OwnerHandle](const FCkJoltDebugger_BodySnapshot& InBody)
        { return InBody.Handle == OwnerHandle; });

        if (Row != nullptr)
        { EnumAddFlags(Row->ProblemFlags, Entry.Value); }
    }

    // ...and the constraints, which have no body of their own but are exactly as broken as the pair they join.
    for (auto& Body : _Bodies)
    {
        if (Body.Population != ECkJoltDebugger_Population::Constraint)
        { continue; }

        for (const auto ConstraintBodyKey : Body.ConstraintBodyKeys)
        {
            if (const auto* Flags = InProblemBodies.Find(ConstraintBodyKey))
            { EnumAddFlags(Body.ProblemFlags, *Flags); }
        }
    }

    for (const auto& Body : _Bodies)
    {
        if (Body.Get_HasProblem())
        { ++_NumProblemRows; }
    }
}

auto
    FCkJoltDebugger_DataCollector::
    Reset()
    -> void
{
    _Bodies.Reset();
    _BakedBodyOwners.Reset();
    _NumProblemRows = 0;
}

// --------------------------------------------------------------------------------------------------------------------
