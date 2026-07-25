#include "CkDialogDebugger/Data/CkDialogDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkDialog/CkDialog_Subsystem.h"
#include "CkDialog/Line/CkDialogLine_Utils.h"
#include "CkDialog/Emitter/CkDialogEmitter_Utils.h"
#include "CkDialog/Emitter/CkDialogEmitter_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDialogDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Snapshot = FCkDialogDebugger_RegistrySnapshot{};

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    // Registry: lines + bank count.
    if (auto* Registry = UCk_DialogRegistry_Subsystem_UE::Get_DialogRegistry(InWorld); ck::IsValid(Registry))
    {
        _Snapshot.IsReady = Registry->Get_IsReady();
        _Snapshot.NumBanks = Registry->Get_RegisteredBanks().Num();

        for (const auto& Line : Registry->Get_AllLines())
        {
            if (ck::Is_NOT_Valid(Line))
            { continue; }

            auto Info = FCkDialogDebugger_LineInfo{};
            Info.Handle        = Line;
            Info.LineID        = UCk_Utils_DialogLine_UE::Get_LineID(Line);
            Info.Text          = UCk_Utils_DialogLine_UE::Get_Text(Line).ToString();
            Info.EventTag      = UCk_Utils_DialogLine_UE::Get_EventTag(Line);
            Info.LinkedEventTag       = UCk_Utils_DialogLine_UE::Get_LinkedEventTag(Line);
            Info.FilterTags    = UCk_Utils_DialogLine_UE::Get_FilterTags(Line);
            Info.NumConditions = UCk_Utils_DialogLine_UE::Get_NumConditions(Line);
            _Snapshot.Lines.Add(Info);
        }
    }

    // Emitters: read-only world walk.
    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_DialogEmitter_Params>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_DialogEmitter_Params&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            const auto Emitter = UCk_Utils_DialogEmitter_UE::Cast(Handle);
            if (ck::Is_NOT_Valid(Emitter))
            { return; }

            auto Info = FCkDialogDebugger_EmitterInfo{};
            Info.Handle      = Emitter;
            Info.DebugName   = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();
            Info.EmitterTags = UCk_Utils_DialogEmitter_UE::Get_EmitterTags(Emitter);

            for (const auto& CooledLine : UCk_Utils_DialogEmitter_UE::Get_ActiveCooldowns(Emitter))
            {
                auto Cd = FCkDialogDebugger_CooldownInfo{};
                Cd.Line   = CooledLine;
                Cd.LineID = ck::IsValid(CooledLine) ? UCk_Utils_DialogLine_UE::Get_LineID(CooledLine) : FName{};

                // Mode off the stored entry, not a magnitude heuristic on Remaining — a Forever cooldown reports
                // its sentinel expiry, which a "very large number" test would confuse with a long Timed one.
                const auto Entry     = UCk_Utils_DialogEmitter_UE::Get_CooldownEntry(Emitter, CooledLine);
                const auto Remaining = UCk_Utils_DialogEmitter_UE::Get_CooldownRemaining(Emitter, CooledLine);

                Cd.IsForever        = Entry.Get_DurationMode() == ECk_Dialog_CooldownDuration::Forever;
                Cd.TotalSeconds     = static_cast<float>(Entry.Get_Duration().Get_Seconds());
                Cd.RemainingSeconds = Cd.IsForever ? 0.0f : static_cast<float>(Remaining.Get_Seconds());

                Info.Cooldowns.Add(Cd);
            }

            if (Handle.Has<ck::FFragment_DialogEmitter_Debug>())
            {
                for (const auto& DebugEntry : Handle.Get<ck::FFragment_DialogEmitter_Debug>().Get_History())
                {
                    const auto& Result = DebugEntry.Get_Result();

                    auto HistoryEntry = FCkDialogDebugger_QueryHistoryEntry{};
                    HistoryEntry.EventTag         = Result.Get_EventTag();
                    HistoryEntry.TimestampSeconds = static_cast<float>(DebugEntry.Get_Timestamp().Get_Seconds());

                    for (const auto& Entry : Result.Get_Entries())
                    {
                        ++HistoryEntry.NumEntries;
                        switch (Entry.Get_Result())
                        {
                            case ECk_DialogLine_QueryResult::Passed:                ++HistoryEntry.NumPassed;       break;
                            case ECk_DialogLine_QueryResult::Fail_LineCondition:    ++HistoryEntry.NumFailLine;     break;
                            case ECk_DialogLine_QueryResult::Fail_EmitterCondition: ++HistoryEntry.NumFailEmitter;  break;
                            default: break;
                        }
                    }
                    Info.QueryHistory.Add(HistoryEntry);
                }
            }

            _Snapshot.Emitters.Add(Info);
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDialogDebugger_DataCollector::
    Reset()
    -> void
{
    _Snapshot = FCkDialogDebugger_RegistrySnapshot{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDialogDebugger_DataCollector::
    Get_Snapshot() const
    -> const FCkDialogDebugger_RegistrySnapshot&
{
    return _Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
