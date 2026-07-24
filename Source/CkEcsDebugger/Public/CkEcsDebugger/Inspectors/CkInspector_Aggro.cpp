#include "CkInspector_Aggro.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggroTarget_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SOverlay.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Aggro)

// =====================================================================================================================

namespace ck_inspector_aggro
{
    auto Get_TargetThreat(const FCk_Handle& InTarget) -> float
    {
        return (ck::IsValid(InTarget) && InTarget.Has<ck::FFragment_AggroTarget_Threat>())
            ? InTarget.Get<ck::FFragment_AggroTarget_Threat>().Get_Threat() : 0.0f;
    }

    auto Get_TargetScore(const FCk_Handle& InTarget) -> float
    {
        return (ck::IsValid(InTarget) && InTarget.Has<ck::FFragment_AggroTarget_Score>())
            ? InTarget.Get<ck::FFragment_AggroTarget_Score>().Get_Score() : 0.0f;
    }

    // A live bar filled RELATIVE to the strongest target of the same owner (so the top threat reads full) — the raw
    // threat clamp ceiling (default 10000) would make an absolute bar read empty. Standalone targets fall back to
    // their own clamp ceiling. The numeric value is overlaid on the bar.
    auto MakeValueBar(
        const FCk_Handle&                   InTarget,
        TFunction<float(const FCk_Handle&)> InValueGetter,
        const FLinearColor&                 InFillColor)
        -> TSharedRef<SWidget>
    {
        const auto MaxGetter = [InValueGetter](const FCk_Handle& InSelf) -> float
        {
            if (ck::Is_NOT_Valid(InSelf) || NOT InSelf.Has<ck::FFragment_AggroTarget_TargetInfo>())
            { return 0.0f; }

            auto Owner = InSelf.Get<ck::FFragment_AggroTarget_TargetInfo>().Get_AggroOwner();
            if (ck::IsValid(Owner) && Owner.Has<ck::FFragment_Aggro_TargetMap>())
            {
                auto Max = 0.0f;
                for (const auto& Pair : Owner.Get<ck::FFragment_Aggro_TargetMap>().Get_TargetsByTrackedEntity())
                {
                    if (ck::IsValid(Pair.Value))
                    { Max = FMath::Max(Max, InValueGetter(Pair.Value)); }
                }
                return Max;
            }

            return InSelf.Has<ck::FFragment_AggroTarget_ThreatParams>()
                ? static_cast<float>(InSelf.Get<ck::FFragment_AggroTarget_ThreatParams>().Get_ThreatClampRange().Get_Max())
                : 0.0f;
        };

        return SNew(SBox)
            .HeightOverride(16.0f)
            .MinDesiredWidth(140.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SProgressBar)
                    .Percent_Lambda([InTarget, InValueGetter, MaxGetter]() -> TOptional<float>
                    {
                        if (ck::Is_NOT_Valid(InTarget))
                        { return 0.0f; }
                        const auto Max = MaxGetter(InTarget);
                        return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(InValueGetter(InTarget) / Max, 0.0f, 1.0f) : 0.0f;
                    })
                    .FillColorAndOpacity(InFillColor)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([InTarget, InValueGetter]() -> FText
                    {
                        if (ck::Is_NOT_Valid(InTarget))
                        { return FText::FromString(TEXT("--")); }
                        return FText::FromString(FString::Printf(TEXT("%.1f"), InValueGetter(InTarget)));
                    })
                    .ColorAndOpacity(FSlateColor(FLinearColor::White))
                ]
            ];
    }
}

// =====================================================================================================================

auto FCkInspector_Aggro::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Aggro"));
}

auto FCkInspector_Aggro::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Aggro_Current,
        ck::FFragment_AggroTarget_Score>();
}

// =====================================================================================================================

auto FCkInspector_Aggro::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Aggro owner (this entity holds the threat table) ----
    if (Entity.Has<ck::FFragment_Aggro_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Owner")));

        const auto CapturedEntity = Entity;

        Builder.AddRow(
            FText::FromString(TEXT("Active Target:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Active = CapturedEntity.Get<ck::FFragment_Aggro_Current>().Get_ActiveTarget();
                if (ck::Is_NOT_Valid(Active))
                { return FText::FromString(TEXT("(None)")); }
                const auto Tracked = ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Active);
                return FText::FromString(ck::Format_UE(TEXT("[{}]"), Tracked));
            },
            CkStyle::Value_Handle());

        Builder.AddRow(
            FText::FromString(TEXT("Tracked Targets:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_TargetMap>())
                { return FText::FromString(TEXT("--")); }
                const auto Num = CapturedEntity.Get<ck::FFragment_Aggro_TargetMap>().Get_TargetsByTrackedEntity().Num();
                return FText::FromString(FString::Printf(TEXT("%d"), Num));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Enabled:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(CapturedEntity.Has<ck::FTag_Aggro_Disabled>() ? TEXT("No") : TEXT("Yes"));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Eval Count:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_EvaluationClock>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_Aggro_EvaluationClock>().Get_DebugEvaluationCount();
                return FText::FromString(FString::Printf(TEXT("%lld"), Count));
            },
            CkStyle::Value_Numeric());
    }

    // ---- Aggro target (this entity is one tracked target of an owner) ----
    if (Entity.Has<ck::FFragment_AggroTarget_Score>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Aggro Target")));

        const auto CapturedEntity = Entity;

        Builder.AddRow(
            FText::FromString(TEXT("Tracked Entity:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }
                const auto Tracked = ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(CapturedEntity);
                return FText::FromString(ck::Format_UE(TEXT("[{}]"), Tracked));
            },
            CkStyle::Value_Handle());

        // Threat + Score as live bars (filled relative to the owner's strongest target), value overlaid.
        Builder.AddWidgetRow(
            FText::FromString(TEXT("Threat:")),
            ck_inspector_aggro::MakeValueBar(Entity, &ck_inspector_aggro::Get_TargetThreat, FLinearColor(0.85f, 0.25f, 0.20f)));

        Builder.AddWidgetRow(
            FText::FromString(TEXT("Score:")),
            ck_inspector_aggro::MakeValueBar(Entity, &ck_inspector_aggro::Get_TargetScore, FLinearColor(0.25f, 0.55f, 0.90f)));

        Builder.AddRow(
            FText::FromString(TEXT("Distance:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroTarget_Score>())
                { return FText::FromString(TEXT("--")); }
                const auto Distance = CapturedEntity.Get<ck::FFragment_AggroTarget_Score>().Get_Distance();
                return FText::FromString(FString::Printf(TEXT("%.0f"), Distance));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }

                auto State = FString();
                if (CapturedEntity.Has<ck::FTag_AggroTarget_IsActive>())         { State += TEXT("Active "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_Perceived>())        { State += TEXT("Perceived "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_WithinRetention>())  { State += TEXT("InRetention "); }
                if (CapturedEntity.Has<ck::FTag_AggroTarget_PendingForget>())    { State += TEXT("PendingForget "); }

                return FText::FromString(State.IsEmpty() ? FString(TEXT("--")) : State.TrimEnd());
            },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Aggro::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
