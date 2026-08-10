#include "CkInspector_Aggro.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAggro/CkAggro_Fragment.h"
#include "CkAggro/CkAggroTarget_Fragment.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

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

    // The meter fills RELATIVE to the strongest target of the same owner (so the top threat reads full) — the raw
    // threat clamp ceiling (default 10000) would make an absolute bar read empty. Standalone targets fall back to
    // their own clamp ceiling.
    auto Get_PeerMax(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> float
    {
        if (ck::Is_NOT_Valid(InTarget) || NOT InTarget.Has<ck::FFragment_AggroTarget_TargetInfo>())
        { return 0.0f; }

        auto Owner = InTarget.Get<ck::FFragment_AggroTarget_TargetInfo>().Get_AggroOwner();

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

        return InTarget.Has<ck::FFragment_AggroTarget_ThreatParams>()
            ? static_cast<float>(InTarget.Get<ck::FFragment_AggroTarget_ThreatParams>().Get_ThreatClampRange().Get_Max())
            : 0.0f;
    }

    auto Get_PeerFraction(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> float
    {
        if (ck::Is_NOT_Valid(InTarget))
        { return 0.0f; }

        const auto Max = Get_PeerMax(InTarget, InValueGetter);

        return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(InValueGetter(InTarget) / Max, 0.0f, 1.0f) : 0.0f;
    }

    auto Format_Value(
        const FCk_Handle&                          InTarget,
        float (*InValueGetter)(const FCk_Handle&))
        -> FText
    {
        if (ck::Is_NOT_Valid(InTarget))
        { return FText::FromString(TEXT("--")); }

        return FText::FromString(ck::Format_UE(TEXT("{:.1f}"), InValueGetter(InTarget)));
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

        // The top target is an entity, so it renders as the same clickable pill every other entity surface uses —
        // the pill reads "None" on its own when there is no active target.
        Builder.AddWidgetRow(
            FText::FromString(TEXT("Active Target:")),
            SNew(SCkDebug_EntityRef)
                .Entity_Lambda([CapturedEntity]() -> FCk_Handle
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Aggro_Current>())
                    { return {}; }

                    const auto Active = CapturedEntity.Get<ck::FFragment_Aggro_Current>().Get_ActiveTarget();

                    if (ck::Is_NOT_Valid(Active))
                    { return {}; }

                    return ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(Active);
                })
                .ShowName(true));

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

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Enabled:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]() -> FText
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(CapturedEntity.Has<ck::FTag_Aggro_Disabled>() ? TEXT("Disabled") : TEXT("Enabled"));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]() -> ECk_Tone
            {
                if (ck::Is_NOT_Valid(CapturedEntity))
                { return ECk_Tone::Neutral; }
                return CapturedEntity.Has<ck::FTag_Aggro_Disabled>() ? ECk_Tone::Err : ECk_Tone::Ok;
            }));

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

        Builder.AddWidgetRow(
            FText::FromString(TEXT("Tracked Entity:")),
            SNew(SCkDebug_EntityRef)
                .Entity_Lambda([CapturedEntity]() -> FCk_Handle
                {
                    if (ck::Is_NOT_Valid(CapturedEntity))
                    { return {}; }
                    return ck::UAggroTarget_TrackedEntity_Utils::Get_StoredEntity(CapturedEntity);
                })
                .ShowName(true));

        // Threat + Score as live meters, each filled relative to the owner's strongest target. Threat reads Err
        // (the hostility axis), Score reads Info (the derived ranking) — the two tones the hand-rolled bars
        // approximated with literal red / blue fills.
        Builder.AddMeterRow(
            FText::FromString(TEXT("Threat:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Get_PeerFraction(CapturedEntity, &ck_inspector_aggro::Get_TargetThreat); }),
            ECk_Tone::Err,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Format_Value(CapturedEntity, &ck_inspector_aggro::Get_TargetThreat); }));

        Builder.AddMeterRow(
            FText::FromString(TEXT("Score:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Get_PeerFraction(CapturedEntity, &ck_inspector_aggro::Get_TargetScore); }),
            ECk_Tone::Info,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            { return ck_inspector_aggro::Format_Value(CapturedEntity, &ck_inspector_aggro::Get_TargetScore); }));

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
            // Left as live text, not chips: these four tags flip continuously (Perceived / retention), and a chips
            // row is a compose-time snapshot that would need a RequestRebuild per flip to stay honest.
            CkStyle::Value_Enum());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Aggro::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
