#include "CkInspector_AStar.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAStar/CkAStar_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_AStar)

// =====================================================================================================================

namespace ck_inspector_astar
{
    // CostThresholdReached is a bounded stop, not a failure — the search ran out of budget the
    // caller set, so it warns rather than errors.
    static auto Get_SearchStatusTone(ECk_AStarSearchStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_AStarSearchStatus::InProgress:           return ECk_Tone::Info;
            case ECk_AStarSearchStatus::Complete:             return ECk_Tone::Ok;
            case ECk_AStarSearchStatus::Failed:               return ECk_Tone::Err;
            case ECk_AStarSearchStatus::CostThresholdReached: return ECk_Tone::Warn;
            default:                                          return ECk_Tone::Neutral;
        }
    }
}

// =====================================================================================================================

auto FCkInspector_AStar::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("A*"));
}

auto FCkInspector_AStar::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<ck::FFragment_AStar_Debug, ck::FFragment_AStar_Params>();
}

// =====================================================================================================================

auto FCkInspector_AStar::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Live search diagnostics ----
    if (Entity.Has<ck::FFragment_AStar_Debug>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Search")));

        const auto CapturedEntity = Entity;

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Status:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Status = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_astar::Get_SearchStatusTone(
                    CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus());
            }));

        Builder.AddRow(
            FText::FromString(TEXT("Open Set:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Size = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_OpenSetSize();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Size));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Closed Set:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Size = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_ClosedSetSize();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Size));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Iterations (frame):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Iters = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_IterationsThisFrame();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Iters));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Time (frame, us):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto TimeUs = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_TimeThisFrameMicroseconds();
                return FText::FromString(FString::Printf(TEXT("%lld"), TimeUs));
            },
            CkStyle::Value_Numeric());

        // The fragment stores 0..100 (CkAStar_Processor.h computes it as a percentage); the meter
        // wants a 0..1 fraction and clamps overrun itself.
        Builder.AddMeterRow(
            FText::FromString(TEXT("Budget Used:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return 0.0f; }
                return CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent() / 100.0f;
            }),
            // The tone IS the alarm: a query eating 80% of its frame budget is worth noticing before
            // it starts spilling across frames at 100%.
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return ECk_Tone::Info; }

                const auto Fraction = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent() / 100.0f;

                if (Fraction >= 1.0f) { return ECk_Tone::Err; }
                if (Fraction >= 0.8f) { return ECk_Tone::Warn; }

                return ECk_Tone::Info;
            }),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Pct = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent();
                return FText::FromString(FString::Printf(TEXT("%.1f%%"), Pct));
            }));
    }

    // ---- Configuration ----
    if (Entity.Has<ck::FFragment_AStar_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Params")));

        const auto& Params        = Entity.Get<ck::FFragment_AStar_Params>();
        const auto  BudgetUs      = Params.Get_BudgetMicroseconds();
        const auto  MaxIterations = Params.Get_MaxIterationsPerTick();
        const auto  CostThreshold = Params.Get_CostThreshold();

        Builder.AddRow(
            FText::FromString(TEXT("Budget (us):")),
            [BudgetUs](const FCk_Handle&)
            {
                return FText::FromString(BudgetUs == 0
                    ? FString(TEXT("0 (unbounded)"))
                    : FString::Printf(TEXT("%lld"), BudgetUs));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Max Iterations:")),
            [MaxIterations](const FCk_Handle&)
            {
                return FText::FromString(MaxIterations == 0
                    ? FString(TEXT("0 (unbounded)"))
                    : ck::Format_UE(TEXT("{}"), MaxIterations));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Cost Threshold:")),
            [CostThreshold](const FCk_Handle&)
            {
                return FText::FromString(CostThreshold > 0.0f
                    ? FString::Printf(TEXT("%.3f"), CostThreshold)
                    : FString(TEXT("0 (disabled)")));
            },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_AStar::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
