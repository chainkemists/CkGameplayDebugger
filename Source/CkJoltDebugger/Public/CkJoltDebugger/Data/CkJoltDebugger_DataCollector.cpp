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
}

auto
    FCkJoltDebugger_DataCollector::
    Reset()
    -> void
{
    _Bodies.Reset();
    _BakedBodyOwners.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
