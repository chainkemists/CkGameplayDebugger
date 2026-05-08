#include "CkInspector_AStar.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAStar/CkAStar_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_AStar)

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

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Status:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Status = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_SearchStatus();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return CkDebugStyle::None(); }
                return CkDebugStyle::Value_Enum();
            });

        Builder.AddRow(
            FText::FromString(TEXT("Open Set:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Size = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_OpenSetSize();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Size));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Closed Set:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Size = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_ClosedSetSize();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Size));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Iterations (frame):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Iters = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_IterationsThisFrame();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Iters));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Time (frame, us):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto TimeUs = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_TimeThisFrameMicroseconds();
                return FText::FromString(FString::Printf(TEXT("%lld"), TimeUs));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Budget Used:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AStar_Debug>())
                { return FText::FromString(TEXT("--")); }
                const auto Pct = CapturedEntity.Get<ck::FFragment_AStar_Debug>().Get_BudgetUsagePercent();
                return FText::FromString(FString::Printf(TEXT("%.1f%%"), Pct));
            },
            CkDebugStyle::Value_Numeric());
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
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Max Iterations:")),
            [MaxIterations](const FCk_Handle&)
            {
                return FText::FromString(MaxIterations == 0
                    ? FString(TEXT("0 (unbounded)"))
                    : ck::Format_UE(TEXT("{}"), MaxIterations));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Cost Threshold:")),
            [CostThreshold](const FCk_Handle&)
            {
                return FText::FromString(CostThreshold > 0.0f
                    ? FString::Printf(TEXT("%.3f"), CostThreshold)
                    : FString(TEXT("0 (disabled)")));
            },
            CkDebugStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_AStar::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
