#include "CkGoapDebugger/Data/CkGoapDebugger_HistoryModel.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_history_model
{
    auto Format_Timestamp(double InWorldSeconds) -> FString
    {
        // Round total milliseconds first — truncating the fractional part
        // renders 34.538 (stored as 34.53799…) as ".537", and rounding only
        // the fraction would produce ".1000" at the rollover edge.
        const auto TotalMs = FMath::RoundToInt64(FMath::Max(0.0, InWorldSeconds) * 1000.0);
        const auto Minutes = static_cast<int32>(TotalMs / 60000);
        const auto Seconds = static_cast<int32>((TotalMs / 1000) % 60);
        const auto Ms      = static_cast<int32>(TotalMs % 1000);
        return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Seconds, Ms);
    }

    auto KindTag(ECkGoapDebugger_HistoryEventKind InKind) -> FString
    {
        switch (InKind)
        {
            case ECkGoapDebugger_HistoryEventKind::ActionActivated:   return TEXT("ACT");
            case ECkGoapDebugger_HistoryEventKind::ActionDeactivated: return TEXT("DEACT");
            case ECkGoapDebugger_HistoryEventKind::PlanFound:         return TEXT("PLAN");
            case ECkGoapDebugger_HistoryEventKind::PlanFailed:        return TEXT("FAIL");
            case ECkGoapDebugger_HistoryEventKind::ChainReset:        return TEXT("RESET");
            case ECkGoapDebugger_HistoryEventKind::ActionSetEnabled:  return TEXT("ON");
            case ECkGoapDebugger_HistoryEventKind::ActionSetDisabled: return TEXT("OFF");
            case ECkGoapDebugger_HistoryEventKind::ChainActivated:    return TEXT("CHAIN");
            case ECkGoapDebugger_HistoryEventKind::WorldStateChanged: return TEXT("WS");
            case ECkGoapDebugger_HistoryEventKind::Replanned:         return TEXT("REPLAN");
        }
        return TEXT("?");
    }

    auto SerializeHistory(
        const FString& InHeaderLine,
        const TArray<FCkGoapDebugger_HistoryEvent>& InEvents,
        const TFunctionRef<FString(const FCk_Handle_Goap_Planner&)> InPlannerName) -> FString
    {
        auto Out = InHeaderLine + LINE_TERMINATOR;
        for (const auto& Ev : InEvents)
        {
            const auto Planner = InPlannerName(Ev.ActionSetHandle);
            auto Line = FString::Printf(TEXT("  %-10s  %-20s  %-6s  %s"),
                *Format_Timestamp(Ev.WorldTimeSeconds), *Planner, *KindTag(Ev.Kind), *Ev.ActionClassName);
            if (NOT Ev.Meta.IsEmpty())
            { Line += FString::Printf(TEXT("  %s"), *Ev.Meta); }
            Out += Line + LINE_TERMINATOR;
        }
        return Out;
    }

    static auto Is_FlapKind(ECkGoapDebugger_HistoryEventKind InKind) -> bool
    {
        return InKind == ECkGoapDebugger_HistoryEventKind::ActionActivated
            || InKind == ECkGoapDebugger_HistoryEventKind::ActionDeactivated;
    }

    auto BuildPlannerGroups(
        const TArray<FCkGoapDebugger_HistoryEvent>& InEvents,
        const TFunctionRef<FString(const FCk_Handle_Goap_Planner&)> InPlannerName)
        -> TArray<FCkGoapDebugger_PlannerGroup>
    {
        auto Groups = TArray<FCkGoapDebugger_PlannerGroup>{};
        auto IndexOf = TMap<FCk_Handle_Goap_Planner, int32>{};

        // 1) Partition into per-planner ordered event lists (first-seen order preserved).
        auto PerPlanner = TArray<TArray<FCkGoapDebugger_HistoryEvent>>{};
        for (const auto& Ev : InEvents)
        {
            auto* Found = IndexOf.Find(Ev.ActionSetHandle);
            auto Idx = (Found != nullptr) ? *Found : INDEX_NONE;
            if (Idx == INDEX_NONE)
            {
                Idx = Groups.Num();
                IndexOf.Add(Ev.ActionSetHandle, Idx);
                auto Group = FCkGoapDebugger_PlannerGroup{};
                Group.Planner = Ev.ActionSetHandle;
                Group.PlannerName = InPlannerName(Ev.ActionSetHandle);
                Groups.Add(MoveTemp(Group));
                PerPlanner.AddDefaulted();
            }
            PerPlanner[Idx].Add(Ev);
        }

        // 2) Run-length-collapse flap runs inside each planner.
        for (auto g = 0; g < Groups.Num(); ++g)
        {
            const auto& Evs = PerPlanner[g];
            auto& Rows = Groups[g].Rows;

            auto i = 0;
            while (i < Evs.Num())
            {
                auto j = i;
                auto Names = TArray<FString>{};
                while (j < Evs.Num() && Is_FlapKind(Evs[j].Kind))
                {
                    const auto& Nm = Evs[j].ActionClassName;
                    if (NOT Nm.IsEmpty() && NOT Names.Contains(Nm))
                    {
                        if (Names.Num() == 2) { break; }   // a 3rd distinct action ends the run
                        Names.Add(Nm);
                    }
                    ++j;
                }

                const auto RunLen = j - i;
                if (RunLen >= k_FlapMinRun && Names.Num() == 2)
                {
                    auto Row = FCkGoapDebugger_HistoryRow{};
                    Row.IsFlap = true;
                    Row.Planner = Groups[g].Planner;
                    Row.FlapActionA = Names[0];
                    Row.FlapActionB = Names[1];
                    Row.FlapCount = RunLen;
                    Row.FlapTStart = Evs[i].WorldTimeSeconds;
                    Row.FlapTEnd   = Evs[j - 1].WorldTimeSeconds;
                    for (auto k = i; k < j; ++k)
                    {
                        Row.FlapTStart = FMath::Min(Row.FlapTStart, Evs[k].WorldTimeSeconds);
                        Row.FlapTEnd   = FMath::Max(Row.FlapTEnd,   Evs[k].WorldTimeSeconds);
                        Row.RawEvents.Add(Evs[k]);
                    }
                    Rows.Add(MoveTemp(Row));
                    i = j;
                }
                else
                {
                    auto Row = FCkGoapDebugger_HistoryRow{};
                    Row.IsFlap = false;
                    Row.Planner = Groups[g].Planner;
                    Row.Event = Evs[i];
                    Row.RawEvents.Add(Evs[i]);
                    Rows.Add(MoveTemp(Row));
                    ++i;
                }
            }
        }

        return Groups;
    }
}
