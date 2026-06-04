#include "CkGoapDebugger/Data/CkGoapDebugger_HistoryModel.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_history_model
{
    auto Format_Timestamp(double InWorldSeconds) -> FString
    {
        const auto Total   = FMath::Max(0.0, InWorldSeconds);
        const auto Minutes = static_cast<int32>(Total) / 60;
        const auto Seconds = static_cast<int32>(Total) % 60;
        const auto Ms      = static_cast<int32>((Total - FMath::Floor(Total)) * 1000.0);
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
}
