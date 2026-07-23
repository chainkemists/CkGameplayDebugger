#include "CkInspector_Aggro.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggroTarget_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Aggro)

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

        Builder.AddRow(
            FText::FromString(TEXT("Threat:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroTarget_Threat>())
                { return FText::FromString(TEXT("--")); }
                const auto Threat = CapturedEntity.Get<ck::FFragment_AggroTarget_Threat>().Get_Threat();
                return FText::FromString(FString::Printf(TEXT("%.2f"), Threat));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Score:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AggroTarget_Score>())
                { return FText::FromString(TEXT("--")); }
                const auto Score = CapturedEntity.Get<ck::FFragment_AggroTarget_Score>().Get_Score();
                return FText::FromString(FString::Printf(TEXT("%.2f"), Score));
            },
            CkStyle::Value_Numeric());

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
